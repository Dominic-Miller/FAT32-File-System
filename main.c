#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include "fat32_structs.h"
#include "fat32_utils.h"

// ------------------------------------------------------------------------------------------------ //

// Global variables
struct FAT32BootSector bootSector;
FILE *imgFile = NULL;
uint32_t currentDirCluster;
struct OpenFile openFiles[10];

// ------------------------------------------------------------------------------------------------ //

// Shell function to display the current path and take in user input until "exit"
void shell(const char *imageName, const struct FAT32BootSector *bs) {
    char cmd[100];
    char path[256] = "/";

    // Initialize the current cluster
    currentDirCluster = bs->rootCluster;

    printf("%s%s> ", imageName, path);
    while (fgets(cmd, sizeof(cmd), stdin)) {
        // Remove newline character from cmd
        cmd[strcspn(cmd, "\n")] = 0;

        // Split command from potential arguments
        char *command = strtok(cmd, " ");
        char *argument = strtok(NULL, " ");
        char *remainingArguments = strtok(NULL, "");

        // Info command
        if (strcmp(command, "info") == 0) {
            printInfo(bs);
        }

        // Exit command
        else if (strcmp(command, "exit") == 0) {
            printf("Exiting...\n");
            break;
        }

        // Cd command
        else if (strcmp(command, "cd") == 0) {
            // If no directory given
            if (argument == NULL) {
                printf("No directory specified.\n");
            }
            // As long as not cd . 
            else if (strcmp(argument, ".") != 0) {
                // Call the cd command to get our new cluster
                int newCluster = cd(currentDirCluster, argument);
                if (newCluster != -1) {
                    currentDirCluster = newCluster;
                    // If cd ..
                    if (strcmp(argument, "..") == 0) {
                        // Handle path update
                        if (strcmp(path, "/") != 0) {
                            char *lastSlash = strrchr(path, '/');
                            if (lastSlash != NULL) {
                                if (lastSlash == path) {
                                    // If the only slash is at the beginning, ensure root path remains
                                    *(lastSlash + 1) = '\0';
                                } else {
                                    // If there's more than one slash, terminate the path at the last slash
                                    *lastSlash = '\0';
                                }
                            }
                        }
                    }
                    // For any other valid cd command, go to that directory
                    else {
                        // Update path, ensure no buffer overflow
                        if (strlen(path) + strlen(argument) < sizeof(path) - 2) {
                            if (strcmp(path, "/") != 0) strcat(path, "/");
                            strcat(path, argument);
                        }
                    }
                } 
                // Otherwise, the directory could not be found
                else {
                    if (newCluster == -1) {
                        printf("Directory %s does not exist.\n", argument);
                    }
                    else if (newCluster == -2) {
                        printf("%s is not a directory.\n", argument);
                    }
                }
            }
        }

        // Ls command
        else if (strcmp(command, "ls") == 0) {
            ls(currentDirCluster);
        }

        // Mkdir command
        else if (strcmp(command, "mkdir") == 0) {
            if (argument == NULL) {
                printf("No directory name specified.\n");
            } 
            else {
                mkdir(argument);
            }
        }

        // Creat command
        else if (strcmp(command, "creat") == 0) {
            if (argument == NULL) {
                printf("No file name specified.\n");
            } 
            else {
                creat(argument);
            }
        }

        // Open command
        else if(strcmp(command, "open") == 0) {
            if (argument == NULL) {
                printf("No file name specified.\n");
            } 
            else if (remainingArguments == NULL) {
                printf("No mode specified.\n");
            } 
            else {
                open(argument, remainingArguments);
            }

        }

        // Close command
        else if (strcmp(command, "close") == 0) {
            if (argument == NULL) {
                printf("No file name specified.\n");
            } 
            else {
                close(argument);
            }
        }

        // Lsof command
        else if (strcmp(command, "lsof") == 0) {
            lsof();
        }

        // Lseek command
        else if(strcmp(command, "lseek") == 0) {
            if (argument == NULL) {
                printf("No file name specified.\n");
            } 
            else if (remainingArguments == NULL) {
                printf("No offset specified.\n");
            } 
            else {
                uint32_t offset = convertToUint32(remainingArguments);
                lseek(argument, offset);
            }

        }

        // Read command
        else if(strcmp(command, "read") == 0){
            if (argument == NULL) {
                printf("No file name specified.\n");
            } 

            else if(remainingArguments == NULL){
                printf("No size specified.\n");
            }

            else{
                uint32_t size = convertToUint32(remainingArguments);
                read(argument,size);
            }
        }

        // Write command
        else if(strcmp(command, "write") == 0){
            if (argument == NULL) {
                printf("No file name specified.\n");
            } 

            else if(remainingArguments == NULL){
                printf("No string specified.\n");
            }

            else{
                write(argument, remainingArguments);
            }
        }


        // Rm and rm -r commands
        else if (strcmp(command, "rm") == 0) {
            if (argument == NULL) {
                printf("No file name specified.\n");
            } 
            // If rm -r
            else if (strcmp(argument, "-r") == 0) { 
                if (remainingArguments == NULL) {
                    printf("No file name specified.\n");
                }
                else {
                    rmr(remainingArguments);
                }
            }
            // If just rm
            else {
                int ret = rm(argument);
                if (ret == -1) {
                    printf("File '%s' does not exist.\n", argument);
                }
            }
        }

        // Rmdir command
        else if (strcmp(command, "rmdir") == 0) {
            if (argument == NULL) {
                printf("No directory name specified.\n");            
            } else {
                rmdir(argument);
            }
        }

        // Unknown command
        else {
            printf("Unknown command.\n");
        }

        // Print the image name and path after each input
        printf("%s%s> ", imageName, path);
    }
}

// ------------------------------------------------------------------------------------------------ // 

// Main function
int main(int argc, char *argv[]) {
    // Check if the code is being run properly with the fat32 image
    if (argc != 2) {
        printf("To run this program, try: ./code fat32.img\n");
        return 1;
    }

    // Open the fat32 image file and assign it to the global imgFile
    imgFile = fopen(argv[1], "r+");
    if (!imgFile) {
        printf("Error: File does not exist.\n");
        return 1;
    }

    fseek(imgFile, 0, SEEK_SET);

    // Load the boot sector into the global bootSector variable
    if (fread(&bootSector, sizeof(struct FAT32BootSector), 1, imgFile) != 1) {
        printf("Error reading boot sector: %s.\n", strerror(errno));
        fclose(imgFile);
        return 1;
    }

    // Activate the shell with the fat32 image for the remainder of the program
    shell(argv[1], &bootSector);

    // Close the file before exiting the program
    fclose(imgFile);
    return 0;
}