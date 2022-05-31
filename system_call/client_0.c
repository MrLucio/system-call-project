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
#include <sys/msg.h>

char *searchPath;
char *searchPrefix = "sendme_";
int pathsNum = 0;
char *paths[100];
int msQueueId;

void signalHandler(int sig) {
    printf("Ricevuto un SIGINT\n");
    exit(0);
}

int main(int argc, char * argv[]) {

    if (signal(SIGINT, signalHandler) == SIG_ERR)
        printf("Error\n");

    // pause();

    if (argc != 2) {
        printf("Error with parameters.\n");
        exit(1);
    }
    else {
        char cwd[PATH_MAX];
        if (getcwd(cwd, sizeof(cwd)) == NULL)
            ErrExit("getcwd");

        searchPath = argv[1];
        char * username = getenv("USER");
        if (chdir(searchPath) == -1)
            ErrExit("chdir");

        printf("Ciao %s, ora inizio l'invio dei contenuti in %s\n", username, searchPath);

        search(searchPath, searchPrefix, paths, &pathsNum);

        int fifo1 = open_fifo("/tmp/fifo_1", O_WRONLY);
        int fifo2 = open_fifo("/tmp/fifo_2", O_WRONLY);

        key_t key = ftok(cwd, 1);
        key_t key_limits = ftok(cwd, 2);
        key_t key_vector = ftok(cwd, 3);
        key_t key_mutex = ftok(cwd, 4);
        key_t key_shmem = ftok(cwd, 5);

        int shmem_counter_id = alloc_shared_memory(key_vector, sizeof(int) * pathsNum);
        int *shmem_counter = (int *) get_shared_memory(shmem_counter_id, 0);

        for (int i = 0; i < pathsNum; i++)
            *(shmem_counter + i) = 1;

        int sem_id = semget(IPC_PRIVATE, 1, IPC_CREAT | S_IRUSR | S_IWUSR);
        int sem_msgShMem = semget(key, 1, IPC_CREAT | S_IRUSR | S_IWUSR);
        int sem_limits = semget(key_limits, 4, IPC_CREAT | S_IRUSR | S_IWUSR);
        int mutex_ShMem = semget(key_mutex, 1, IPC_CREAT | S_IRUSR | S_IWUSR);
        int sem_shmem = semget(key_shmem, 1, IPC_CREAT | S_IRUSR | S_IWUSR);

        semctl(sem_id, 0, SETVAL, pathsNum);

        messageQueue_t msgQueue;
        msQueueId = msgget(key, IPC_CREAT | S_IRUSR | S_IWUSR);

        int shmid = alloc_shared_memory(key, sizeof(message_t) * (pathsNum > 50 ? 50 : pathsNum));
        int *shms = (int *) get_shared_memory(shmid, 0);

        char c[4] = {0};
        sprintf(c, "%d", pathsNum);

        write(fifo1, c, strlen(c));

        semOp(sem_msgShMem, 0, -1);
        
        if (*shms != pathsNum) {
            printf("Error with shmem confirmation %d %d\n", *shms, pathsNum);
            exit(1);
        }

        message_t *shmem = (message_t *) shms;

        for (int i = 0; i < pathsNum; i++) {
            int pid = fork();
            if (pid == 0) {
                int free_position;
                message_t messages[4];

                int fd = open(paths[i], O_RDONLY);
                off_t end_offset = lseek(fd, 0, SEEK_END);

                int position = 0;
                int window_size;
                for (int j = 4; j > 0; j--) {
                    lseek(fd, position, SEEK_SET);
                    window_size = (end_offset - position + j - 1) / j; // ceil division of (end_offset - position) with j
                    position += window_size;

                    messages[4 - j].pid = getpid();
                    strcpy(messages[4 - j].path, paths[i]);
                    
                    ssize_t num_read = read(fd, messages[4 - j].chunk, window_size);
                    if (num_read == -1)
                        ErrExit("read");
                    messages[4 - j].chunk[num_read] = '\0';
                }

                semOp(sem_id, 0, -1);
                semOp(sem_id, 0, 0);

                msgQueue.mtype = 1;
                msgQueue.msg = messages[MSGQUEUE];

                // Send message on FIFO1
                semOp(sem_limits, FIFO1, -1);
                write(fifo1, &messages[FIFO1], sizeof(message_t));

                // Send message on FIFO 2
                semOp(sem_limits, FIFO2, -1);
                write(fifo2, &messages[FIFO2], sizeof(message_t));

                // Send message 
                semOp(sem_limits, SHMEM, -1);
                semOp(mutex_ShMem, 0, -1);
                free_position = find_free_position(shmem_counter, pathsNum);
                shmem[free_position] = messages[SHMEM];
                shmem_counter[free_position] = 0;
                semOp(sem_shmem, 0, 1);
                semOp(mutex_ShMem, 0, 1);

                // Send message on Message Queue
                semOp(sem_limits, MSGQUEUE, -1);
                msgsnd(msQueueId, &msgQueue, sizeof(message_t), 0);

                exit(0);
            }
        }
        while (wait(NULL) != -1);

        close(fifo1);
        close(fifo2);

        long confirmation;
        msgrcv(msQueueId, &confirmation, 0, 200, 0);
        if (confirmation != 200)
            printf("Error at confirmation\n");

        //semOp(sem_msgShMem, 0, 1);
        /*if (semctl(sem_id, 0, IPC_RMID, 0) == -1)
            ErrExit("msgctl failed");*/

        free_shared_memory(shms);
    }

    return 0;

}
