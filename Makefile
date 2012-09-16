objects = main.o

raidfix : $(objects)
	g++ -o raidfix $(objects)

main.o : main.cpp FileHandle.h

clean :
	rm raidfix $(objects)
