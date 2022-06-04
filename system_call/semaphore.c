/// @file semaphore.c
/// @brief Contiene l'implementazione delle funzioni
///         specifiche per la gestione dei SEMAFORI.

#include <sys/sem.h>

#include "err_exit.h"
#include "semaphore.h"
#include <stdio.h>

void semOp(int semid, unsigned short sem_num, short sem_op) {
    struct sembuf sop = {.sem_num = sem_num, .sem_op = sem_op, .sem_flg = 0};

    if (semop(semid, &sop, 1) == -1)
        ErrExit("semop failed");
}

void remove_semaphore(int semid) {
    if (semctl(semid, 0, IPC_RMID, 0) == -1)
        printf("sem already removed\n");
        //ErrExit("semctl 1 failed");
}