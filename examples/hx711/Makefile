.PHONY: all clean enable-spidev

all: hx711-test

clean:
	rm -f -- hx711-test

enable-spidev:
	modprobe spidev
	echo spidev > /sys/bus/spi/devices/spi0.0/driver_override
	echo spi0.0 > /sys/bus/spi/drivers/spidev/bind

hx711-test: hx711-test.cpp Makefile
	g++ -Wall -Wextra -std=c++20 -flto=4 -O2 -o $@ hx711-test.cpp
