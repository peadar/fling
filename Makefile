CXXFLAGS = -g -std=c++0x -Wall

.PHONY: all clean install

all:fling dlab

FLING_OBJS += fling.o common.o readme.o
DLAB_OBJS += dlab.o common.o
EXTRA_CLEAN += readme.c readme.txt readme.filtered
LDFLAGS += -g

readme.txt: README.md
	cat $^ | sed 's/\*//g' | sed 's/\\//g' > $@
readme.c: readme.txt
	xxd -i $^ $@

fling: $(FLING_OBJS)
	$(CXX) $(LDFLAGS) -o $@ $^ -lX11 -lXinerama -lXmu -lXrandr

dlab: $(DLAB_OBJS)
	$(CXX) $(LDFLAGS) -o $@ $^ -lX11 -lXinerama -lXmu -lXrandr

clean:
	rm -f dlab fling $(FLING_OBJS) $(DLAB_OBJS) $(EXTRA_CLEAN)

install:
	cp fling /usr/local/bin
