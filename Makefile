$(info ***************         Start here         *************)
default : main.out
	./main.out

all : main.out

main.out: main.cpp mm_alloc.h memory_maps.h
	g++ -g main.cpp ./stackoverflow/proc_statm.c -o main.out -Wall

clean:
	rm *.out

