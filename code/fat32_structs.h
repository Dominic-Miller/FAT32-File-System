#ifndef FAT32_STRUCTS_H
#define FAT32_STRUCTS_H

#include <stdint.h>
#include <stdbool.h>

// Structure for the FAT32 Boot Sector
#pragma pack(push, 1)
struct FAT32BootSector {
    uint8_t  jmpBoot[3];
    char     OEMName[8];
    uint16_t bytesPerSector;
    uint8_t  sectorsPerCluster;
    uint16_t reservedSectorCount;
    uint8_t  numFATs;
    uint16_t rootEntryCount;
    uint16_t totalSectors16;
    uint8_t  media;
    uint16_t FATSize16;
    uint16_t sectorsPerTrack;
    uint16_t numHeads;
    uint32_t hiddenSectors;
    uint32_t totalSectors32;
    uint32_t FATSize32;
    uint16_t extFlags;
    uint16_t FSVersion;
    uint32_t rootCluster;
    uint16_t FSInfo;
    uint16_t backupBootSect;
    uint8_t  reserved[12];
    uint8_t  driveNumber;
    uint8_t  reserved2;
    uint8_t  bootSignature;
    uint32_t volumeID;
    char     volumeLabel[11];
    char     fileSystemType[8];
};
#pragma pack(pop)

// ------------------------------------------------------------------------------------------------ // 

// Structure for a FAT32 Directory Entry (used mainly for ls and cd)
#pragma pack(push, 1)
struct FAT32DirectoryEntry {
    uint8_t  name[11];
    uint8_t  attributes;
    uint8_t  reserved;
    uint8_t  createTimeMs;
    uint16_t createTime;
    uint16_t createDate;
    uint16_t lastAccessDate;
    uint16_t firstClusterHi;
    uint16_t lastWriteTime;
    uint16_t lastWriteDate;
    uint16_t firstClusterLo;
    uint32_t fileSize;
};
#pragma pack(pop)

// ------------------------------------------------------------------------------------------------ // 

#pragma pack(push, 1)
struct OpenFile {
    char filename[12];
    char mode[3];
    uint32_t fileCluster;
    uint32_t offset;
    bool isOpen;
    char path[256];
};
#pragma pack(pop)

#endif 
