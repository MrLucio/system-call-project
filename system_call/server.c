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
    fifo2 = open_fifo("/tmp/fifo_2", O_RDONLY);
    //Shared_Memory
    key_t key = ftok(cwd, 1);
    printf("key = %d\n", key);
    shMemId = alloc_shared_memory(key, sizeof(t_message));
    printf("shid = %d\n", shMemId);
    //Message_Queue
    msQueId = msgget(key, IPC_CREAT | S_IRUSR | S_IWUSR);
    printf("msQueId = %d\n", msQueId);


    //Variabili ricezione numero file
    int numFile;
    char buffer_numFile[3];
    //Variabili invio conferma ricezione
    int *shms;
    //Variabili ricezione dati
    t_message msg;
    //Variabili scrittura dati
    int fileOpen;
    char header[PATH_MAX], itoch, pidStr[5], channel[9], newPath[PATH_MAX];

    while (1){
        //Ricezione numero file
        read(fifo1, buffer_numFile, 3);
        numFile = atoi(buffer_numFile);
        printf("Numero di file = %d\n", numFile);
        //Invio conferma ricezione numero file
        shms = (int *)get_shared_memory(shMemId, 0);
        *shms = 1;
        free_shared_memory(shms);

        //Ricezione dati
        read_fifo(fifo1, &msg, sizeof(msg));
        printf("pid = %d; path = %s; chunk = %s\n", msg.pid, msg.path, msg.chunk);
        //Creazione header
        sprintf(header, "\n[Parte ");
        itoch = '1';
        sprintf(pidStr, "%d", msg.pid);
        sprintf(channel, "FIFO1");
        creatHeader(header, itoch, msg.path, pidStr, channel);
        printf("head = %s\n", header);
        //Scrittura su file
        pathOutConcat(msg.path, newPath);
        fileOpen = open(newPath, O_WRONLY | O_CREAT, 0666);
        write(fileOpen, header, strlen(header));
        write(fileOpen, msg.chunk, strlen(msg.chunk));
        close(fileOpen);

        //Invio segnale terminazione
        printf("Inviato segnale sulla message_queue di terminazione\n");
        printf("Waiting for Ctrl-C to end...\n");
        t_messageEnd endMsg = {200};
        msgsnd(msQueId, &endMsg, sizeof(t_messageEnd), 0);
        pause();
    }
    
    return 0;
}
