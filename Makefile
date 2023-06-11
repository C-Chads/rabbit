#Rabbit, a small, lightwight, fast, and customizable web server.
#Copyright (C) 2023 kevidryon2
#
#This program is free software: you can redistribute it and/or modify
#it under the terms of the GNU Affero General Public License as
#published by the Free Software Foundation, either version 3 of the
#License, or (at your option) any later version.
#
#This program is distributed in the hope that it will be useful,
#but WITHOUT ANY WARRANTY; without even the implied warranty of
#MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#GNU Affero General Public License for more details.
#
#You should have received a copy of the GNU Affero General Public License
#along with this program.  If not, see <https://www.gnu.org/licenses/>.

everything: clean x86-arch arm-arch aarch64 riscv-arch

all: clean x86-arch
static: clean x86-arch-static

arm-dynamic: clean arm-arch-dynamic
arm: clean arm-arch

arm64-dynamic: clean aarch64-dynamic
arm64: clean aarch64

riscv-dynamic: clean riscv-arch-dynamic
riscv: clean riscv-arch


clean:
	rm -f build/*
x86-arch:
	make -f Makefile.x86
x86-arch-static:
	make -f Makefile.x86 static
arm-arch:
	make -f Makefile.arm
arm-arch-dynamic:
	make -f Makefile.arm dynamic
aarch64:
	make -f Makefile.aarch64
aarch64-dynamic:
	make -f Makefile.aarch64 dynamic
riscv-arch:
	make -f Makefile.riscv
riscv-arch-dynamic:
	make -f Makefile.riscv dynamic
