all: nuvmtop

SOURCES := $(wildcard src/*.cpp src/*.hpp)

nuvmtop: $(SOURCES)
	g++ --std=c++2a src/main.cpp src/data_puller.cpp -lncurses -o nuvmtop

clean:
	rm nuvmtop
