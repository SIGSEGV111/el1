.PHONY: all clean

all: ads111x

clean:
	rm -vf ads111x

ads111x: ads111x.cpp Makefile
	g++ -std=gnu++20 -flto=4 -O0 -g -fwhole-program ads111x.cpp -o ads111x
