$(info ***************         Start here         *************)
all : 
	g++ main.cpp ./stackoverflow/proc_statm.c -Wall

test:
	./a.out

