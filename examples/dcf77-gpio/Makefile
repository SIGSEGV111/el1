.PHONY: all clean

all: dcf77-gpio

clean:
	rm -vf dcf77-gpio

dcf77-gpio: dcf77-gpio.cpp Makefile
	g++ -Wall -Wextra -Wno-volatile -std=gnu++20 -flto=4 -O3 -fwhole-program dcf77-gpio.cpp -o dcf77-gpio -I ../../src/
