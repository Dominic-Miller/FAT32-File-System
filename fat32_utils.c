#include "fat32_structs.h"
#include "fat32_utils.h" 
#include "globals.h"
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>

#define ATTR_READ_ONLY   0x01
#define ATTR_HIDDEN      0x02
#define ATTR_SYSTEM      0x04
#define ATTR_VOLUME_ID   0x08
#define ATTR_DIRECTORY   0x10
#define ATTR_ARCHIVE     0x20



// ------------------------------------------------------------------------------------------------ //

// Helper function implementations

// Function to convert a string to uppercase (needed for cd string comparison)
void strtoupper(char *str) {
    while(*str) {
        *str = toupper((unsigned char)*str);
        str++;
    }
}

// Function to format a directory name for comparison between user input and FAT32 directories
void formatDirName(const char *entryName, char *formattedName) {
    int i, j;

    // Copy the first 8 characters (filename) to formattedName
    for (i = 0, j = 0; i < 8 && entryName[i] != ' '; ++i, ++j) {
        formattedName[j] = entryName[i];
    }

    // Check if there's an extension
    if (entryName[8] != ' ') {
        // Add a dot before the extension
        formattedName[j++] = '.';

        // Copy the extension (next 3 characters)
        for (i = 8; i < 11 && entryName[i] != ' '; ++i, ++j) {
            formattedName[j] = entryName[i];
        }
    }

    // Null-terminate the formatted name
    formattedName[j] = '\0';

    // Convert to upper case
    strtoupper(formattedName);
}

// Function to convert an input name to the FAT32 naming convention (for ls and creat functions)
void toFAT32Name(const char* input, char* fat32Name) {
    memset(fat32Name, ' ', 11); // Fill with spaces
    int i = 0, j = 0;
    // Copy the name part up to 8 characters or until a dot
    for (; input[i] != '\0' && input[i] != '.' && j < 8; i++, j++) {
        fat32Name[j] = toupper((unsigned char)input[i]);
    }
    // Process extension if present
    if (input[i] == '.') {
        i++;
        j = 8; // Start filling extension at position 8
        for (; input[i] != '\0' && j < 11; i++, j++) {
            fat32Name[j] = toupper((unsigned char)input[i]);
        }
    }
    fat32Name[11] = '\0'; // Ensure null termination for safety
}


// Find a free cluster in the FAT and return its number
uint32_t findFreeCluster() {
    uint32_t fatOffset = bootSector.reservedSectorCount;
    uint32_t fatSector = fatOffset;
    uint32_t fatEntryValue;

    for (uint32_t i = 2; i < bootSector.FATSize32 * bootSector.bytesPerSector / 4; i++) {
        if (fatOffset >= fatSector * bootSector.bytesPerSector + bootSector.bytesPerSector) {
            fatSector++;
            fatOffset = bootSector.reservedSectorCount + fatSector;
        }

        fseek(imgFile, fatOffset, SEEK_SET);
        fread(&fatEntryValue, sizeof(uint32_t), 1, imgFile);

        // Move to the next FAT entry
        fatOffset += 4;  

        if (fatEntryValue == 0) {
            return i;
        }
    }
    // If this far, no free cluster found
    return 0xFFFFFFFF; 
}

// Function to get the first sector of a cluster
uint32_t getFirstSectorOfCluster(uint32_t clusterNumber) {
    return ((clusterNumber - 2) * bootSector.sectorsPerCluster) + bootSector.reservedSectorCount + (bootSector.numFATs * bootSector.FATSize32);
}

// Function to get the next cluster given the current one
uint32_t getNextCluster(uint32_t currentCluster) {
    uint32_t fatOffset = currentCluster * 4; // Each FAT entry is 4 bytes
    uint32_t fatSector = bootSector.reservedSectorCount + (fatOffset / bootSector.bytesPerSector);
    uint32_t entOffset = fatOffset % bootSector.bytesPerSector;

    uint32_t fatTablePosition = (fatSector * bootSector.bytesPerSector) + entOffset;

    fseek(imgFile, fatTablePosition, SEEK_SET);
    uint32_t nextCluster;
    if (fread(&nextCluster, sizeof(uint32_t), 1, imgFile) != 1) {
        perror("Error reading next cluster"); // Error handling if read fails
        return 0xFFFFFFFF; // Indicate an error or end-of-chain if reading fails
    }

    nextCluster &= 0x0FFFFFFF; // Mask to get 28 lower bits

    //printf("Current Cluster: %u, Next Cluster: %u, Position: %u\n",
    //        currentCluster, nextCluster, fatTablePosition);

    // Handling end-of-chain or erroneous zero cluster (which should not happen unless it's the start of the data region)
    if (nextCluster >= 0x0FFFFFF8 || nextCluster == 0) {
        return 0xFFFFFFFF; // End of cluster chain or invalid cluster
    }

    return nextCluster;
}

// Update the FAT chain by setting the next cluster for the given cluster
void updateFATChain(uint32_t cluster, uint32_t nextCluster) {
    uint32_t fatOffset = cluster * 4 + bootSector.reservedSectorCount * bootSector.bytesPerSector;
    uint32_t fatSector = fatOffset / bootSector.bytesPerSector;
    uint32_t entOffset = fatOffset % bootSector.bytesPerSector;

    fseek(imgFile, fatSector * bootSector.bytesPerSector + entOffset, SEEK_SET);
    fwrite(&nextCluster, sizeof(uint32_t), 1, imgFile);
}

