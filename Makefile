cc		= cl
link	= link

I = .\inc
S = .\src
O = .\out

cflags = $(cflags) /I $I /DUNICODE /D_UNICODE
rcflags = $(rcflags) /I $I

Objs = $O\regedit.obj $O\regtree.obj

all: regedit.exe

$O\regedit.res: $S\regedit.rc $I\regedit.h
	rc $(rcflags) -r -fo $O\regedit.res $S\regedit.rc

{$S}.c{$O}.obj:
	$(cc) $(cflags) -Fo$O\ -c $<

regedit.exe: $(Objs) $O\regedit.res
	$(link) $(ldebug) $(guilflags) -out:regedit.exe $(Objs) $O\regedit.res $(guilibs) advapi32.lib

clean:
	del $O\*.obj

run:
	./regedit.exe