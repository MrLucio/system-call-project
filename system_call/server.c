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
#include <signal.h>
#include <linux/limits.h>
#include <sys/msg.h>
#include <sys/sem.h>
#include "err_exit.h"
#include "defines.h"
#include "shared_memory.h"
#include "semaphore.h"
#include "fifo.h"

#ifndef SEMUN_H
#define SEMUN_H
#include <sys/sem.h>
// definition of the union semun
union semun {
    int val;
    struct semid_ds * buf;
    unsigned short * array;
};
#endif

int fifo1;
int fifo2;
int shMemId;
int msQueId;
int numFile;

void signalHandler(int sig) {
    printf("\nTerminazione programma\n");
    close(fifo1);
    close(fifo2);
    remove_shared_memory(shMemId);
    msgctl(msQueId, IPC_RMID, NULL);
    exit(0);
}

t_message* chkSharedMemory(t_message *shMem){
    for (int i = 0; i < numFile; i++){
        if (shMem[i].pid != -1){
            return &shMem[i];
        }
    }
    return (t_message *)-1;
}

int main(int argc, char * argv[]) {
    if (signal(SIGINT, signalHandler) == SIG_ERR) ErrExit("set_signal");
    //get current directory
    char cwd[PATH_MAX];
    if (getcwd(cwd, sizeof(cwd)) == NULL) ErrExit("getcwd");

    //Apertura strutture dati
    //fifo_1
    create_fifo("/tmp/fifo_1");
    fifo1 = open_fifo("/tmp/fifo_1", O_RDONLY);
    //fifo_2
    create_fifo("/tmp/fifo_2");
    fifo2 = open_fifo("/tmp/fifo_2", O_RDONLY | O_NONBLOCK);
    //Message_Queue
    key_t key = ftok(cwd, 1);
    msQueId = msgget(key, IPC_CREAT | S_IRUSR | S_IWUSR);


    //Variabili ricezione numero file
    char buffer_numFile[3];
    //Variabili invio conferma ricezione
    int *shms;
    //Variabili ricezione dati
    union semun args;
    args.val = 0;
    int sem_msgShMem = semget(key, 1, IPC_CREAT | S_IRUSR | S_IWUSR);
    semctl(sem_msgShMem, 0, SETVAL, args);
    struct sembuf sem_p = {0, -1, 0};
    int newMsg, msgReciv = 0;
    t_message *shMem;
    t_message msg, empty = {-1};
    t_messageQue msgQue;
    //Variabili scrittura dati
    int fileOpen, num;
    char header[PATH_MAX], channel[9];

    printf("sem: %d\n", semctl(sem_msgShMem, 0, GETVAL));
    while (1){
        //Ricezione numero file
        read(fifo1, buffer_numFile, 3);
        numFile = atoi(buffer_numFile);
        printf("Numero di file = %d\n", numFile);
        fifo1 = open_fifo("/tmp/fifo_1", O_RDONLY | O_NONBLOCK);
        //Invio conferma ricezione numero file
        //Shared_Memory
        int size = sizeof(t_message)*numFile;
        shMemId = alloc_shared_memory(key, size);
        shms = (int *)get_shared_memory(shMemId, 0);
        *shms = 1;
        free_shared_memory(shms);

        shMem = (t_message *)get_shared_memory(shMemId, 0);
        newMsg = 0;
        sleep(1);
        //Ricezione dati
        while (msgReciv < numFile*4){
            //FIFO1
            if(read(fifo1, &msg, sizeof(msg)) > 0){
                newMsg = 1;
                num = 1;
                strcpy(channel, "FIFO1");
            }
            //FIFO2
            else if(read(fifo2, &msg, sizeof(msg)) > 0){
                newMsg = 1;
                num = 2;
                strcpy(channel, "FIFO2");
            }
            //SharedMemory
            else if (semctl(sem_msgShMem, 0, GETVAL) > 0){
                printf("sem: %d\n", semctl(sem_msgShMem, 0, GETVAL));
                msg = *chkSharedMemory(shMem);
                newMsg = 1;
                num = 3;
                strcpy(channel, "ShdMem");
                *chkSharedMemory(shMem) = empty;
                semop(sem_msgShMem, &sem_p, 1);
            }
            //MessageQueue
            else if(msgrcv(msQueId, &msgQue, sizeof(t_message), 1, IPC_NOWAIT) != -1){
                msg = msgQue.msg;
                newMsg = 1;
                num = 4;
                strcpy(channel, "MsgQueue");
            }
            //Creazione header
            if (newMsg){
                sprintf(header, "[Parte %d del file \"%s\", spedita dal processo %d tramite %s]\n", num, msg.path, msg.pid, channel);
                //Scrittura su file
                strcat(msg.path, "_out");
                fileOpen = open(msg.path, O_WRONLY | O_CREAT | O_APPEND, 0666);
                write(fileOpen, header, strlen(header));
                write(fileOpen, msg.chunk, strlen(msg.chunk));
                write(fileOpen, "\n", sizeof(char));
                close(fileOpen);

                msgReciv ++;
                newMsg = 0;
                printf("\nchannel= %s; mess_ricevuti = %d\n", channel, msgReciv);
            }
        }



        //Invio segnale terminazione
        printf("Inviato segnale sulla message_queue di terminazione\n");
        printf("Waiting for Ctrl-C to end...\n");
        t_messageEnd endMsg = {200};
        msgsnd(msQueId, &endMsg, sizeof(t_messageEnd), 0);
        pause();
    }
    
    return 0;
}
