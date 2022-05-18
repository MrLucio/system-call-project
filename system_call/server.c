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
#include <sys/time.h>
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

void writeFile(t_message msgRcv[][MAX_PART]){
    int fileOpen;
    char channel[4][9] = {"FIFO1", "FIFO2", "MsgQueue", "ShdMem"};
    char header[PATH_MAX], path[PATH_MAX+4];
    for (int i = 0; i < numFile; i++){
        strcpy(path, msgRcv[i][0].path);
        strcat(path, "_out");
        fileOpen = open(path, O_WRONLY | O_CREAT, 0666);
        for (int j = 0; j < MAX_PART; j++){
            sprintf(header, "[Parte %d del file \"%s\", spedita dal processo %d tramite %s]\n", j + 1, msgRcv[i][j].path, msgRcv[i][j].pid, channel[j]);
            write(fileOpen, header, strlen(header));
            write(fileOpen, msgRcv[i][j].chunk, strlen(msgRcv[i][j].chunk));
            write(fileOpen, "\n", sizeof(char));
        }
        close(fileOpen);
    }
}

void initMsgRcv(t_message msgRcv[][MAX_PART]){
    for (int i = 0; i < numFile; i++){
        msgRcv[i][0].pid = -1;
    }
}

void addMsg(t_message msg, int num, t_message msgRcv[][MAX_PART], int numMsgRcv){
    //int find = 0;
    for (int i = 0; i < numFile; i++){
        if(msgRcv[i][0].pid == msg.pid){
            msgRcv[i][num] = msg;
            //find = 1;
            break;
        }else if (msgRcv[i][0].pid == -1){
                msgRcv[i][0].pid = msg.pid;
                msgRcv[i][num] = msg;
                break;
            }
    }
    /*if(!find){
        for (int i = 0; i < numFile; i++){
            if (msgRcv[i][0].pid == -1){
                msgRcv[i][0].pid = msg.pid;
                msgRcv[i][num] = msg;
                break;
            }
            
        }
        
    }*/
}

int main(int argc, char * argv[]) {
    if (signal(SIGINT, signalHandler) == SIG_ERR) ErrExit("set_signal");
    //Settaggio directory corrente
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
    key_t key_limits = ftok(cwd, 2);
    msQueId = msgget(key, IPC_CREAT | S_IRUSR | S_IWUSR);


    //Variabili ricezione numero file
    char buffer_numFile[3];
    //Variabili invio conferma ricezione
    int *shms;
    //Variabili ricezione dati
    int sem_msgShMem = semget(key, 1, IPC_CREAT | S_IRUSR | S_IWUSR);
    int sem_limits = semget(key_limits, 4, IPC_CREAT | S_IRUSR | S_IWUSR);
    semctl(sem_msgShMem, 0, SETVAL, 0);
    int numMsgRcv;
    t_message *shMem;
    t_message msg;
    t_messageQue msgQue;
    //Variabili cronometro
    struct timeval t1, t2;

    while (1){
        create_fifo("/tmp/fifo_1");
        printf("\nWaiting for Ctrl-C to end or new file to recive...\n");
        //Ricezione numero file
        fifo1 = open_fifo("/tmp/fifo_1", O_RDONLY);
        read(fifo1, buffer_numFile, 3);
        numFile = atoi(buffer_numFile);
        fifo1 = open_fifo("/tmp/fifo_1", O_RDONLY | O_NONBLOCK);
        //Invio conferma ricezione numero file
        shMemId = alloc_shared_memory(key, sizeof(t_message) * numFile);
        shms = (int *) get_shared_memory(shMemId, 0);
        *shms = numFile;
        semOp(sem_msgShMem, 0, 1);

        t_message msgRcv[numFile][MAX_PART];
        initMsgRcv(msgRcv);
        shMem = (t_message *)shms;
        numMsgRcv = 0;
        //Ricezione dati
        gettimeofday(&t1, NULL);
        while (numMsgRcv < numFile*4){
            //FIFO1
            if(read(fifo1, &msg, sizeof(msg)) > 0){
                semOp(sem_limits, 0, 1);
                numMsgRcv ++;
                printf("\nChannel: FIFO1; Num_msg: %d\n", numMsgRcv);
                addMsg(msg, 0, msgRcv, numMsgRcv/4);
            }
            //FIFO2
            if(read(fifo2, &msg, sizeof(msg)) > 0){
                semOp(sem_limits, 1, 1);
                numMsgRcv ++;
                printf("\nChannel: FIFO2; Num_msg: %d\n", numMsgRcv);
                addMsg(msg, 1, msgRcv, numMsgRcv/4);
            }
            //MessageQueue
            if(msgrcv(msQueId, &msgQue, sizeof(t_message), 1, IPC_NOWAIT) != -1){
                msg = msgQue.msg;
                semOp(sem_limits, 3, 1);
                numMsgRcv ++;
                printf("\nChannel: MsgQueue; Num_msg: %d\n", numMsgRcv);
                addMsg(msg, 2, msgRcv, numMsgRcv/4);
            }
            //SharedMemory
            if (semctl(sem_msgShMem, 0, GETVAL) > 0){
                msg = *shMem++;
                semOp(sem_limits, 2, 1);
                semOp(sem_msgShMem, 0, -1);
                numMsgRcv ++;
                printf("\nChannel: ShdMem; Num_msg: %d\n", numMsgRcv);
                addMsg(msg, 3, msgRcv, numMsgRcv/4);
            }
        
        }
        gettimeofday(&t2, NULL);
        //Invio segnale terminazione
        t_messageEnd endMsg = {200};
        msgsnd(msQueId, &endMsg, sizeof(t_messageEnd), 0);
        unlink("/tmp/fifo_1");
        remove_shared_memory(shMemId);
        printf("\nLa ricezione ha impiegato %.0f ms\n", ((t2.tv_sec - t1.tv_sec) * 1000.0) + ((t2.tv_usec - t1.tv_usec) / 1000.0));
        gettimeofday(&t1, NULL);
        writeFile(msgRcv);
        gettimeofday(&t2, NULL);
        printf("\nLa scrittura ha impiegato %.0f ms\n", ((t2.tv_sec - t1.tv_sec) * 1000.0) + ((t2.tv_usec - t1.tv_usec) / 1000.0));
        printf("\n$------------------------------------------------$\n");
    }
    
    return 0;
}
