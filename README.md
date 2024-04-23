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

### Compilation & Execution
Go to the directory of the Makefile and run the following:
```bash
make run
```
This will compile and run the program

### Cleaning up
Go to the directory of the Part 1 Makefile and run the following:
```bash
make clean
```
This will empty the trace files so you can compile properly again next time

## Commands

After the program complies and begins to run, type in the following command:
```bash
info
```
This will parse through the boot sector and present information about the FAT32 image file.

Type the following command:
```bash
exit
```
This will safely exit the entire program, closing the image and freeing used memory

Type the following command:
```bash
cd [DIRNAME]
```
This command changes directory within the image, '[DIRNAME]' will be replaced by the specified directory created.

Type the following command:
```bash
ls
```
This command lists all the files/directories within the current working directory.

Type the following command:
```bash
mkdir [DIRNAME]
```
This command creates a new directory inside the current working directory, with '[DIRNAME]' being the desired directory name.

Type the following command:
```bash
creat [FILENAME]
```
This command will create a new file inside the current working directory, with '[FILENAME]' being the desired file name.

Type the following command:
```bash
open [FILENAME] [FLAGS]
```
This command will open file [FILENAME] inside the current working directory and the only valid [FLAGS] are:
  -r: read-only
  -w: write-only
  -rw: read-write
  -wr: write-read

Type the following command:
```bash
close [FILENAME]
```
This command will close a file within the current working directory if it already opened.

Type the following command:
```bash
lsof
```
This command will print out a list of files that are opened

Type the following command:
```bash
lseek [FILENAME] [OFFSET]
```
This command will set the offset of file [FILENAME] to the desired [OFFSET] for further reading or writing.

Type the following command:
```bash
read [FILENAME] [SIZE]
```
This command will read data from an opened file starting from the offset stored to the [SIZE] specified.

Type the following command:
```bash
write [FILENAME] [STRING]
```
This command start writing at the file’s offset and stop after writing [STRING].

Type the following command:
```bash
rm [FILENAME]
```
This command deletes a file [FILENAME] within the current working directory.

Type the following command:
```bash
rmdir [DIRNAME]
```
This command removes an empty directory [DIRNAME] within the current working directory.

Type the following command:
```bash
rm -r [DIRNAME]
```
This command removes a directory [DIRNAME] within the current working directory, even if it contains content inside it.

