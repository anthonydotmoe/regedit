# This makefile is for building the program for Windows CE on Arm.

I=./inc
S=./src

CC	=	arm-mingw32ce-gcc
WINDRES	=	arm-mingw32ce-windres

CFLAGS = 	-D_WIN32_IE=0x0500 -DUNICODE -D_UNICODE -Iinc -lcommctrl

build:
	docker run --env DOCKER --rm -v"$(shell pwd)":/build ghcr.io/enlyze/windows-ce-build-environment-arm:latest /bin/sh -c 'cd /build; make all'

all: regedit.exe

regedit.exe: regedit.o regnode.o regedit.rsc
	$(CC) -o $@ regedit.o regnode.o regedit.rsc $(CFLAGS)

regedit.rsc: $S/regedit.rc $I/regedit.h
	$(WINDRES) -Iinc $S/regedit.rc $@

%.o: $S/%.c
	$(CC) $(CFLAGS) $< -c

upload: all
	cp regedit.exe /run/media/user/TFAT/APP/Navigator.exe