// Function to check if mode for opening a file is valid
bool isValidMode(const char *mode) {
    return strcmp(mode, "-r") == 0 || strcmp(mode, "-w") == 0 ||
           strcmp(mode, "-rw") == 0 || strcmp(mode, "-wr") == 0;
}

// Function to check if file is already open (At max 10 files can be opened)
int findOpenFile(const char *filename) {
    for (int i = 0; i < 10; i++) {
        if (openFiles[i].isOpen && strcmp(openFiles[i].filename, filename) == 0) {
            return i;
        }
    }
    return -1;
}

// Function to calculate the file size based on its starting cluster
uint32_t getFileSize(uint32_t firstCluster) {
    uint32_t clusterSize = bootSector.sectorsPerCluster * bootSector.bytesPerSector;
    uint32_t fileSize = 0;
    uint32_t currentCluster = firstCluster;

    // While we are not at the end of the chain, add up the cluster sizes
    while (currentCluster < 0x0FFFFFF8) { 
        fileSize += clusterSize;
        currentCluster = getNextCluster(currentCluster);
    }

    return fileSize;
}

// Function to convert a char* to a uni32_t (used for lseek function when taking in input)
uint32_t convertToUint32(const char *str) {
    char *endptr;
    uint32_t value = (uint32_t)strtoul(str, &endptr, 10);

    if (*endptr != '\0') {
        printf("Conversion error, non-numeric data found: %s\n", endptr);
        return 0;
    }

    return value;
}

// Function to extend the file size by allocating new clusters as needed (used for write function)
bool extendFileSize(struct OpenFile *file, uint32_t newSize) {
    uint32_t currentCluster = file->fileCluster;
    uint32_t clusterSize = bootSector.sectorsPerCluster * bootSector.bytesPerSector;
    uint32_t fileSize = getFileSize(currentCluster);
    uint32_t lastCluster = currentCluster;
    bool foundFreeCluster = false;

    // Check if the current size is already sufficient
    if (fileSize >= newSize) {
        return true;
    }

    // Calculate how many additional clusters are needed
    while (fileSize < newSize) {
        uint32_t nextCluster = getNextCluster(currentCluster);

        // If the current cluster is the end of the chain, find a new cluster
        if (nextCluster >= 0x0FFFFFF8) {
            foundFreeCluster = false;
            uint32_t newCluster = findFreeCluster();
            if (newCluster == 0xFFFFFFFF) {
                printf("Error: No free clusters available.\n");
                return false;
            }

            // Update the FAT to link the new cluster
            updateFATChain(currentCluster, newCluster);
            updateFATChain(newCluster, 0x0FFFFFF8);
            foundFreeCluster = true;
            nextCluster = newCluster;
        }

        currentCluster = nextCluster;
        fileSize += clusterSize;

        if (!foundFreeCluster) {
            lastCluster = currentCluster;
        }
    }

    // Update the last cluster if it wasn't already updated
    if (!foundFreeCluster) {
        updateFATChain(lastCluster, 0x0FFFFFF8);
    }

    return true;
}

// Helper function to determine if a directory is empty
int isDirectoryEmpty(uint32_t cluster) {
    uint32_t firstSector = getFirstSectorOfCluster(cluster);
    struct FAT32DirectoryEntry dirEntry;

    fseek(imgFile, firstSector * bootSector.bytesPerSector, SEEK_SET);
    for (int i = 0; i < bootSector.sectorsPerCluster * (bootSector.bytesPerSector / sizeof(dirEntry)); ++i) {
        fread(&dirEntry, sizeof(dirEntry), 1, imgFile);
        if (dirEntry.name[0] == 0) break;  // End of directory
        if (dirEntry.name[0] == 0xE5) continue;  // Skip deleted entries
        if (strncmp(dirEntry.name, ".          ", 11) == 0 || strncmp(dirEntry.name, "..         ", 11) == 0) {
            continue;  // Skip '.' and '..' entries
        }
        return 0;  // Found a valid entry, directory is not empty
    }
    return 1;  // No valid entries found, directory is empty
}


int findDirectoryEntry(const char *filename, struct FAT32DirectoryEntry *entry) {
    uint32_t currentCluster = currentDirCluster;
    struct FAT32DirectoryEntry dirEntry;

    char fat32Name[12];
    toFAT32Name(filename, fat32Name);

    do {
        uint32_t firstSector = getFirstSectorOfCluster(currentCluster);
        fseek(imgFile, firstSector * bootSector.bytesPerSector, SEEK_SET);

        for (int i = 0; i < bootSector.sectorsPerCluster * (bootSector.bytesPerSector / sizeof(dirEntry)); ++i) {
            fread(&dirEntry, sizeof(dirEntry), 1, imgFile);

            if (dirEntry.name[0] == 0) {  // End of directory entries
                return -1;
            }
            if (dirEntry.name[0] == 0xE5) continue;  // Skip deleted entries

            if (strncmp(dirEntry.name, fat32Name, 11) == 0) {
                *entry = dirEntry;
                return 0;  // Found entry
            }
        }

        currentCluster = getNextCluster(currentCluster);
    } while (currentCluster != 0xFFFFFFFF);

    return -1;  // Entry not found
}

