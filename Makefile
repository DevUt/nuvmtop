all: nuvmtop

SOURCES := $(wildcard src/*.cpp src/*.hpp)

nuvmtop: $(SOURCES)
	g++ --std=c++2a src/main.cpp -lncurses -o nuvmtop

clean:
	rm uvmtop
