all:
	g++ main.cpp ./perf_event/proc_statm.c

test:
	./a.out

