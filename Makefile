CXXFLAGS = -g -std=c++0x

all:fling

fling: fling.o
	$(CXX) -o $@ $< -lX11 -lXinerama -lXmu
clean:
	rm -f fling fling.o

install:
	cp fling /usr/local/bin
