/// @file sender_manager.c
/// @brief Contiene l'implementazione del sender_manager.

#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/shm.h>
#include <sys/ipc.h>
#include <sys/stat.h>
#include <string.h>
#include "err_exit.h"
#include "defines.h"
#include "shared_memory.h"
#include "semaphore.h"
#include "fifo.h"
#include <linux/limits.h>

void pathOutConcat(char *path, char *retBuf){
    for (int i = strlen(path), j = i+4; i >= 0; i--,j--){
        retBuf[j] = path[i];
        if (path[i] == '.'){
            retBuf[j-1] = 't';
            retBuf[j-2] = 'u';
            retBuf[j-3] = 'o';
            retBuf[j-4] = '_';
            j -= 4;
        }
    }
}

int main(int argc, char * argv[]) {
    //Apertura strutture dati
    create_fifo();
    int fifo = open_fifo(O_RDONLY);

    //mkfifo("/tmp/myfifo", S_IRUSR|S_IWUSR);
    //int fifo = open("/tmp/myfifo", O_RDONLY);

    char cwd[PATH_MAX];
    if (getcwd(cwd, sizeof(cwd)) == NULL)
        ErrExit("getcwd");

    printf("%s %ld\n", cwd, strlen(cwd));
    //Ricezione numero file
    int numFile;
    char buffer_numFile[3];
    read(fifo, buffer_numFile, 3);
    numFile = atoi(buffer_numFile);
    printf("Numero di file = %d\n", numFile);
    //invio conferma ricezione
    key_t key = ftok(cwd, 1);
    printf("key = %d\n", key);
    int shMemId = alloc_shared_memory(key, sizeof(t_message));
    printf("shid = %d\n", shMemId);
    int *shms = (int *)get_shared_memory(shMemId, 0);
    printf("shmSegemt = %p\n", shms);
    *shms = 1;
    printf("shmSegemtValue = %d\n", *shms);

    int fileOpen;
    t_message msg;
    char newPath[PATH_MAX];
    read_fifo(fifo, &msg, sizeof(msg));
    printf("pid = %d; path = %s; chunk = %s\n", msg.pid, msg.path, msg.chunk);
    pathOutConcat(msg.path, newPath);
    fileOpen = open(newPath, O_WRONLY | O_CREAT);
    //write(fileOpen, compFile->chunk, strlen(compFile->chunk));
    for (int i = 0; i < numFile; i++){
    }

    close(fileOpen);


    


    //pause();
    free_shared_memory(shms);
    remove_shared_memory(shMemId);
    close(fifo);
    return 0;
}
