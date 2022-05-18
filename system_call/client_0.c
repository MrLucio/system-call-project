/// @file client.c
/// @brief Contiene l'implementazione del client.

#include "defines.h"
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <stdlib.h>
#include <linux/limits.h>
#include <dirent.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/sem.h>
#include <sys/wait.h>
#include <math.h>
#include "err_exit.h"
#include "shared_memory.h"
#include "fifo.h"
#include "semaphore.h"
#include <sys/msg.h>

char *searchPath;
char *searchPrefix = "sendme_";
int pathsNum = 0;
char *paths[100];
int msQueId;

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

size_t append2Path(char *directory) {
    size_t lastPathEnd = strlen(searchPath);
    // extends current searchPath: searchPath + / + directory
    strcat(strcat(&searchPath[lastPathEnd], "/"), directory);
    return lastPathEnd;
}

int checkFileSize(char *pathname, off_t size) {
    if (pathname == NULL)
        return 0;

    struct stat statbuf;
    if (stat(pathname, &statbuf) == -1)
        return 0;

    return statbuf.st_size >= size;
}

void search() {
    // open the current searchPath
    DIR *dirp = opendir(searchPath);
    if (dirp == NULL) return;
    // readdir returns NULL when end-of-directory is reached.
    // In oder to get when an error occurres, we set errno to zero, and the we
    // call readdir. If readdir returns NULL, and errno is different from zero,
    // an error must have occurred.
    int errno = 0;
    // iter. until NULL is returned as a result
    struct dirent *dentry;
    while ( (dentry = readdir(dirp)) != NULL) {
        if (pathsNum == 100) return;
        // Skip . and ..
        if (strcmp(dentry->d_name, ".") == 0 ||
            strcmp(dentry->d_name, "..") == 0)
        {  continue;  }

        // is the current dentry a regular file?
        if (dentry->d_type == DT_REG) {
            // exetend current searchPath with the file name
            size_t lastPath = append2Path(dentry->d_name);

            if (strncmp(dentry->d_name, searchPrefix, strlen(searchPrefix)) == 0
                && !checkFileSize(searchPath, 4096))
            {
                paths[pathsNum] = (char *)(malloc(strlen(searchPath)));
                strcpy(paths[pathsNum++], searchPath);
            }

            // reset current searchPath
            searchPath[lastPath] = '\0';
        // is the current dentry a directory
        } else if (dentry->d_type == DT_DIR) {
            // exetend current searchPath with the directory name
            size_t lastPath = append2Path(dentry->d_name);
            // call search method
            search();
            // reset current searchPath
            searchPath[lastPath] = '\0';
        }
        errno = 0;
    }

    if (errno != 0)
        ErrExit("readdir failed");

    if (closedir(dirp) == -1)
        ErrExit("closedir failed");
}

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
        return 1;
    }
    else {
        char cwd[PATH_MAX];
        if (getcwd(cwd, sizeof(cwd)) == NULL)
            ErrExit("getcwd");

        searchPath = argv[1];
        char * username = getenv("USER");
        chdir(searchPath);
        printf("Ciao %s, ora inizio l'invio dei contenuti in %s\n", username, searchPath);

        search();

        create_fifo("/tmp/fifo_1");
        int fifo1 = open_fifo("/tmp/fifo_1", O_WRONLY);

        create_fifo("/tmp/fifo_2");
        int fifo2 = open_fifo("/tmp/fifo_2", O_WRONLY);

        char c[4] = {0};
        sprintf(c, "%d", pathsNum);

        write(fifo1, c, strlen(c));
        
        key_t key = ftok(cwd, 1);
        key_t key_limits = ftok(cwd, 2);

        int sem_id = semget(IPC_PRIVATE, 1, IPC_CREAT | S_IRUSR | S_IWUSR);
        int sem_msgShMem = semget(key, 1, IPC_CREAT | S_IRUSR | S_IWUSR);
        int mutex_ShMem = semget(IPC_PRIVATE, 1, IPC_CREAT | S_IRUSR | S_IWUSR);
        int sem_limits = semget(key_limits, 4, IPC_CREAT | S_IRUSR | S_IWUSR);

        union semun args;
        args.val = pathsNum;

        semctl(sem_id, 0, SETVAL, args);

        args.val = 0;
        semctl(sem_msgShMem, 0, SETVAL, 0);
        args.val = 1;
        semctl(mutex_ShMem, 0, SETVAL, 1);

        unsigned short values[] = {50, 50, 50, 50};
        args.array = values;
        semctl(sem_limits, 0, SETALL, values);

        t_messageQue msgQue;
        msQueId = msgget(key, IPC_CREAT | S_IRUSR | S_IWUSR);

        int shmid = alloc_shared_memory(key, sizeof(t_message) * pathsNum);
        int *shms = (int *) get_shared_memory(shmid, 0);

        semOp(sem_msgShMem, 0, -1);
        
        if (*shms != pathsNum) {
            printf("Error with shmem confirmation %d %d\n", *shms, pathsNum);
            exit(1);
        }

        t_message *shmem = (t_message *) shms;

        int shmem_counter_id = alloc_shared_memory(IPC_PRIVATE, sizeof(int));
        int *shmem_counter = (int *) get_shared_memory(shmem_counter_id, 0);

        for (int i = 0; i < pathsNum; i++) {
            int pid = fork();
            if (pid == 0) {
                t_message messages[4];

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

                msgQue.mtype = 1;
                msgQue.msg = messages[3];

                semOp(sem_limits, 0, -1);
                write(fifo1, &messages[0], sizeof(t_message));
                semOp(sem_limits, 1, -1);
                write(fifo2, &messages[1], sizeof(t_message));

                semOp(sem_limits, 2, -1);
                semOp(mutex_ShMem, 0, -1);
                *(shmem + (*shmem_counter)++) = messages[2];
                semOp(sem_msgShMem, 0, 1);
                semOp(mutex_ShMem, 0, 1);

                semOp(sem_limits, 3, -1);
                msgsnd(msQueId, &msgQue, sizeof(t_message), 0);

                exit(0);
            }
        }
        while (wait(NULL) != -1);

        t_messageEnd endMsg;
        msgrcv(msQueId, &endMsg, sizeof(t_messageEnd), 200, 0);
        if (endMsg.mtype != 200)
            printf("Error at confirmation\n");

        //invio conferma ricezione
        shmdt(shms);

        if (shmctl(shmid, IPC_RMID, NULL) == -1)
            ErrExit("shmctl failed\n");
        else
            printf("shared memory segment removed successfully\n");
    }

    return 0;

}
