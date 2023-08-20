.PHONY: all clean test

all: w1-test

clean:
	rm -vf w1-test

w1-test: w1-test.cpp Makefile
	g++ -pthread -std=gnu++20 -g -O0 -o w1-test w1-test.cpp

test: clean w1-test
	bash -c 'time ./w1-test'
