#ifndef GLOBALS_H
#define GLOBALS_H

#include <stdio.h>
#include <stdint.h>

extern FILE *imgFile;
extern struct FAT32BootSector bootSector;
extern uint32_t currentDirCluster;
extern struct OpenFile openFiles[10];

#define ATTR_READ_ONLY   0x01
#define ATTR_HIDDEN      0x02
#define ATTR_SYSTEM      0x04
#define ATTR_VOLUME_ID   0x08
#define ATTR_DIRECTORY   0x10
#define ATTR_ARCHIVE     0x20

#endif
