.PHONY: all clean

all: bin2cpp

clean:
	rm -vf bin2cpp

bin2cpp: bin2cpp.cpp Makefile
	g++ -std=gnu++20 -flto=4 -O3 -fwhole-program bin2cpp.cpp -o bin2cpp
