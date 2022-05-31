/// @file shared_memory.c
/// @brief Contiene l'implementazione delle funzioni
///         specifiche per la gestione della MEMORIA CONDIVISA.

#include "err_exit.h"
#include "shared_memory.h"

#include <stdio.h>
#include <stdlib.h>
#include <sys/shm.h>
#include <sys/stat.h>

int alloc_shared_memory(key_t shmKey, size_t size) {
    // get, or create, a shared memory segment
    int shmid = shmget(shmKey, size, IPC_CREAT | S_IRUSR | S_IWUSR);
    if(shmid == -1)
        ErrExit("shmget failed");
    return shmid;
}

void *get_shared_memory(int shmid, int shmflg) {
    // attach the shared memory
    void *ptr = shmat(shmid, NULL, shmflg);
    if (ptr == (void *) -1)
        ErrExit("shmat failed");
    return ptr;
}

void free_shared_memory(void *ptr_sh) {
    // detach the shared memory segments
    if (shmdt(ptr_sh) == -1)
        ErrExit("shmdt failed");
}

void remove_shared_memory(int shmid) {
    // delete the shared memory segment
    if (shmctl(shmid, IPC_RMID, NULL) == -1)
        printf("shmem already removed\n");
        //ErrExit("shmctl failed");
}

int find_free_position(int *ptr_sh, int numFile) {
    for (int i = 0; i < numFile; i++)
        if (*(ptr_sh + i) == 1) 
            return i;
    printf("Error: free position not found");
    exit(1);
}

int find_taken_position(int *ptr_sh, int numFile) {
    for (int i = 0; i < numFile; i++)
        if (*(ptr_sh + i) == 0)
            return i;
    printf("Error: taken position not found");
    exit(1);
}