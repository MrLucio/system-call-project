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

int main(int argc, char * argv[]) {
    //Apertura strutture dati
    //create_fifo();
    //int fifo = open_fifo(O_RDONLY);

    mkfifo("/tmp/myfifo", S_IRUSR|S_IWUSR);
    int fifo = open("/tmp/myfifo", O_RDONLY);

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

    char buffer_compFile[27];

    printf("fifo = %d\n", fifo);

    //int fileOpen;
    t_message msg;
    int len;
    char s[13];
    if (read(fifo, &len, sizeof(int)) == -1) {
        ErrExit("aiuto");
    }
    if (read(fifo, s, 13) == -1) {
        ErrExit("aiuto");
    }
    //read(fifo, buffer_compFile, 27);
    //printf("pid = %d; path = %s; dati= %s\n", msg.pid, *msg.path_file, *msg.data);
    printf("pid = %s, len = %d\n", s, len);
    //fileOpen = open(strcat(compFile->path_file ,"_out"), O_WRONLY | O_EXCL);
    //write(fileOpen, compFile->data, strlen(compFile->data));
    for (int i = 0; i < numFile; i++){
    }

    //close(fileOpen);


    


    pause();
    free_shared_memory(shms);
    remove_shared_memory(shMemId);
    close(fifo);
    return 0;
}
