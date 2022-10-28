CPP      = cl
CPPFLAGS = /EHsc /std:c++20
SOURCES  = main.cpp schurmalloc.cpp schurmallocTest.cpp
OBJS     = $(SOURCES:.cpp=.obj)

all: schurmalloc.exe

schurmalloc.exe: $(OBJS)
	$(CPP) $(CPPFLAGS) $(OBJS) /link /out:schurmalloc.exe

main.obj: schurmalloc.h
schurmalloc.obj: schurmalloc.h
schurmallocTest.obj: schurmalloc.h

clean:
	del schurmalloc.exe *.obj
