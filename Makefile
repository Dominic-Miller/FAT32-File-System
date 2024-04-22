CC = gcc
CFLAGS = -w -Icode 
DEPS = code/fat32_structs.h code/fat32_utils.h
OBJ_NAMES = main.o fat32_utils.o
OBJ = $(addprefix bin/,$(OBJ_NAMES)) 
EXEC = bin/filesys

# Ensure the bin directory exists
$(shell mkdir -p bin)

bin/%.o: code/%.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

$(EXEC): $(OBJ)
	$(CC) -o $@ $^ $(CFLAGS)

.PHONY: clean run

clean:
	rm -f bin/*.o *~ core *~ $(EXEC)

run: $(EXEC)
	./$(EXEC) image/fat32.img
