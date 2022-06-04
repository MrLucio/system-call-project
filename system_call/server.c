/// @file sender_manager.c
/// @brief Contiene l'implementazione del sender_manager.

//#include <stdlib.h>
//#include <sys/shm.h>
//#include <sys/ipc.h>
//#include <sys/stat.h>
//#include <string.h>
//#include <linux/limits.h>
//#include <sys/sem.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <signal.h>
#include <sys/msg.h>
#include "err_exit.h"
#include "defines.h"
#include "message_queue.h"
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

void signalHandler(int sig);
void pathOutConcat(char *path, char *retBuf);
void write_files(message_t msgRcv[][MAX_PART]);
void initMsgRcv(message_t msgRcv[][MAX_PART]);
void addMsg(message_t msg, int num, message_t msgRcv[][MAX_PART]);

int main(int argc, char * argv[]) {
    //Set the signal handler function
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

    // Semaphore creation
    sem_msgShMem = semget(key, 1, IPC_CREAT | S_IRUSR | S_IWUSR); 
    sem_limits = semget(key_limits, 4, IPC_CREAT | S_IRUSR | S_IWUSR); //50 message limit semaphore
    mutex_ShMem = semget(key_mutex, 1, IPC_CREAT | S_IRUSR | S_IWUSR); //mutex access shared memory semaphore
    sem_shmem = semget(key_shmem, 1, IPC_CREAT | S_IRUSR | S_IWUSR);

    // Semaphore initialization
    union semun arg;
    arg.array = arr_limits;
    semctl(sem_msgShMem, 0, SETVAL, 0);
    semctl(mutex_ShMem, 0, SETVAL, 1);
    semctl(sem_shmem, 0, SETVAL, 0);
    if (semctl(sem_limits, 0, SETALL, arg) == -1)
        ErrExit("semctl failed");


    char buffer_numFile[3];     // Buffer for read the total number of file to recive
    int numMsgRcv;              // The number of the message recived
    int taken_position;
    message_t *shMem;           // Pointer to the shared memory to read the message
    message_t msg;              // Will contain the message recived

    while (1){

        // FIFO creation
        create_fifo("/tmp/fifo_1");
        create_fifo("/tmp/fifo_2");

        // FIFO opening
        fifo1 = open_fifo("/tmp/fifo_1", O_RDONLY);
        fifo2 = open_fifo("/tmp/fifo_2", O_RDONLY);

        // Reading the number of file from FIFO 1
        memset(buffer_numFile, '\0', 4);

        if (read(fifo1, buffer_numFile, 3) == -1)
            ErrExit("read number of file failed");

        close(fifo1);
        fifo1 = open_fifo("/tmp/fifo_1", O_RDONLY | O_NONBLOCK);

        numFile = atoi(buffer_numFile);
        if (numFile == 0)
            ErrExit("atoi number of file failed");

        // Shared memory initialization
        shmidMsg = alloc_shared_memory(key, sizeof(message_t) * (numFile > 50 ? 50 : numFile));
        bufferMsg = get_shared_memory(shmidMsg, 0);

        // Support vector initialization
        shmidSupport = alloc_shared_memory(key_vector, sizeof(int) * numFile);
        bufferSupport = (int *) get_shared_memory(shmidSupport, 0);

        // Message Queue creation
        msqid = get_message_queue(key);

        // Sending acknowledge to client
        *bufferMsg = numFile;
        semOp(sem_msgShMem, 0, 1);

        // Initialization variables for reciving file part
        message_t msgRcv[numFile][MAX_PART];
        initMsgRcv(msgRcv);
        shMem = (message_t *)bufferMsg;
        numMsgRcv = 0;
        taken_position = -1;

        // Reciving file part
        while (numMsgRcv < numFile * 4){
            //FIFO1
            if(read(fifo1, &msg, sizeof(message_t)) > 0){
                semOp(sem_limits, FIFO1, 1);
                numMsgRcv++;
                printf("\nChannel: FIFO1; Num_msg: %d\n", numMsgRcv);
                addMsg(msg, FIFO1, msgRcv);
            }
            //FIFO2
            if(read(fifo2, &msg, sizeof(message_t)) > 0){
                semOp(sem_limits, FIFO2, 1);
                numMsgRcv++;
                printf("\nChannel: FIFO2; Num_msg: %d\n", numMsgRcv);
                addMsg(msg, FIFO2, msgRcv);
            }
            //Message Queue
            if(read_message_queue(msqid, msg) != -1){
                semOp(sem_limits, MSGQUEUE, 1);
                numMsgRcv++;
                printf("\nChannel: MsgQueue; Num_msg: %d\n", numMsgRcv);
                addMsg(msg, MSGQUEUE, msgRcv);
            }
            //Shared Memory
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
                addMsg(msg, SHMEM, msgRcv);
            }
        
        }

        // Sending termination signal to client
        long confirmation = 200;
        msgsnd(msqid, &confirmation, 0, 0);

        // Closing and deleting FIFO1
        close(fifo1);
        unlink("/tmp/fifo_1");

        // Closing and detaching Shared memory and support vector
        free_shared_memory(shMem);
        remove_shared_memory(shmidMsg);
        free_shared_memory(bufferSupport);
        remove_shared_memory(shmidSupport);

        // Writing the message recived on file
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

// When Ctrl-C is pressed, all the ipc will close and the server process will terminate
void signalHandler(int sig) {
    printf("\nTerminazione programma\n");

    close(fifo1);
    close(fifo2);
    unlink("/tmp/fifo_1");
    unlink("/tmp/fifo_2");

    remove_shared_memory(shmidMsg);
    remove_shared_memory(shmidSupport);

    remove_message_queue(msqid);

    remove_semaphore(sem_msgShMem);
    remove_semaphore(sem_limits);
    remove_semaphore(mutex_ShMem);
    remove_semaphore(sem_shmem);

    exit(0);
}

// Concatenate the string _out to the input path, also checking if an extension is present
void pathOutConcat(char *path, char *retBuf){
    int find = 0;
    char strAppend[4] = "_out";
    for (int i = strlen(path), j = i+4; i >= 0; i--,j--){
        retBuf[j] = path[i];
        if (path[i] == '.' && !find){
            find = 1;
            for (int k = 3; k >= 0; k--){
                j--;
                retBuf[j] = strAppend[k];
            }
        }
    }
    if (!find){
        strcpy(retBuf, path);
        strcat(retBuf, strAppend);
    }
}

// Write all the message recived on the output file
void write_files(message_t msgRcv[][MAX_PART]){
    int fileOpen;
    char *channel[] = {"FIFO1", "FIFO2", "MsgQueue", "ShdMem"};
    char header[PATH_MAX], path[PATH_MAX+4];
    for (int i = 0; i < numFile; i++){
        pathOutConcat(msgRcv[i][0].path, path);
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

// Initialize the array that will contain the messages with pid = -1
void initMsgRcv(message_t msgRcv[][MAX_PART]){
    for (int i = 0; i < numFile; i++){
        msgRcv[i][0].pid = -1;
    }
}

// Add a message in the first free position of msgRcv array
void addMsg(message_t msg, int num, message_t msgRcv[][MAX_PART]){
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
