CXXFLAGS = -g -std=c++0x -Wall

all:fling tile

COMMON_OBJS += common.o area.o

FLING_OBJS += fling.o $(COMMON_OBJS)
TILE_OBJS += tile.o $(COMMON_OBJS)

fling: $(FLING_OBJS)
	$(CXX) -o $@ $^ -lX11 -lXinerama -lXmu

tile: $(TILE_OBJS)
	$(CXX) -o $@ $^ -lX11 -lXinerama -lXmu
clean:
	rm -f tile fling $(COMMON_OBJS) fling.o tile.o

install:
	cp fling /usr/local/bin
