/// @file sender_manager.c
/// @brief Contiene l'implementazione del sender_manager.

#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/shm.h>
#include <sys/ipc.h>
#include <sys/stat.h>
#include "err_exit.h"
#include "defines.h"
#include "shared_memory.h"
#include "semaphore.h"
#include "fifo.h"
#include <linux/limits.h>

int main(int argc, char * argv[]) {
    //Apertura strutture dati
    mkfifo("/tmp/myfifo", S_IRUSR|S_IWUSR);
    int fifo = open("/tmp/myfifo", O_RDONLY);

    char cwd[PATH_MAX];
    if (getcwd(cwd, sizeof(cwd)) == NULL)
        ErrExit("getcwd");

    printf("%s %d\n", cwd, strlen(cwd));
    key_t key = ftok(cwd, 1);
    printf("key = %d\n", key);
    int shMemId = alloc_shared_memory(key, sizeof(int));
    printf("shid = %d\n", shMemId);
    //Ricezione numero file
    int numFile;
    char buffer_numFile[3];
    read(fifo, buffer_numFile, 3);
    numFile = atoi(buffer_numFile);
    //invio conferma ricezione
    int *shms = (int *)get_shared_memory(shMemId, 0);
    if (shms == (void *)-1)
        printf("first shmat failed\n");
    shms = 13;
    shmdt(shms);
    if (shmctl(shMemId, IPC_RMID, NULL) == -1)
        printf("shmctl failed");
    else
        printf("shared memory segment removed successfully\n");
    //*shms = "13";


    close(fifo);
    return 0;
}
