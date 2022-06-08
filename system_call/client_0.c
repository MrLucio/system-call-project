/// @file client.c
/// @brief Contiene l'implementazione del client.

#include "defines.h"
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <linux/limits.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/sem.h>
#include <sys/wait.h>
#include "err_exit.h"
#include "shared_memory.h"
#include "fifo.h"
#include "semaphore.h"
#include "message_queue.h"
#include <sys/msg.h>

int sem_childs;
int sem_ack;
int sem_limits;
int mutex_shmem;
int sem_shmem;
char *searchPath;
char *searchPrefix = "sendme_";
int numFile;
char *paths[100];
int msqid;
int fifo1, fifo2;

void signalHandler(int sig) {
    if (sig == SIGUSR1) {
        // Close internal semaphore
        if (semctl(sem_childs, 0, IPC_RMID, 0) == -1)
            ErrExit("msgctl failed");
        printf("\nTerminazione programma\n");
        exit(0);
    }
}

int main(int argc, char * argv[]) {
    if (argc != 2) {
        printf("Error with parameters.\n");
        exit(1);
    }
    else {
        sigset_t mySet;

        // Initialize mySet to contain all signals
        sigfillset(&mySet);

        // Set SIGUSR1 and SIGINT signal handler
        if (signal(SIGUSR1, signalHandler) == SIG_ERR || signal(SIGINT, signalHandler) == SIG_ERR)
            ErrExit("signal handle change failed\n");

        // Get current working directory, used later for ftok()
        char cwd[PATH_MAX];
        if (getcwd(cwd, sizeof(cwd)) == NULL)
            ErrExit("getcwd");

        // Retrieve current user's name
        char * username = getenv("USER");

        // Set new directory based on the argument passed
        searchPath = argv[1];
        if (chdir(searchPath) == -1)
            ErrExit("chdir");

        // Keys creation
        key_t key = ftok(cwd, 1);
        key_t key_limits = ftok(cwd, 2);
        key_t key_vector = ftok(cwd, 3);
        key_t key_mutex = ftok(cwd, 4);
        key_t key_shmem = ftok(cwd, 5);

        // Semaphores creation
        sem_childs = semget(IPC_PRIVATE, 1, IPC_CREAT | S_IRUSR | S_IWUSR);
        sem_ack = semget(key, 1, IPC_CREAT | S_IRUSR | S_IWUSR);
        sem_limits = semget(key_limits, 4, IPC_CREAT | S_IRUSR | S_IWUSR);
        mutex_shmem = semget(key_mutex, 1, IPC_CREAT | S_IRUSR | S_IWUSR);
        sem_shmem = semget(key_shmem, 1, IPC_CREAT | S_IRUSR | S_IWUSR);

        while(1) {
            // Remove SIGUSR1 and SIGINT from mySet
            sigdelset(&mySet, SIGUSR1);
            sigdelset(&mySet, SIGINT);

            // Blocking all signals but SIGUSR1 and SIGINT
            if (sigprocmask(SIG_SETMASK, &mySet, NULL) == -1)
                ErrExit("sigprocmask failed");

            pause();

            // Add SIGUSR1 and SIGINT to mySet
            sigaddset(&mySet, SIGUSR1);
            sigaddset(&mySet, SIGINT);

            // Blocking all signals
            if (sigprocmask(SIG_SETMASK, &mySet, NULL) == -1)
                ErrExit("sigprocmask failed");

            printf("Ciao %s, ora inizio l'invio dei contenuti in %s\n", username, searchPath);

            numFile = 0;

            // Search recursively for sendme files in the current directory
            search(searchPath, searchPrefix, paths, &numFile);

            // Open FIFO1 and FIFO2 initialized by the server
            fifo1 = open_fifo("/tmp/fifo_1", O_WRONLY);
            fifo2 = open_fifo("/tmp/fifo_2", O_WRONLY);

            int shmem_vector_id = alloc_shared_memory(key_vector, sizeof(int) * numFile);
            int *shmem_vector = (int *) get_shared_memory(shmem_vector_id, 0);

            for (int i = 0; i < numFile; i++)
                shmem_vector[i] = 1;

            semctl(sem_childs, 0, SETVAL, numFile);

            messageQueue_t msgQueue;
            msqid = get_message_queue(key);

            int shmid = alloc_shared_memory(key, sizeof(message_t) * (numFile > 50 ? 50 : numFile));
            int *shmbuf = (int *) get_shared_memory(shmid, 0);

            // Initialize string that will hold the number of files,
            // 3 chars will be used to represent a number of files
            // from 0 to 999 while the last char is the string terminator
            char pathsChar[4] = {0};
            // Convert integer representing the number of files to a string
            sprintf(pathsChar, "%d", numFile);

            // Write number of files to FIFO1
            write(fifo1, pathsChar, strlen(pathsChar));

            // 
            semOp(sem_ack, 0, -1);

            // Check if the confirmation sent by the server is valid
            if (*shmbuf != numFile) {
                printf("Error with shmem confirmation %d %d\n", *shmbuf, numFile);
                exit(1);
            }

            message_t *shmem = (message_t *) shmbuf;

            // Cycle all files
            for (int i = 0; i < numFile; i++) {

                // Fork current process for each file
                int pid = fork();

                // Branch executed only by a child
                if (pid == 0) {
                    // Variable used to identify a writable position on the shared memory
                    int free_position;

                    // Array of messages that will be sent to the server
                    message_t messages[4];

                    // Open file and get its size
                    int fd = open(paths[i], O_RDONLY);
                    off_t end_offset = lseek(fd, 0, SEEK_END);

                    int position = 0;
                    int window_size;
                    for (int j = 4; j > 0; j--) {

                        /*
                            Move to the last position read and
                            calculate a new window size (the next number
                            of bytes to read from the file)
                        */
                        lseek(fd, position, SEEK_SET);
                        window_size = (end_offset - position + j - 1) / j; // ceil division of (end_offset - position) with j
                        position += window_size;

                        // Store PID and path of the file into the message
                        messages[4 - j].pid = getpid();
                        strcpy(messages[4 - j].path, paths[i]);

                        // Fetch and store the desired chunk of the file into a message
                        ssize_t num_read = read(fd, messages[4 - j].chunk, window_size);
                        if (num_read == -1)
                            ErrExit("read");

                        // Append terminator char to the message chunk
                        messages[4 - j].chunk[num_read] = '\0';
                    }

                    // Decrement value of the semaphore
                    semOp(sem_childs, 0, -1);
                    // Wait for the semaphore to reach the value 0
                    semOp(sem_childs, 0, 0);

                    // Initialize the message that will be sent on the message
                    // queue that requires a particular structure (with an mype)
                    msgQueue.mtype = 1;
                    msgQueue.msg = messages[MSGQUEUE];

                    // Array of four booleans to indicate whether a message has been
                    // successfully sent to the corresponding IPC
                    int status[4] = {1, 1, 1, 1};

                    // Cycle while there's still a TRUE boolean value
                    while(indexOf(status, 4, 1) != -1) {

                        // Check if the message hasn't already been sent on FIFO 1
                        // and the IPC is not full (50 connections reached)
                        if (status[FIFO1] && semctl(sem_limits, FIFO1, GETVAL) != 0) {
                            semOp(sem_limits, FIFO1, -1);

                            // Write message on FIFO 1
                            write(fifo1, &messages[FIFO1], sizeof(message_t));
                            
                            // Mark FIFO1 IPC as done
                            status[FIFO1] = 0;
                        }

                        // Check if the message hasn't already been sent on FIFO 2
                        // and the IPC is not full (50 connections reached)
                        if (status[FIFO2] && semctl(sem_limits, FIFO2, GETVAL) != 0) {
                            semOp(sem_limits, FIFO2, -1);

                            // Write message on FIFO 2
                            write(fifo2, &messages[FIFO2], sizeof(message_t));

                            // Mark FIFO2 IPC as done
                            status[FIFO2] = 0;
                        }

                        // Check if the message hasn't already been sent on MSGQUEUE
                        // and the IPC is not full (50 connections reached)
                        if (status[MSGQUEUE] && semctl(sem_limits, MSGQUEUE, GETVAL) != 0) {
                            semOp(sem_limits, MSGQUEUE, -1);

                            // Send message on message queue
                            msgsnd(msqid, &msgQueue, sizeof(message_t), 0);

                            // Mark MSGQUEUE IPC as done
                            status[MSGQUEUE] = 0;
                        }


                        // Check if the message hasn't already been sent on SHMEM
                        // and the IPC is not full (50 connections reached)
                        if (status[SHMEM] && semctl(sem_limits, SHMEM, GETVAL) != 0) {
                            semOp(sem_limits, SHMEM, -1);

                            // Try to acquire the mutex
                            semOp(mutex_shmem, 0, -1);

                            // Find the first free index of the shmem using the support vector
                            free_position = indexOf(shmem_vector, numFile, 1);
                            // Write the message to the desired index
                            memcpy(&shmem[free_position], &messages[SHMEM], sizeof(message_t));
                            // Mark the position as not free on the support vector
                            shmem_vector[free_position] = 0;

                            // Release the mutex
                            semOp(mutex_shmem, 0, 1);
                            // Increase counter of messages available on shmem
                            semOp(sem_shmem, 0, 1);

                            // Mark SHMEM IPC as done
                            status[SHMEM] = 0;
                        }

                    }

                    // Exit without errors
                    exit(0);
                }
            }
            // Make parent wait for all childs to finish their execution
            while (wait(NULL) != -1);

            // Close FIFO1 and FIFO2 file descriptors
            close(fifo1);
            close(fifo2);

            // Receive message confirmation, only an mtype (long) is needed
            long confirmation;
            msgrcv(msqid, &confirmation, 0, 200, 0);
            // Check if confirmation is valid
            if (confirmation != 200)
                printf("Error at confirmation\n");

            // Increment value of semaphore that will wake up
            // the server, notifying that confirmation has been received
            semOp(sem_ack, 0, 1);

            // Free all allocated memory of the current execution
            for (int i = 0; i < numFile; i++)
                free(paths[i]);

            // Detach all the shared memory used during the execution
            free_shared_memory(shmbuf);
            free_shared_memory(shmem_vector);
        }
    }

    return 0;

}
