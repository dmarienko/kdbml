#
# Before run make setup your Matlab location
#
# MATLAB = your_matlab_path
MATLAB = /usr/local/MATLAB/
CC = gcc
LD = gcc
UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S), Linux)
    OS = glnxa64
    EXT = mexa64
else
    $(error Your platfrom is not supported by this makefile.)
endif
CFLAGS = -I ${MATLAB}/extern/include -I ${MATLAB}/simulink/include -std=c99 -D_GNU_SOURCE -DMATLAB_MEX_FILE -std=c99 -D_GNU_SOURCE -fexceptions -fPIC -fno-omit-frame-pointer -pthread -DMX_COMPAT_32 -O -DNDEBUG
LDFLAGS = -O -pthread -shared -Wl,--version-script,${MATLAB}/extern/lib/${OS}/mexFunction.map -Wl,--no-undefined -Wl,-rpath-link,${MATLAB}/bin/${OS} -L${MATLAB}/bin/${OS}
LIBS = -lmx -lmex -lmat -lm -lstdc++

all: clean libs

libs: qdbc.${EXT}

%.o: %c
	${CC} ${CFLAGS} -c -o$@ $<

%.${EXT}:%.o
	${LD} ${LDFLAGS} -o$@ $< kx/c.o ${LIBS}

clean:
	@rm -f *.o *.${EXT}
