
GCC         = gcc
EXE	    = bridge_crossing
OBJ	    = $(EXE).o
SOURCE      = $(EXE).c

default: $(EXE)

$(OBJ): $(SOURCE)
	$(GCC) -c -o $@ $(SOURCE) -std=gnu99 -lrt -pthread -lm

$(EXE): $(OBJ)
	$(GCC) $(OBJ) -o $(EXE) -std=gnu99 -lrt -pthread -lm

clean:
	rm -rf *.o $(EXE)
