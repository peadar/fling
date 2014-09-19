CXXFLAGS = -g -std=c++0x -Wall

all:fling

OBJS += fling.o common.o

fling: $(OBJS)
	$(CXX) -o $@ $^ -lX11 -lXinerama -lXmu
clean:
	rm -f fling $(OBJS)

install:
	cp fling /usr/local/bin