void removeDirectoryEntry(const char *filename) {
    uint32_t currentCluster = currentDirCluster;
    struct FAT32DirectoryEntry dirEntry;

    do {
        uint32_t firstSector = getFirstSectorOfCluster(currentCluster);
        fseek(imgFile, firstSector * bootSector.bytesPerSector, SEEK_SET);

        for (int i = 0; i < bootSector.sectorsPerCluster * (bootSector.bytesPerSector / sizeof(dirEntry)); ++i) {
            fread(&dirEntry, sizeof(dirEntry), 1, imgFile);

            char formattedName[12];
            memcpy(formattedName, dirEntry.name, 11);
            formattedName[11] = '\0'; // Ensure null termination

            if (strncmp(formattedName, filename, 11) == 0) {
                fseek(imgFile, -((long)sizeof(dirEntry)), SEEK_CUR);
                dirEntry.name[0] = 0xE5; // Mark as deleted
                fwrite(&dirEntry, sizeof(dirEntry), 1, imgFile);
                fflush(imgFile);  // Ensure the change is written immediately
                return;
            }
        }

        currentCluster = getNextCluster(currentCluster);
    } while (currentCluster != 0xFFFFFFFF);
}

void freeClusters(uint32_t clusterNumber) {
    uint32_t currentCluster = clusterNumber;
    uint32_t nextCluster;

    while (currentCluster < 0x0FFFFFF8) {
        nextCluster = getNextCluster(currentCluster);
        updateFATChain(currentCluster, 0x00000000); // Mark the cluster as free in the FAT
        currentCluster = nextCluster;
    }
}

// Helper function to delete all files and subdirectories recursively
void deleteDirectoryContents(uint32_t cluster) {
    struct FAT32DirectoryEntry dirEntry;
    uint32_t firstSector = getFirstSectorOfCluster(cluster);

    fseek(imgFile, firstSector * bootSector.bytesPerSector, SEEK_SET);

    for (int i = 0; i < bootSector.sectorsPerCluster * (bootSector.bytesPerSector / sizeof(dirEntry)); i++) {
        fread(&dirEntry, sizeof(dirEntry), 1, imgFile);

        // If we reach the end of the directory, break
        if (dirEntry.name[0] == 0) {
            break; 
        }

        // Skip deleted entries and current/parent directory references
        if (dirEntry.name[0] == 0xE5 || strncmp(dirEntry.name, ".          ", 11) == 0 || strncmp(dirEntry.name, "..         ", 11) == 0) {
            continue;
        }

        char name[12];
        formatDirName(dirEntry.name, name);

        // If it is a subdirectory, recursively call the delete function
        if (dirEntry.attributes & ATTR_DIRECTORY) {
            deleteDirectoryContents((dirEntry.firstClusterHi << 16) | dirEntry.firstClusterLo);
            rmdir(name);
        } 
        // If it is a file, delete it
        else {
            rm(name);
        }
    }
}

// ------------------------------------------------------------------------------------------------ //

// Function implementations for the shell

// Function to print the info of the boot sector
void printInfo(const struct FAT32BootSector *bs) {
    printf("Bytes Per Sector: %d\n", bs->bytesPerSector);
    printf("Sectors Per Cluster: %d\n", bs->sectorsPerCluster);
    printf("Root Cluster: %d\n", bs->rootCluster);
    // Assuming the calculation for total # of clusters in data region is correct
    printf("Total # of Clusters in Data Region: %d\n", (bs->totalSectors32 - bs->reservedSectorCount - (bs->numFATs * bs->FATSize32)) / bs->sectorsPerCluster);
    printf("# of Entries in One FAT: %d\n", bs->FATSize32 * bs->bytesPerSector / 4); // Each FAT entry is 4 bytes
    printf("Size of Image (in bytes): %d\n", bs->totalSectors32 * bs->bytesPerSector);
}

// Function to list the available directories and files from the current cluster
void ls(int currentClusterNumber) {
    uint32_t currentCluster = currentClusterNumber;
    struct FAT32DirectoryEntry dirEntry;

    // While we are in our range of accessible clusters, search for all of the directories and files
    do {
        uint32_t firstSector = getFirstSectorOfCluster(currentCluster);
        fseek(imgFile, firstSector * bootSector.bytesPerSector, SEEK_SET);

        for (int i = 0; i < bootSector.sectorsPerCluster * (bootSector.bytesPerSector / sizeof(dirEntry)); ++i) {
            fread(&dirEntry, sizeof(dirEntry), 1, imgFile);

            // If we reach the end of the directory, break
            if (dirEntry.name[0] == 0) break; 

            // If not a deleted directory or file, get and print the name
            if (dirEntry.name[0] != 0xE5) {
                char name[12];
                memcpy(name, dirEntry.name, 11);
                name[11] = '\0';
                formatDirName(name, name);

                // Print the name if it is a directory or a file only
                if (dirEntry.attributes == 0x10 || dirEntry.attributes == 0x20) {
                    printf("%s\n", name);
                } 
            }
        }
        // Move to next cluster in the chain
        currentCluster = getNextCluster(currentCluster);
    } while (currentCluster < 0x0FFFFFF8);
}

