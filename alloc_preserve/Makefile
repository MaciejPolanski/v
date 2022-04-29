$(info ***************         Start here         *************)
default : preserve.out
	./preserve.out

all : preserve.out ut_preserving.out ut_chunk.out

preserve.out: preserve.cpp mm_alloc_preserve.h ../memory_maps.h
	g++ -g preserve.cpp ../stackoverflow/proc_statm.c -o preserve.out -Wall

test: ut_preserving.out ut_chunk.out
	./ut_chunk.out
	./ut_preserving.out

ut_preserving.out: ut_preserving.cpp mm_alloc_preserve.h ../memory_maps.h
	g++ -g ut_preserving.cpp -o ut_preserving.out -pthread -Wall

ut_chunk.out: ut_chunk.cpp mm_alloc_preserve.h ../memory_maps.h
	g++ -g ut_chunk.cpp -o ut_chunk.out -pthread -Wall

clean:
	rm *.out

