# FAT32-File-System
The purpose of this project is to get familiar with basic file-system design and implementation. I will need to incorporate various aspects of the FAT32 file system, such as cluster-based storage, FAT tables, sectors, and directory structure.

## File Listing
```
bin/
│
├── fat32_utils.o
├── filesys
├── main.o
│
code/
|
├── fat32_structs.h
├── fat32_utils.c
├── fat32_utils.h
├── globals.h
├── main.c
|
image/
|
├── fat32.img
|
Makefile
|
│
README.md
```
## How to Compile & Execute

### Compilation & Execution for Part 1
Go to the directory of the Makefile and run the following:
```bash
make run
```
This will compile and run the program

### Cleaning up for Part 1
Go to the directory of the Part 1 Makefile and run the following:
```bash
make clean
```
This will empty the trace files so you can compile properly again next time
