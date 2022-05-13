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
#include "err_exit.h"
#include "defines.h"
#include "shared_memory.h"
#include "semaphore.h"
#include "fifo.h"

int fifo1;
int fifo2;
int shMemId;
int msQueId;
int numFile;


void pathOutConcat(char *path, char *retBuf){
    int point = 0;
    for (int i = strlen(path), j = i+4; i >= 0; i--,j--){
        retBuf[j] = path[i];
        if (path[i] == '.'){
            point = 1;
            retBuf[j-1] = 't';
            retBuf[j-2] = 'u';
            retBuf[j-3] = 'o';
            retBuf[j-4] = '_';
            j -= 4;
        }
    }
    if(!point){
        strcat(retBuf, "_out");
    }
}

void creatHeader(char header[], char n, char path[], char pid[], char channel[]){
    strcat(strcat(strcat(strcat(strcat(strcat(strcat(strcat(header, &n), " del file \""), path), "\", spedita dal processo "), pid), " tramite "), channel), "]\n");
}

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
    printf("current wd = %s\n", cwd);

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
    printf("msQueId = %d\n", msQueId);


    //Variabili ricezione numero file
    char buffer_numFile[3];
    //Variabili invio conferma ricezione
    int *shms;
    //Variabili ricezione dati
    int newMsg, msgReciv = 0;
    t_message *shMem;
    t_message msg, empty = {-1};
    t_messageQue msgQue;
    //Variabili scrittura dati
    int fileOpen;
    char header[PATH_MAX], num, pidStr[5], channel[9], newPath[PATH_MAX];

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
        printf("shid = %d\n", shMemId);
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
                //printf("FIFO1:pid = %d; path = %s; chunk = %s\n", msg.pid, msg.path, msg.chunk);
                newMsg = 1;
                num = '1';
                sprintf(channel, "FIFO1");
            }
            //FIFO2
            else if(read(fifo2, &msg, sizeof(msg)) > 0){
                //printf("FIFO2: pid = %d; path = %s; chunk = %s\n", msg.pid, msg.path, msg.chunk);
                newMsg = 1;
                num = '2';
                sprintf(channel, "FIFO2");
            }
            //SharedMemory
            else if (chkSharedMemory(shMem) != (t_message *)-1){
                msg = *chkSharedMemory(shMem);
                //printf("ShMem: pid = %d; path = %s; chunk = %s\n", msg.pid, msg.path, msg.chunk);
                newMsg = 1;
                num = '3';
                sprintf(channel, "ShdMem");
                *chkSharedMemory(shMem) = empty;
            }
            //MessageQueue
            else if(msgrcv(msQueId, &msgQue, sizeof(t_message), 1, IPC_NOWAIT) != -1){
                msg = msgQue.msg;
                //printf("MsgQueue: pid = %d; path = %s; chunk = %s\n", msg.pid, msg.path, msg.chunk);
                newMsg = 1;
                num = '4';
                sprintf(channel, "MsgQueue");
            }
            //Creazione header
            if (newMsg){
                sprintf(header, "\n[Parte ");
                sprintf(pidStr, "%d", msg.pid);
                creatHeader(header, num, msg.path, pidStr, channel);
                printf("%s", header);
                //Scrittura su file
                pathOutConcat(msg.path, newPath);
                fileOpen = open(newPath, O_WRONLY | O_CREAT | O_APPEND, 0666);
                write(fileOpen, header, strlen(header));
                write(fileOpen, msg.chunk, strlen(msg.chunk));
                close(fileOpen);

                msgReciv ++;
                newMsg = 0;
                printf("mess_ricevuti = %d\n\n", msgReciv);
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