// Function to change the current directory given the current cluster and the directory name
int cd(int currentDirCluster, const char *dirName) {
    struct FAT32DirectoryEntry dirEntry;
    uint32_t currentCluster = currentDirCluster;

    // If cd .. we must go to the parent directory
    if (strcmp(dirName, "..") == 0) {
        // If we are already at the root directory, simply return the current cluster
        if (currentDirCluster == bootSector.rootCluster) {
            return bootSector.rootCluster;
        }

        // Navigate to the parent directory by finding the ".." entry in the current directory
        uint32_t firstSector = getFirstSectorOfCluster(currentCluster);
        fseek(imgFile, firstSector * bootSector.bytesPerSector, SEEK_SET);

        for (int i = 0; i < bootSector.sectorsPerCluster * (bootSector.bytesPerSector / sizeof(dirEntry)); ++i) {
            fread(&dirEntry, sizeof(dirEntry), 1, imgFile);

            // Check if this is the ".." entry by comparing the first 11 characters
            if (strncmp(dirEntry.name, "..         ", 11) == 0) {
                // Calculate the parent directory's cluster number
                uint32_t parentCluster = (dirEntry.firstClusterHi << 16) | dirEntry.firstClusterLo;

                // In FAT32, root's parent is considered as root itself
                if (parentCluster == 0) {
                    parentCluster = bootSector.rootCluster;
                }

                return parentCluster;
            }
        }
        // If we got here, something went wrong, return -1
        return -1;
    }

    // Get the uppercase of the name for comparison to FAT32 directories
    char upperDirName[12];
    strncpy(upperDirName, dirName, sizeof(upperDirName) - 1);
    upperDirName[11] = '\0';
    strtoupper(upperDirName);

    // While we are in our range of accessible clusters, search through all the directories
    do {
        uint32_t firstSector = getFirstSectorOfCluster(currentCluster);
        fseek(imgFile, firstSector * bootSector.bytesPerSector, SEEK_SET);

        // Search through all entries in the current cluster
        for (int i = 0; i < bootSector.sectorsPerCluster * (bootSector.bytesPerSector / sizeof(dirEntry)); ++i) {
            fread(&dirEntry, sizeof(dirEntry), 1, imgFile);

            // If we reach the end of the directory, break
            if (dirEntry.name[0] == 0) break; 

            // If directory has been deleted or does not exist, continue
            if (dirEntry.name[0] == 0xE5 || !(dirEntry.attributes & 0x10)) continue;

            // Otherwise, get the directory name and check if it's what we are looking for
            char formattedName[12];
            formatDirName(dirEntry.name, formattedName);

            // If we found the directory, move to it, and return successful change
            if (strcmp(formattedName, upperDirName) == 0) {
                // Check if this is a directory or a file
                if (dirEntry.attributes & 0x10) {
                    return (dirEntry.firstClusterHi << 16) | dirEntry.firstClusterLo;
                } 
                // Found, but it's not a directory
                else {
                    return -2; 
                }
            }
        }
        // Update our current cluster
        currentCluster = getNextCluster(currentCluster);
    } while (currentCluster < 0x0FFFFFF8);

    // If this far, directory does not exist
    return -1; 
}

// Function to creat a new file in the current dirctory with the given name
void creat(const char *filename) {
    struct FAT32DirectoryEntry dirEntry;
    uint32_t currentCluster = currentDirCluster;
    int foundEmpty = 0;
    long emptyEntryPos = -1;
    uint32_t lastClusterInChain = 0;

    // Convert the filename to FAT32 format
    char formattedName[12];
    toFAT32Name(filename, formattedName);

    // Checker to see if we have already found an available position
    int foundPos = 0;

    // Scan through the avaiable clusters
    do {
        uint32_t firstSector = getFirstSectorOfCluster(currentCluster);
        
        // Scan through all of the entries in each available cluster
        for (int i = 0; i < bootSector.sectorsPerCluster * (bootSector.bytesPerSector / sizeof(dirEntry)); ++i) {
            fseek(imgFile, firstSector * bootSector.bytesPerSector + (i * sizeof(dirEntry)), SEEK_SET);
            fread(&dirEntry, sizeof(dirEntry), 1, imgFile);

            // If we have not found an available position yet, keep looking
            if (!foundPos) {
                // If we are at the end of the directory, mark this position
                if (dirEntry.name[0] == 0x00) {
                    if (!foundEmpty) {
                        foundEmpty = 1;
                        emptyEntryPos = ftell(imgFile) - sizeof(dirEntry);
                        foundPos = 1;
                    }
                    break;
                }

                // If we found an empty entry, we can create a new file here
                if (dirEntry.name[0] == 0xE5 && !foundEmpty) {
                    foundEmpty = 1;
                    emptyEntryPos = ftell(imgFile) - sizeof(dirEntry);
                    foundPos = 1;
                }
            }

            // Check if a file or directory with the same name already exists
            char existingName[12];
            toFAT32Name((char *)dirEntry.name, existingName);
            toFAT32Name(filename, formattedName);
            if (strcmp(formattedName, existingName) == 0) {
                fprintf(stderr, "A file or directory named %s already exists.\n", filename);
                return;
            }
        }

        // If empty entry in a cluster, break so we can create the new file at the currentpos
        if (foundEmpty) {
            break;
        }

        lastClusterInChain = currentCluster;
        currentCluster = getNextCluster(currentCluster);
    } while (currentCluster < 0x0FFFFFF8);

    // Convert the filename to FAT32 format again (it gets emptied in the loop)
    toFAT32Name(filename, formattedName);

    // If no empty entry found, allocate a new cluster for the directory
    if (!foundEmpty) {
        uint32_t newCluster = findFreeCluster();
        if (newCluster == 0xFFFFFFFF) {
            printf("No free cluster available.\n");
            return;
        }

        // Update the FAT chain
        updateFATChain(lastClusterInChain, newCluster);
        updateFATChain(newCluster, 0x0FFFFFF8);

        // Set emptyEntryPos to the beginning of the new cluster
        emptyEntryPos = getFirstSectorOfCluster(newCluster) * bootSector.bytesPerSector;
    }

    // Construct the new directory entry for the file
    memset(&dirEntry, 0, sizeof(dirEntry));
    memcpy(dirEntry.name, formattedName, 11);
    dirEntry.attributes = 0x20;
    dirEntry.firstClusterHi = 0x00;
    dirEntry.firstClusterLo = 0x00;
    dirEntry.fileSize = 0;

    // Write the new directory entry to the found position
    fseek(imgFile, emptyEntryPos, SEEK_SET);
    fwrite(&dirEntry, sizeof(dirEntry), 1, imgFile);
    fflush(imgFile);

    printf("File %s created successfully.\n", filename);
}

