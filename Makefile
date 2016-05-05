# Modified By: Felipe Gutierrez, June 2015

PREFIX=fscheck
LIBS=

GCC=gcc
GPP=g++
# CUDA
OPTS=-O -Wall -o

EXECUTABLES=$(PREFIX)

all:
	make $(EXECUTABLES)
	./fscheck ../P5/xv6/fs.img

# EXECUTABLES
$(PREFIX): $(PREFIX).c
	$(GCC) $(OPTS) $(PREFIX) $(PREFIX).c $(LIBS)


# CLEAN
clean:
	rm -f $(EXECUTABLES)

