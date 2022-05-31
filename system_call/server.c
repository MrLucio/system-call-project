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

int fifo1;
int fifo2;
int sem_msgShMem;
int sem_limits;
int mutex_ShMem;
int sem_shmem;
int shmidMsg;
int *bufferMsg;
int shmidSupport;
int *bufferSupport;
int msqid;
int numFile;
unsigned short arr_limits[] = {50, 50, 50, 50};

void signalHandler(int sig) {
    printf("\nTerminazione programma\n");

    close(fifo1);
    close(fifo2);
    unlink("/tmp/fifo_1");
    unlink("/tmp/fifo_2");

    remove_shared_memory(shmidMsg);
    remove_shared_memory(shmidSupport);

    remove_semaphore(sem_msgShMem);
    remove_semaphore(sem_limits);
    remove_semaphore(mutex_ShMem);
    remove_semaphore(sem_shmem);

    if (msgctl(msqid, IPC_RMID, NULL) == -1)
        ErrExit("msgctl failed");

    exit(0);
}

void write_files(message_t msgRcv[][MAX_PART]){
    int fileOpen;
    char *channel[] = {"FIFO1", "FIFO2", "MsgQueue", "ShdMem"};
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

void initMsgRcv(message_t msgRcv[][MAX_PART]){
    for (int i = 0; i < numFile; i++){
        msgRcv[i][0].pid = -1;
    }
}

void addMsg(message_t msg, int num, message_t msgRcv[][MAX_PART], int numMsgRcv){
    for (int i = 0; i < numFile; i++){
        if(msgRcv[i][0].pid == msg.pid){
            msgRcv[i][num] = msg;
            break;
        }else if (msgRcv[i][0].pid == -1){
            msgRcv[i][0].pid = msg.pid;
            msgRcv[i][num] = msg;
            break;
        }
    }
}

int main(int argc, char * argv[]) {
    if (signal(SIGINT, signalHandler) == SIG_ERR)
        ErrExit("set_signal failed");
    
    // Get current working directory, used later for ftok()
    char cwd[PATH_MAX];
    if (getcwd(cwd, sizeof(cwd)) == NULL)
        ErrExit("getcwd failed");

    // Keys creation
    key_t key = ftok(cwd, 1);
    key_t key_limits = ftok(cwd, 2);
    key_t key_vector = ftok(cwd, 3);
    key_t key_mutex = ftok(cwd, 4);
    key_t key_shmem = ftok(cwd, 5);

    //Variabili ricezione numero file
    char buffer_numFile[3];
    //Variabili invio conferma ricezione
    
    //Variabili ricezione dati
    sem_msgShMem = semget(key, 1, IPC_CREAT | S_IRUSR | S_IWUSR);
    sem_limits = semget(key_limits, 4, IPC_CREAT | S_IRUSR | S_IWUSR);
    mutex_ShMem = semget(key_mutex, 1, IPC_CREAT | S_IRUSR | S_IWUSR);
    sem_shmem = semget(key_shmem, 1, IPC_CREAT | S_IRUSR | S_IWUSR);

    semctl(sem_msgShMem, 0, SETVAL, 0);
    semctl(mutex_ShMem, 0, SETVAL, 1);
    semctl(sem_shmem, 0, SETVAL, 0);

    union semun arg;
    arg.array = arr_limits;
    if (semctl(sem_limits, 0, SETALL, arg) == -1)
        ErrExit("semctl failed");

    int numMsgRcv;
    int taken_position;
    message_t *shMem;
    message_t msg;
    messageQueue_t msgQueue;

    while (1){

        // FIFO 1 and FIFO 2 openings
        create_fifo("/tmp/fifo_1");
        create_fifo("/tmp/fifo_2");

        fifo1 = open_fifo("/tmp/fifo_1", O_RDONLY);
        fifo2 = open_fifo("/tmp/fifo_2", O_RDONLY);

        memset(buffer_numFile, '\0', 4);

        if (read(fifo1, buffer_numFile, 3) == -1)
            ErrExit("read failed");

        close(fifo1);
        fifo1 = open_fifo("/tmp/fifo_1", O_RDONLY | O_NONBLOCK);

        numFile = atoi(buffer_numFile);
        if (numFile == 0)
            ErrExit("atoi failed");

        shmidMsg = alloc_shared_memory(key, sizeof(message_t) * (numFile > 50 ? 50 : numFile));

        bufferMsg = get_shared_memory(shmidMsg, 0);

        shmidSupport = alloc_shared_memory(key_vector, sizeof(int) * numFile);
        bufferSupport = (int *) get_shared_memory(shmidSupport, 0);

        // Message Queue creation
        msqid = msgget(key, IPC_CREAT | S_IRUSR | S_IWUSR);
        if (msqid == -1)
            ErrExit("msgget failed");

        //Invio conferma ricezione numero file
        *bufferMsg = numFile;
        semOp(sem_msgShMem, 0, 1);

        message_t msgRcv[numFile][MAX_PART];
        initMsgRcv(msgRcv);
        shMem = (message_t *)bufferMsg;
        numMsgRcv = 0;
        taken_position = -1;

        //Ricezione dati
        while (numMsgRcv < numFile * 4){
            //FIFO1
            if(read(fifo1, &msg, sizeof(message_t)) > 0){
                semOp(sem_limits, FIFO1, 1);
                numMsgRcv++;
                printf("\nChannel: FIFO1; Num_msg: %d\n", numMsgRcv);
                addMsg(msg, FIFO1, msgRcv, numMsgRcv / 4);
            }
            //FIFO2
            if(read(fifo2, &msg, sizeof(message_t)) > 0){
                semOp(sem_limits, FIFO2, 1);
                numMsgRcv++;
                printf("\nChannel: FIFO2; Num_msg: %d\n", numMsgRcv);
                addMsg(msg, FIFO2, msgRcv, numMsgRcv / 4);
            }
            //SharedMemory
            if (semctl(sem_shmem, 0, GETVAL) > 0){
                semOp(mutex_ShMem, 0, -1);
                taken_position = find_taken_position(bufferSupport, numFile);
                msg = shMem[taken_position];
                bufferSupport[taken_position] = 1;
                semOp(sem_shmem, 0, -1);
                semOp(mutex_ShMem, 0, 1);
                semOp(sem_limits, SHMEM, 1);
                numMsgRcv++;
                printf("\nChannel: ShdMem; Num_msg: %d\n", numMsgRcv);
                addMsg(msg, SHMEM, msgRcv, numMsgRcv / 4);
            }
            //MessageQueue
            if(msgrcv(msqid, &msgQueue, sizeof(message_t), 1, IPC_NOWAIT) != -1){
                msg = msgQueue.msg;
                semOp(sem_limits, MSGQUEUE, 1);
                numMsgRcv++;
                printf("\nChannel: MsgQueue; Num_msg: %d\n", numMsgRcv);
                addMsg(msg, MSGQUEUE, msgRcv, numMsgRcv / 4);
            }
        
        }

        //Invio segnale terminazione
        long confirmation = 200;
        msgsnd(msqid, &confirmation, 0, 0);

        close(fifo1);
        //close(fifo2);
        unlink("/tmp/fifo_1");
        //unlink("/tmp/fifo_2");

        free_shared_memory(shMem);
        remove_shared_memory(shmidMsg);
        free_shared_memory(bufferSupport);
        remove_shared_memory(shmidSupport);

        write_files(msgRcv);

        //semOp(sem_msgShMem, 0, -1);
        
        /*
        if (msgctl(msqid, IPC_RMID, NULL) == -1)
            ErrExit("msgctl failed");
            */

        printf("\nWaiting for Ctrl-C to end or new file to receive...\n");
    }
    
    return 0;
}