// Function to create a new directory with the given name in the current directory
void mkdir(const char *dirName) {
    struct FAT32DirectoryEntry dirEntry;
    uint32_t currentCluster = currentDirCluster;
    int foundEmpty = 0;
    long emptyEntryPos = -1;
    uint32_t lastClusterInChain = 0;
    uint32_t newClusterNum = 0;

    // Convert the filename to FAT32 format
    char formattedName[12];
    toFAT32Name(dirName, formattedName);

    // Checker to see if we have already found an available position
    int foundPos = 0;

    // Scan through the avaiable clusters
    do {
        uint32_t firstSector = getFirstSectorOfCluster(currentCluster);
        fseek(imgFile, firstSector * bootSector.bytesPerSector, SEEK_SET);

        // Scan through all of the entries in each available cluster
        for (int i = 0; i < bootSector.sectorsPerCluster * (bootSector.bytesPerSector / sizeof(dirEntry)); ++i) {
            fread(&dirEntry, sizeof(dirEntry), 1, imgFile);

            // If we have not found an available position yet, keep looking
            if (!foundPos) {
                // If we are at the end of the directory, mark this position
                if (dirEntry.name[0] == 0x00) {
                    if (!foundEmpty) {
                        foundEmpty = 1;
                        emptyEntryPos = ftell(imgFile) - sizeof(dirEntry);
                        foundPos = 1;
                    }
                    break;
                }

                // If we found an empty entry, we can create a new file here
                if (dirEntry.name[0] == 0xE5 && !foundEmpty) {
                    foundEmpty = 1;
                    emptyEntryPos = ftell(imgFile) - sizeof(dirEntry);
                    foundPos = 1;
                }
            }

            // Check if a file or directory with the same name already exists
            char existingName[12];
            toFAT32Name((char *)dirEntry.name, existingName);
            toFAT32Name(dirName, formattedName);
            if (strcmp(formattedName, existingName) == 0) {
                fprintf(stderr, "A file or directory named %s already exists.\n", dirName);
                return;
            }
        }

        // If empty entry in a cluster, break so we can create the new file at the currentpos
        if (foundEmpty) {
            break;
        }

        lastClusterInChain = currentCluster;
        currentCluster = getNextCluster(currentCluster);
    } while (currentCluster < 0x0FFFFFF8);

    // Convert the filename to FAT32 format again (it gets emptied in the loop)
    toFAT32Name(dirName, formattedName);

    // If no empty entry found, allocate a new cluster for the directory
    if (!foundEmpty) {
        newClusterNum = findFreeCluster();
        if (newClusterNum == 0xFFFFFFFF) {
            printf("No free cluster available.\n");
            return;
        }

        // Update the FAT chain
        updateFATChain(lastClusterInChain, newClusterNum);
        updateFATChain(newClusterNum, 0x0FFFFFF8);

        // Set emptyEntryPos to the beginning of the new cluster
        emptyEntryPos = getFirstSectorOfCluster(newClusterNum) * bootSector.bytesPerSector;
    }

    // Construct the new directory entry for the directory
    memset(&dirEntry, 0, sizeof(dirEntry));
    memcpy(dirEntry.name, formattedName, 11);
    dirEntry.attributes = 0x10;
    dirEntry.firstClusterHi = (newClusterNum >> 16) & 0xFFFF;
    dirEntry.firstClusterLo = newClusterNum & 0xFFFF;
    dirEntry.fileSize = 0;

    // Write the new directory entry to the found position
    fseek(imgFile, emptyEntryPos, SEEK_SET);
    fwrite(&dirEntry, sizeof(dirEntry), 1, imgFile);

    // Create '.' and '..' entries inside the new directory
    fseek(imgFile, getFirstSectorOfCluster(newClusterNum) * bootSector.bytesPerSector, SEEK_SET);

    // '.' entry
    memset(&dirEntry, 0, sizeof(dirEntry));
    memcpy(dirEntry.name, ".          ", 11);
    dirEntry.attributes = 0x10;
    dirEntry.firstClusterHi = (newClusterNum >> 16) & 0xFFFF;
    dirEntry.firstClusterLo = newClusterNum & 0xFFFF;
    fwrite(&dirEntry, sizeof(dirEntry), 1, imgFile);

    // '..' entry
    memset(&dirEntry, 0, sizeof(dirEntry));
    memcpy(dirEntry.name, "..         ", 11);
    dirEntry.attributes = 0x10;
    dirEntry.firstClusterHi = (currentDirCluster >> 16) & 0xFFFF;
    dirEntry.firstClusterLo = currentDirCluster & 0xFFFF;
    fwrite(&dirEntry, sizeof(dirEntry), 1, imgFile);

    // Add an End-of-Directory marker after the new entry and commit the changes to the image file
    struct FAT32DirectoryEntry eodMarker = {0};
    fwrite(&eodMarker, sizeof(struct FAT32DirectoryEntry), 1, imgFile);
    fflush(imgFile);

    printf("Directory %s created successfully.\n", dirName);
}

