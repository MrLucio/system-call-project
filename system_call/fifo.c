/// @file fifo.c
/// @brief Contiene l'implementazione delle funzioni
///         specifiche per la gestione delle FIFO.

#include "err_exit.h"
#include "fifo.h"
#include <unistd.h>
#include <sys/stat.h>


char *fname = "/tmp/myfifo";

void create_fifo(){
    //if(mkfifo(fname, S_IRUSR|S_IWUSR) == -1) ErrExit("make_fifo");
    mkfifo(fname, S_IRUSR|S_IWUSR);
}

int open_fifo(int flag){
    int fifo = open(fname, flag);
    if (fifo == -1) ErrExit("open_fifo");
    return fifo;
}