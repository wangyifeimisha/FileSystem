

# Build details

_CXX                    = g++
_CXXFLAGS               = -W -Wall -g

# Compile to objects

%.o: %.cpp
	$(_CXX) $(_CXXFLAGS) -c -o $@ $<

# Build Executable

.PHONY: all
all: FS

# executable 1
_exe1 = FS
_objects1 = main.o FS.o disk.o

FS: $(_objects1)
	$(_CXX) $(_CXXFLAGS) -o $(_exe1) $(_objects1)

# Dependencies

FS.o: FS.h disk.h
main.o: FS.h disk.h
disk.o: disk.h

# Clean up

.PHONY: clean
clean:
	rm -f "$(_exe1)" $(_objects1)