// Function to open a file from the current directory (reads in the mode to open the file)
int open(char* filename, char* mode) {
    // First check if a valid mode was given
    if (!isValidMode(mode)) {
        printf("Invalid mode specified.\n");
        return -1;
    }

    // Next, check if the file is already open
    if (findOpenFile(filename) != -1) {
        printf("File '%s' is already open.\n", filename);
        return -1;
    }

    // Get the uppercase of the name for comparison to FAT32 files
    char upperFileName[12];
    strncpy(upperFileName, filename, sizeof(upperFileName) - 1);
    upperFileName[11] = '\0';
    strtoupper(upperFileName);

    uint32_t currentCluster = currentDirCluster;
    struct FAT32DirectoryEntry dirEntry;

    // Loop through the clusters in the directory until you find the file
    do {
        uint32_t firstSector = getFirstSectorOfCluster(currentCluster);
        fseek(imgFile, firstSector * bootSector.bytesPerSector, SEEK_SET);

        // Search through all entries in the current cluster
        for (int i = 0; i < bootSector.sectorsPerCluster * (bootSector.bytesPerSector / sizeof(dirEntry)); ++i) {
            fread(&dirEntry, sizeof(dirEntry), 1, imgFile);

            // If we reach the end of the directory, break
            if (dirEntry.name[0] == 0) break;

            // If file has been deleted or does not exist, continue
            if (dirEntry.name[0] == 0xE5 || !(dirEntry.attributes & 0x20)) continue;

            // Otherwise, get the file name and check if it's what we are looking for
            char formattedName[12];
            formatDirName(dirEntry.name, formattedName);

            // If we found the file, open it with the given mode
            if (strcmp(formattedName, upperFileName) == 0) {
                // Go through our open files an make sure this one is not already in there
                for (int j = 0; j < 10; j++) {
                    if (!openFiles[j].isOpen) {
                        // Add this file to our open files with the mode opened (without the -)
                        strcpy(openFiles[j].filename, formattedName);
                        strcpy(openFiles[j].mode, mode + 1);
                        openFiles[j].fileCluster = (dirEntry.firstClusterHi << 16) | dirEntry.firstClusterLo;
                        openFiles[j].offset = 0;
                        openFiles[j].isOpen = true;
                        printf("File '%s' opened in mode '%s'.\n", filename, mode);
                        return 0;
                    }
                }
                printf("Max open files limit reached.\n");
                return -1;
            }
        }
        // Update our current cluster
        currentCluster = getNextCluster(currentCluster);
    } while (currentCluster < 0x0FFFFFF8);

    // If this far, file does not exist
    printf("File '%s' does not exist.\n", filename);
    return -1;
}

// Function to find and close an open file by its filename
int close(char* filename) {

    // Get the uppercase of the name for comparison to FAT32 files
    char upperFileName[12];
    strncpy(upperFileName, filename, sizeof(upperFileName) - 1);
    upperFileName[11] = '\0';
    strtoupper(upperFileName);

    // Check if the file is currently open
    for (int i = 0; i < 10; i++) {
        // If we find the file, reset all of its parameters so it doesn't take up space in the array
        if (openFiles[i].isOpen && strcmp(openFiles[i].filename, upperFileName) == 0) {
            openFiles[i].filename[0] = '\0';
            openFiles[i].mode[0] = '\0';    
            openFiles[i].fileCluster = 0;     
            openFiles[i].offset = 0;          
            openFiles[i].isOpen = false;
            printf("File '%s' closed successfully.\n", filename);
            return 0;
        }
    }

    // If the function has not returned by now, the file was either not open or does not exist
    printf("File '%s' is not open or does not exist in the directory.\n", filename);
    return -1;
}

