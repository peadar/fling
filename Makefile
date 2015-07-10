CXXFLAGS = -g -std=c++0x -Wall

all:fling dlab

FLING_OBJS += fling.o common.o
DLAB_OBJS += dlab.o common.o

fling: $(FLING_OBJS)
	$(CXX) -o $@ $^ -lX11 -lXinerama -lXmu

dlab: $(DLAB_OBJS)
	$(CXX) -o $@ $^ -lX11 -lXinerama -lXmu


clean:
	rm -f fling $(FLING_OBJS) $(DLAB_OBJS)

install:
	cp fling /usr/local/bin
