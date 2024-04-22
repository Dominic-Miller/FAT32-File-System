#ifndef FAT32_UTILS_H
#define FAT32_UTILS_H

#include "fat32_structs.h"
#include <stdio.h>

// Helper functions
void strtoupper(char *str);
void formatDirName(const char *entryName, char *formattedName);
void toFAT32Name(const char* input, char* fat32Name);
uint32_t getFirstSectorOfCluster(uint32_t clusterNumber);
uint32_t getNextCluster(uint32_t currentCluster);
uint32_t findFreeCluster();
void updateFATChain(uint32_t cluster, uint32_t nextCluster);
bool isValidMode(const char *mode);
int findOpenFile(const char *filename);
uint32_t getFileSize(uint32_t firstCluster);
uint32_t convertToUint32(const char *str);
bool extendFileSize(struct OpenFile *file, uint32_t newSize);
int isDirectoryEmpty(uint32_t cluster);
void removeDirectoryEntry(const char *filename);
void freeClusters(uint32_t clusterNumber);
int findDirectoryEntry(const char *filename, struct FAT32DirectoryEntry *entry);
void deleteDirectoryContents(uint32_t cluster);


// Main implementation shell functions
void printInfo(const struct FAT32BootSector *bs);
void ls(int currentClusterNumber);
int cd(int currentDirCluster, const char *dirName);
void creat(const char *filename);
void mkdir(const char *dirName);
int open(char* filename, char* mode);
int close(char* filename);
void lsof();
int lseek(char* filename, uint32_t offset);
int read(char* filename, uint32_t size);
int write(char *filename, char *string);
int rm(const char *filename);
int rmdir(const char *filename);
void rmr(const char* dirname);

#endif 