// Function to list all opened files
void lsof() {
    bool anyOpen = false;

    // Header for the list
    printf("%-10s %-12s %-10s %-10s %s\n", "Index", "Filename", "Mode", "Offset", "Path");
    
    // Loop through the array of open files and print out the details
    for (int i = 0; i < 10; i++) {
        if (openFiles[i].isOpen) {
            anyOpen = true;

            // Print details of each opened file
            printf("%-10d %-12s %-10s %-10u %s\n",
                   i, 
                   openFiles[i].filename,
                   openFiles[i].mode,
                   openFiles[i].offset,
                   openFiles[i].path);
        }
    }

    // If none are open, print that
    if (!anyOpen) {
        printf("No files are currently opened.\n");
    }
}

// Function to set the file offset for reading/writing
int lseek(char* filename, uint32_t offset) {

    // Get the uppercase of the name for comparison to FAT32 files
    char upperFileName[12];
    strncpy(upperFileName, filename, sizeof(upperFileName) - 1);
    upperFileName[11] = '\0';
    strtoupper(upperFileName);

    // Loop through our open files, make sure it is in there and set the offset
    for (int i = 0; i < 10; i++) {
        // If we have found the file we are looking for
        if (openFiles[i].isOpen && strcmp(openFiles[i].filename, upperFileName) == 0) {
            uint32_t fileSize = getFileSize(openFiles[i].fileCluster);

            // If the offset is too large, error
            if (offset > fileSize) {
                printf("Offset %u is larger than the size of the file '%s' (%u bytes).\n", offset, filename, fileSize);
                return -1;
            }

            // Otherwise, set the offset
            openFiles[i].offset = offset;
            printf("Offset of file '%s' set to %u bytes.\n", filename, offset);
            return 0; 
        }
    }

    // If we got this far, the file is not currently open
    printf("File '%s' is not open or does not exist in the directory.\n", filename);
    return -1;
}

#define min(a, b) ((a) < (b) ? (a) : (b))


// Function to read a certain amount of characters from a specified file
int read(char* filename, uint32_t size) {

    // Get the uppercase of the name for comparison to FAT32 files
    char upperFileName[12];
    strncpy(upperFileName, filename, sizeof(upperFileName) - 1);
    upperFileName[11] = '\0';
    strtoupper(upperFileName); 

    // Loop through our open files, make sure the file we want to read is in it
    for (int i = 0; i < 10; i++) {
        // Check if file is open and get the total size of the file
        if (openFiles[i].isOpen && strcmp(openFiles[i].filename, upperFileName) == 0) {
            // First check if the file has read access
            if (strchr(openFiles[i].mode, 'r') == NULL) {
                printf("File '%s' is not opened for read.\n", filename);
                return -1;
            }

            uint32_t fileSize = getFileSize(openFiles[i].fileCluster); 
            // If we cannot read any more 
            if (openFiles[i].offset >= fileSize) {
                printf("Read position is beyond the end of the file.\n");
                return -1;
            }

            // Create a buffer and determine the maximum readable size
            uint32_t readSize = min(size, fileSize - openFiles[i].offset); 
            uint8_t *buffer = calloc(1, readSize);
            if (!buffer) {
                printf("Unable to allocate memory for read buffer.\n");
                return -1;
            }

            uint32_t currentCluster = openFiles[i].fileCluster;
            uint32_t currentOffset = openFiles[i].offset;
            uint32_t clusterSize = bootSector.sectorsPerCluster * bootSector.bytesPerSector;
            uint32_t bytesLeft = readSize;
            uint32_t bytesRead = 0;

            // Read data until all requested bytes are read or end of file cluster chain is reached
            while (bytesLeft > 0 && currentCluster < 0x0FFFFFF8) {
                uint32_t clusterOffset = currentOffset % clusterSize;
                uint32_t effectiveClusterSize = clusterSize - clusterOffset;
                uint32_t bytesToRead = min(bytesLeft, effectiveClusterSize);
                uint32_t clusterAddress = getFirstSectorOfCluster(currentCluster) * bootSector.bytesPerSector + clusterOffset;

                fseek(imgFile, clusterAddress, SEEK_SET);
                fread(buffer + bytesRead, 1, bytesToRead, imgFile);

                bytesRead += bytesToRead;
                bytesLeft -= bytesToRead;
                currentOffset += bytesToRead;

                // Move to the next cluster if needed
                if (bytesLeft > 0) {
                    currentCluster = getNextCluster(currentCluster);
                    // Check for end of cluster chain, break if we reach the end
                    if (currentCluster < 2 || currentCluster >= 0x0FFFFFF8) break; 
                }
            }

            // Output the read data within the range of the buffer
            printf("%.*s", bytesRead, buffer); 
            printf("\n");
            free(buffer);

            // Update file offset after read and return success
            openFiles[i].offset += bytesRead; 
            return 0;
        }
    }

    // If we made it this far, the file could not be found
    printf("File '%s' is not found or not open for read.\n", filename);
    return -1;
}

