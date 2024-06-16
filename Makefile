# This makefile is for building the program for Windows CE on Arm.

I=./inc
S=./src

CC	=	arm-mingw32ce-gcc
WINDRES	=	arm-mingw32ce-windres

CFLAGS = 	-D_WIN32_IE=0x0500 -DUNICODE -D_UNICODE -Iinc -lcomctrl32 -ladvapi32

build:
	docker run --env DOCKER --rm -v"$(shell pwd)":/build ghcr.io/enlyze/windows-ce-build-environment-arm:latest /bin/sh -c 'cd /build; make all'

all: regedit.exe

regedit.exe: regedit.o regnode.o regedit.res

regedit.res: $S/regedit.rc $I/regedit.h
	${WINDRES} $? $@

%.o: $S/%.c
	$(CC) $(CFLAGS) $< -c
