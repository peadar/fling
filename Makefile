CXXFLAGS = -g

all:fling

fling: fling.o
	$(CXX) -o $@ $< -lX11 -lXinerama
clean:
	rm -f fling fling.o