// Function to write a string to a given file at the current offset
int write(char *filename, char *string) {

    // Get the uppercase of the name for comparison to FAT32 files
    char upperFileName[12];
    strncpy(upperFileName, filename, sizeof(upperFileName) - 1);
    upperFileName[11] = '\0';
    strtoupper(upperFileName);

    // Make sure the file is open and we can write to it
    int fileIndex = findOpenFile(upperFileName);
    if (fileIndex == -1) {
        printf("File '%s' is not open.\n", filename);
        return -1;
    }

    if (strchr(openFiles[fileIndex].mode, 'w') == NULL) {
        printf("File '%s' is not opened for writing.\n", filename);
        return -1;
    }

    uint32_t cluster = openFiles[fileIndex].fileCluster;
    uint32_t offset = openFiles[fileIndex].offset;
    uint32_t fileSize = getFileSize(cluster);
    uint32_t clusterSize = bootSector.sectorsPerCluster * bootSector.bytesPerSector;
    uint32_t writeSize = strlen(string);

    // Extend the size of the file if we need to
    if (offset + writeSize > fileSize) {
        printf("Extending file size...\n");
        // Extend the file to fit the new data
        if (!extendFileSize(&openFiles[fileIndex], offset + writeSize)) {
            printf("Unable to extend file size.\n");
            return -1;
        }
    }

    uint32_t bytesWritten = 0;

    // While we have not written all of the bytes we need to, keep writing
    while (bytesWritten < writeSize) {
        uint32_t clusterOffset = offset % clusterSize;
        uint32_t effectiveClusterSize = clusterSize - clusterOffset;
        uint32_t bytesToWrite = (writeSize - bytesWritten < effectiveClusterSize) ? writeSize - bytesWritten : effectiveClusterSize;
        uint32_t clusterAddress = getFirstSectorOfCluster(cluster) * bootSector.bytesPerSector + clusterOffset;

        fseek(imgFile, clusterAddress, SEEK_SET);
        fwrite(string + bytesWritten, 1, bytesToWrite, imgFile);

        bytesWritten += bytesToWrite;
        offset += bytesToWrite;

        // Update the file offset in the open file structure
        openFiles[fileIndex].offset = offset;

        // If we are not at the end of writing, see if we are at the end of the cluster
        if (bytesWritten < writeSize) {
            uint32_t nextCluster = getNextCluster(cluster);
            if (nextCluster < 2 || nextCluster >= 0x0FFFFFF8) { 
                printf("No additional clusters available.\n");
                return -1;
            }
            cluster = nextCluster;
        }
    }

    // Flush the file to make sure data was written to the disk and print success message
    fflush(imgFile); 
    printf("%s written to '%s'.\n", string, filename);

    return 0;
}

// Function to remove a file entry
int rm(const char *filename) {
    struct FAT32DirectoryEntry dirEntry;

    if (findOpenFile(filename) != -1) {
        printf("File '%s' is currently open and cannot be deleted.\n", filename);
        return -3;
    }

    if (findDirectoryEntry(filename, &dirEntry) != 0) {
        return -1;
    }

    if ((dirEntry.attributes & ATTR_DIRECTORY) == ATTR_DIRECTORY) {
        printf("'%s' is a directory, not a file.\n", filename);
        return -2;
    }

    // Proceed with deletion if it's a file
    char fat32Name[12];
    toFAT32Name(filename, fat32Name);
    removeDirectoryEntry(fat32Name);
    freeClusters((dirEntry.firstClusterHi << 16) | dirEntry.firstClusterLo);
    printf("File '%s' successfully deleted.\n", filename);
    return 0;
}

// Function to remove a directory entry
int rmdir(const char *dirname) {

    struct FAT32DirectoryEntry dirEntry;

    if (findDirectoryEntry(dirname, &dirEntry) != 0) {
        printf("Directory '%s' does not exist.\n", dirname);
        return -1;
    }

    if ((dirEntry.attributes & ATTR_DIRECTORY) != ATTR_DIRECTORY) {
        printf("'%s' is not a directory.\n", dirname);
        return -1;
    }

    // Check if the directory is empty
    uint32_t cluster = (dirEntry.firstClusterHi << 16) | dirEntry.firstClusterLo;
    if (!isDirectoryEmpty(cluster)) {
        printf("Directory '%s' is not empty.\n", dirname);
        return -1;
    }

    // Proceed with deletion
    char fat32Name[12];
    toFAT32Name(dirname, fat32Name);
    removeDirectoryEntry(fat32Name);
    freeClusters(cluster);
    printf("Directory '%s' successfully removed.\n", dirname);
    return 0;
}

// Rm -r recursive function to remove a directory and its contents
void rmr(const char *dirname) {
    struct FAT32DirectoryEntry dirEntry;

    // Check if the directory exists
    if (findDirectoryEntry(dirname, &dirEntry) != 0) {
        printf("Directory '%s' does not exist.\n", dirname);
        return;
    }

    if ((dirEntry.attributes & ATTR_DIRECTORY) != ATTR_DIRECTORY) {
        printf("'%s' is not a directory.\n", dirname);
        return;
    }

    // Get the starting cluster for the directory
    uint32_t cluster = (dirEntry.firstClusterHi << 16) | dirEntry.firstClusterLo;

    // Recursively delete all files and subdirectories
    deleteDirectoryContents(cluster);

    // After deleting contents, remove the directory itself
    char fat32Name[12];
    toFAT32Name(dirname, fat32Name);
    removeDirectoryEntry(fat32Name);
    freeClusters(cluster);

    printf("Directory '%s' removed successfully.\n", dirname);
}
