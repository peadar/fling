CXXFLAGS = -g -std=c++0x -Wall

all:fling tile

COMMON_OBJS += common.o

FLING_OBJS += fling.o $(COMMON_OBJS)
TILE_OBJS += area.o $(COMMON_OBJS)

fling: $(FLING_OBJS)
	$(CXX) -o $@ $^ -lX11 -lXinerama -lXmu

tile: $(TILE_OBJS)
	$(CXX) -o $@ $^ -lX11 -lXinerama -lXmu
clean:
	rm -f tile fling $(COMMON_OBJS) fling.o area.o

install:
	cp fling /usr/local/bin
