/// @file defines.h
/// @brief Contiene la definizioni di variabili
///         e funzioni specifiche del progetto.

#pragma once

#include <linux/limits.h>
#include <stdlib.h>
#include <dirent.h>
#include <sys/stat.h>
#include <string.h>
#include "err_exit.h"

#define PATH_MAX_ELAB 150
#define CHUNK_MAX_ELAB 1025
#define MAX_PART 4
#define FIFO1 0
#define FIFO2 1
#define SHMEM 2
#define MSGQUEUE 3

typedef struct message{
    int pid;
    char path[PATH_MAX_ELAB];
    char chunk[CHUNK_MAX_ELAB];
} message_t;

typedef struct messageEnd{
    long mtype;
} messageEnd_t;

size_t append2Path(char *directory, char *searchPath);
int checkFileSize(char *pathname, off_t size);
void search(char *searchPath, char *searchPrefix, char **paths, int *pathsNum);
int indexOf(int *ptr, int length, int value);
