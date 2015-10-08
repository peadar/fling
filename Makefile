CXXFLAGS = -g -std=c++0x -Wall

all:fling dlab

FLING_OBJS += fling.o common.o readme.o
DLAB_OBJS += dlab.o common.o
EXTRA_CLEAN += readme.c

readme.c: README.md
	xxd -i $^ $@

fling: $(FLING_OBJS)
	$(CXX) -o $@ $^ -lX11 -lXinerama -lXmu

dlab: $(DLAB_OBJS)
	$(CXX) -o $@ $^ -lX11 -lXinerama -lXmu

clean:
	rm -f dlab fling $(FLING_OBJS) $(DLAB_OBJS) $(EXTRA_CLEAN)

install:
	cp fling /usr/local/bin
