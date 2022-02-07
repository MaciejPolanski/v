$(info ***************         Start here         *************)
all : a.out test.out

a.out: main.cpp v_allocator.h memory_maps.h
	g++ -g main.cpp ./stackoverflow/proc_statm.c -Wall

test.out: preserving_unit_test.cpp v_allocator.h memory_maps.h
	g++ -g preserving_unit_test.cpp -o test.out -Wall

clean:
	rm *.out

