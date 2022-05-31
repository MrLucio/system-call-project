/// @file fifo.c
/// @brief Contiene l'implementazione delle funzioni
///         specifiche per la gestione delle FIFO.

#include "err_exit.h"
#include "fifo.h"
#include <unistd.h>
#include <sys/types.h>
#include <fcntl.h>
#include <sys/stat.h>


char *fname = "/tmp/myfifo";

void create_fifo(char *path){
    int fifo = mkfifo(path, S_IRUSR | S_IWUSR);
    if (fifo == -1)
        ErrExit("mkfifo failed");
}

int open_fifo(char *path, int flag){
    int fifo = open(path, flag);
    if (fifo == -1)
        ErrExit("open failed");
    return fifo;
}

void read_fifo(int fifo, void *buffer, size_t size){
    if (read(fifo, buffer, size) == -1)
        ErrExit("read failed");
}