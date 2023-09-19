# Rabbit
A really small and fast (like a rabbit) nomad web server written in C, that can be installed in any directory and transferred anywhere on the file system.

## Getting Started
First download the source code and compile it.

```bash
git clone https://github.com/kevidryon2/rabbit.git
cd rabbit
mkdir build
make
```

Since Rabbit only consists of less than 1K lines of code, it is really fast to compile; by default, it will compile for the x86 architecture, but if you want to compile for another architecture (ARM32, ARM64, or Risc-V) just specify one of the following arguments to make: `arm`, `arm64`, `riscv`.

If you want to compile multiple architectures at once, choose between the following arguments:
`x86-arch`, `arm-arch`, `aarch64`, `riscv-arch`.

There are a few flags you can define to alter the compilation of Rabbit that you can put in CFLAGS:

- `-DDISABLE_CACHE`: Disable caching; this is particularly useful for web developers who want to test their website using Rabbit without having to constantly clear the cache.
- `-DNO-REDIRECT-ROOT`: Disable redirecting `/` to `/index.html`.
- `-DREDIRECT-ROOT-PHP`: Redirect `/` to `/index.php`.
- `-DERRORS-PHP`: Set the default error pages to `/(error).php` (For example, `/404.php` or `/500.php`)

-

Then, create Rabbit's directory structure and copy Rabbit to it; in this example, we'll be using the `/srv` directory, but you can use any directory you want.

```bash
mkdir --parents /srv/public /srv/cache /srv/scripts /srv/bin
cp build/rabbit /src/bin
```

-

Lastly, start Rabbit.

```
cd /srv/bin
export RABBIT_PATH=..
./rabbit
Rabbit Beveren 1 (PID = <some random number>)
Using port 8080

Creating socket... OK
Binding... OK
Listening... OK

Using directory /srv/

Loading scripts...
Loaded Rabbit. Accepting requests.
```
