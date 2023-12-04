.PHONY: all clean

all: neopixel-spi-driver

clean:
	rm -vf neopixel-spi-driver

neopixel-spi-driver: neopixel-spi-driver.cpp Makefile
	g++ -I ../../gen/srclib/include -std=gnu++20 -flto=4 -O0 -g -fwhole-program neopixel-spi-driver.cpp -o neopixel-spi-driver
