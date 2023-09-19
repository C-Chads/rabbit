/*
Rabbit, a small, lightwight, fast, and customizable web server.
Copyright (C) 2023 kevidryon2

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU Affero General Public License as
published by the Free Software Foundation, either version 3 of the
License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Affero General Public License for more details.

You should have received a copy of the GNU Affero General Public License
along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <time.h>
#include <netinet/ip.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <dirent.h>
#include <errno.h>
#include <stdbool.h>
#include <signal.h>
#include "hirolib.h"
#include "bns.h"
#include "server.h"
#include "c-stacktrace.h"

extern char *verbs[];
extern LoadedScript *scripts;
extern int nloadedscripts;

void sigpipe() {
	SetColor16(COLOR_RED);
	printf("EPIPE");
	ResetColor16();
}

void segfault() {
	printf("\nWhoops! Rabbit crashed.\n\
Please sent all this information in an issue to the Rabbit Github repo \
( https://github.com/kevidryon2/rabbit ):\n");
	CRITICAL("Recieved signal 11 (SIGSEGV)!");
}

int filesize(FILE *fp) {
	int os = ftell(fp);
	fseek(fp, 0, SEEK_END);
	int s = ftell(fp);
	fseek(fp, os, SEEK_SET);
	return s;
}

void logdata(char *data) {
	for (int i=0; i<strlen(data); i++) {
		if (data[i] < ' ' || data[i] > '~') {
			printf("\\%sx%02x", "", (unsigned char)(data[i]));
		} else putchar(data[i]);
	}
}


int needle(char *n, char **h, int lh);
char *compile(char *script);
void exec(char *binscript);
int search_begin(char **restrict array, int num_elements, char *restrict string);
int startswith(char *s, char *c);
int endswith(char *restrict s, char *restrict end);
char *combine(char *restrict a, char *restrict b);
char *ntoken(char *const s, char *d, int t);

int RabbitInit(unsigned short port);
LoadedScript *RabbitLoadScript(char *data, int len);
loadFile_returnData RabbitLoadFile(char *pubpath, char *cachepath, int csock);
RequestData *RabbitParseRequest(const char *reqbuff);
void RabbitExecScript(LoadedScript script, RequestData reqdata, char *resbuff);
int RabbitSearchScript(char *path, int pathlen);
int RabbitCallPHP(char *source_path, char *output_path, RequestData data, loadFile_returnData *output);

char *escapestr(unsigned char *s) {
	unsigned char *o = malloc(BUFSIZ);

	memset(o, 0, BUFSIZ);
	
	int j;

	int oi = 0;
	for (int i=0; i<strlen(s); i++) {
		for (j=i; s[j] &&
			 	  s[j] != '/' &&
			 	  s[j] < 0x80 &&
			 	  s[j] > 0x1f; j++);
		j -= i;
		if (j > 0) {
			if (j > 25) j = 25;
			o[oi] = 'A'+j; oi++;
			memcpy(o+oi, s+i, j); oi += j, i += j;
			i--;
		} else {
			o[oi] = (s[i]>>4)+'a'; oi++;
			o[oi] = (s[i]%16)+'a'; oi++;
		}
	}
	
	return o;
}

void set_env_variable() {
	printf("You should set the RABBIT_PATH environment variable to the path you want to use as the server main directory.\n");
	exit(1);
}

int main(int argc, char **argv, char **envp) {
	char *buffer;
	char scriptpath[PATH_MAX];
	struct dirent *ent;
	FILE *fp;
	int len;
	int sn = 0;
	unsigned short port;
	char *tmp;
	
	printf("Rabbit "RABBIT_VERS" (PID = %d)\n", getpid());
	
	init_exceptions(argv[0]);

	signal(SIGPIPE, sigpipe);
	signal(SIGSEGV, segfault);
	
	/* Seed RNG */
	srand(time(NULL));
	
	if (argc<2) {
		port = 8080;
	} else {
		port = atoi(argv[1]);
	}

	printf("Using port %d\n\n", port);
	
	int serversock = RabbitInit(port);
	char rootpath[PATH_MAX];
	char cwdbuffer[PATH_MAX];
	char *fullpath;
	loadFile_returnData read_data;
	
	/* Get server path */
	getcwd(cwdbuffer, PATH_MAX);
	strncpy(rootpath, cwdbuffer, PATH_MAX);
	if (!getenv("RABBIT_PATH")) set_env_variable();
	if (!(fullpath = realpath(getenv("RABBIT_PATH"), NULL))) {
		perror(getenv("RABBIT_PATH"));
		return 127;
	}
	
	strncpy(rootpath, fullpath, PATH_MAX);
	strncat(rootpath, "/", PATH_MAX);
	
	printf("Using directory %s\n", rootpath);
	
	printf("\nLoading scripts...\n");
	
	snprintf(scriptpath, PATH_MAX, "%s/scripts/", rootpath);
	
	DIR *dp = opendir(scriptpath);
	if (!dp) {
		printf("can't open script directory :( (Errno %d)\n", errno);
		printf("You should create the 'scripts', 'public', 'cache' (the latter as a ramdisk) directories in the main server folder.\n");
	}
	
	scripts = malloc(sizeof(LoadedScript));
	nloadedscripts=0;
	
	while ((ent = readdir(dp))) {
		if (ent->d_name[0] != '.') {
			if (endswith(ent->d_name, ".bns")) {
				
				/*Open file*/
				buffer = combine(scriptpath, ent->d_name);
				printf("Loading file %s... ", ent->d_name);
				fp = fopen(buffer, "r");
				
				if (!fp) {
					printf("can't open :( (Error %d)\n", errno);
					return 1;
				}
				
				/* Allocate script */
				nloadedscripts++;
				scripts = realloc(scripts, nloadedscripts*sizeof(LoadedScript));
				
				/* Read file */
				free(buffer);
				fseek(fp, 0, SEEK_END);
				len = ftell(fp);
				buffer = malloc(len);
				
				fseek(fp, 0, SEEK_SET);
				fread(buffer, 1, len, fp);
				
				/* Load file */
				tmp = RabbitLoadScript(buffer, len);
				if (!tmp) {
					printf("can't load :( (Error %d)\n", errno);
					return 1;
				}
				
				scripts[sn] = *(LoadedScript*)tmp;
				
				printf("OK (Script %d)\n", sn);
				
				sn++;
				fclose(fp);
			}
		}
	};
	closedir(dp);
	
	printf("Loaded Rabbit. Accepting requests.\n\n");
	
	int csock;
	int statcode;
	int script;
	int bodylen;
	
	struct sockaddr_in caddr;
	socklen_t caddrl;
	char reqbuff[BUFSIZ];
	char *resbuff = malloc(BUFSIZ);
	
	if (!resbuff) {
		perror("malloc");
		return -1;
	}
	
	RequestData *reqdata;
	
	FILE *tmpfp;
	
	FILE *publicfp;
	FILE *cachedfp;
	
	char public_path[PATH_MAX];
	char cached_path[PATH_MAX];
	char phpoutput_path[PATH_MAX];
	char *line;
	char *body;
	
	while (true) {
		/* Accept */
		csock = accept(serversock, &caddr, &caddrl);
		printf("Recieved Request (Address %d.%d.%d.%d, Port %d) ",
			   caddr.sin_addr.s_addr%256,
			   (caddr.sin_addr.s_addr>>8)%256,
			   (caddr.sin_addr.s_addr>>16)%256,
			   (caddr.sin_addr.s_addr>>24)%256,
			   be16toh(caddr.sin_port)
		);
		
		/* Clear buffers */
		memset(resbuff, 0, BUFSIZ);
		memset(reqbuff, 0, BUFSIZ);
		memset(&reqdata, 0, sizeof(reqdata));
		
		/* Read request */
		putchar('I');
		read(csock, reqbuff, BUFSIZ);
		
		/* Parse request */
		putchar('P');
		
		if (!(reqdata = RabbitParseRequest(reqbuff))) {
			switch (errno) {
				//using HTTP/0.9
				case 1:
				case 2:
					SetColor16(COLOR_RED);
					printf("H");
					ResetColor16();
					printf(" (Data: ");
					logdata(reqbuff);
					printf(")");
					sprintf(resbuff, "505 HTTP Version Not Supported\nServer: Rabbit/"RABBIT_VERS"\n\nHTTP Version not supported.");
					write(csock, resbuff, strlen(resbuff));
					goto endreq;
				
				//Invalid verb
				case 3:
					SetColor16(COLOR_RED);
					printf("V (Data: ");
					logdata(reqbuff);
					printf(")");
					ResetColor16();
					RabbitErrorHandler(501, resbuff, reqdata);
					write(csock, resbuff, strlen(resbuff));
					goto endreq;
			}
		}
		
		reqdata->truepath = ntoken(reqdata->path, "?", 0);
		
		putchar('H');

		/* Search for a script to handle the request */
		script = RabbitSearchScript(reqdata->truepath, strlen(reqdata->truepath));
		if (script > -1) goto script;
		
		/* TODO: Parse headers */
		
		goto noscript;
script:
		if (reqdata->verb == VERB_OPTIONS) {
			printf("Ts");
			/* TODO: Return verbs supported by script */
		}
		printf("S%d", script);
		RabbitExecScript(scripts[script], *reqdata, resbuff);
		goto endreq;
		
noscript:
		/* If verb is OPTIONS return allowed options (GET, OPTIONS, HEAD) */
		if (reqdata->verb == VERB_OPTIONS) {
			printf("T");
			snprintf(resbuff, BUFSIZ, "HTTP/1.0 200 OK\r\nServer: Rabbit/"RABBIT_VERS"\r\nAllow: OPTIONS, GET, HEAD\r\n");
			write(csock, resbuff, strlen(resbuff));
			goto endreq;
			
		}
		
		/* Fetch file */
		putchar('F');
		memset(public_path, 0, PATH_MAX);
		snprintf(public_path, sizeof public_path, "%s/public/%s", rootpath, reqdata->truepath);
		
		/* If file doesn't exist in public directory return 404 Not Found */
		if (!(publicfp = fopen(public_path, "r"))) {
			SetColor16(COLOR_RED);
			printf("%d %s ", errno, reqdata->path);
			ResetColor16();
			if (errno == 2) {
				snprintf(resbuff, BUFSIZ, "HTTP/1.0 404 Not Found\nServer: Rabbit/"RABBIT_VERS"\n\nError: File %s not found.\n", reqdata->truepath);
			} else {
				snprintf(resbuff, BUFSIZ, "HTTP/1.0 500 Internal Server Error\nServer: Rabbit/"RABBIT_VERS"\n\nError: Recieved errno %d while trying to read file %s.\n", errno, reqdata->truepath);
			}
			write(csock, resbuff, strlen(resbuff));
			goto endreq;
		}
		
		fclose(publicfp);
		
		/* File exists */
		printf(" %s ", reqdata->truepath);
		memset(cached_path, 0, PATH_MAX);
		snprintf(cached_path, sizeof cached_path, "%s/cache/%s", rootpath, tmp = escapestr(reqdata->truepath));
		free(tmp);
		snprintf(phpoutput_path, sizeof cached_path, "%s/cache/%s.html", rootpath, tmp = escapestr(reqdata->truepath));


		read_data = RabbitLoadFile(public_path, cached_path, csock);
		
		if (endswith(reqdata->truepath, ".php")) {
			free(read_data.data);
#ifdef DISABLE_CACHE
			if (RabbitCallPHP(public_path, phpoutput_path, *reqdata, &read_data)) {
#else
			if (RabbitCallPHP(cached_path, phpoutput_path, *reqdata, &read_data)) {
#endif
				goto response;
			} else {
				snprintf(resbuff, BUFSIZ, "HTTP/1.0 500 Internal Server Error\nServer: Rabbit/"RABBIT_VERS"\n\nCan't compile PHP file");
				goto endreq;
			}
		}

response:
		/* Send response */
		putchar('O');
		
		sprintf(resbuff,"HTTP/1.0 200 OK\r\nServer: Rabbit/"RABBIT_VERS"\r\n\r\n");
		write(csock, resbuff, strlen(resbuff));
		
		/* Finish and flush */
		fflush(stdout);

		write(csock, read_data.data, read_data.datalen);
		free(read_data.data);
		free(reqdata->truepath);
		free(reqdata);
		
endreq:
		/* Finish and flush */
		putchar('\n');
		fflush(stdout);
		close(csock);
	}
}
