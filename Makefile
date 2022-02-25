$(info ***************         Start here         *************)
default : main.out
	./main.out

all : main.out ut_preserving.out ut_chunk.out

main.out: main.cpp mm_alloc.h memory_maps.h
	g++ -g main.cpp ./stackoverflow/proc_statm.c -o main.out -Wall

test: ut_preserving.out ut_chunk.out
	./ut_chunk.out
	./ut_preserving.out

ut_preserving.out: ut_preserving.cpp mm_alloc.h memory_maps.h
	g++ -g ut_preserving.cpp -o ut_preserving.out -pthread -Wall

ut_chunk.out: ut_chunk.cpp mm_alloc.h memory_maps.h
	g++ -g ut_chunk.cpp -o ut_chunk.out -pthread -Wall

clean:
	rm *.out

