$(info ***************         Start here         *************)
all : a.out ut_preserving.out ut_chunk.out

a.out: main.cpp v_allocator.h memory_maps.h
	g++ -g main.cpp ./stackoverflow/proc_statm.c -Wall

test: ut_preserving.out ut_chunk.out
	./ut_chunk.out
	./ut_preserving.out

ut_preserving.out: ut_preserving.cpp v_allocator.h memory_maps.h
	g++ -g ut_preserving.cpp -o ut_preserving.out -pthread -Wall

ut_chunk.out: ut_chunk.cpp v_allocator.h memory_maps.h
	g++ -g ut_chunk.cpp -o ut_chunk.out -pthread -Wall

clean:
	rm *.out

