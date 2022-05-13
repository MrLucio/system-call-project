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

char *searchPath;
char *searchPrefix = "sendme_";
int pathsNum = 0;
char *paths[100];

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
        // Skip . and ..
        if (strcmp(dentry->d_name, ".") == 0 ||
            strcmp(dentry->d_name, "..") == 0)
        {  continue;  }

        // is the current dentry a regular file?
        if (dentry->d_type == DT_REG) {
            // exetend current searchPath with the file name
            size_t lastPath = append2Path(dentry->d_name);

            if (strncmp(dentry->d_name, searchPrefix, strlen(searchPrefix)) == 0) {
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
        chdir(searchPath);    // TODO: errori
        printf("Ciao %s, ora inizio l'invio dei contenuti in %s\n", username, searchPath);

        search();

        create_fifo("/tmp/fifo_1");
        int fifo1 = open_fifo("/tmp/fifo_1", O_WRONLY);

        create_fifo("/tmp/fifo_2");
        int fifo2 = open_fifo("/tmp/fifo_2", O_WRONLY);

        char c[4] = {0};
        sprintf(c, "%d", pathsNum);

        write(fifo1, c, strlen(c));

        struct sembuf sem_p = {0, -1, 0};
        struct sembuf sem_v = {0, 1, 0};
        struct sembuf sem_wait_zero = {0, 0, 0};

        union semun args;
        args.val = pathsNum;

        int sem_id = semget(IPC_PRIVATE, 1, IPC_CREAT | S_IRUSR | S_IWUSR);
        semctl(sem_id, 0, SETVAL, args);

        key_t key = ftok(cwd, 1);
        printf("key =  %d\n", key);

        int shmid = alloc_shared_memory(key, sizeof(t_message) * pathsNum);
        //printf("shmid = %d\n", shmid);

        for (int i = 0; i < pathsNum; i++) {
            int pid = fork();
            if (pid == 0) {
                t_message messages[4];

                int fd = open(paths[i], O_RDONLY);
                off_t end_offset = lseek(fd, 0, SEEK_END);

                if (end_offset >= 4096) {
                    ErrExit("file size larger than 4KB\n");
                    //exit(0);
                }

                int position = 0;
                int window_size;
                for (int j = 4; j > 0; j--) {
                    lseek(fd, position, SEEK_SET);
                    window_size = (end_offset - position + j - 1) / j; // ceil division of (end_offset - position) with j
                    position += window_size;

                    messages[4 - j].pid = getpid();
                    strcpy(messages[4 - j].path, paths[4 - j]);
                    
                    ssize_t num_read = read(fd, messages[4 - j].chunk, window_size);
                    if (num_read == -1)
                        ErrExit("read");
                    messages[4 - j].chunk[num_read] = '\0';
                }

                semop(sem_id, &sem_p, 1);
                semop(sem_id, &sem_wait_zero, 1);

                write(fifo1, &messages[0], sizeof(t_message));
                write(fifo2, &messages[1], sizeof(t_message));

                /*
                    TODO: INVIO
                */

                i = pathsNum;
                return 0;
            }
        }
        while ( wait(NULL) != -1);

        //invio conferma ricezione
        //printf("%d\n", shmid);
        int *shms = (int *)get_shared_memory(shmid, 0);
        if (shms == (void *)-1)
            printf("first shmat failed\n");
        //printf("%d\n", *shms);
        sleep(2);
        shmdt(shms);
        if (shmctl(shmid, IPC_RMID, NULL) == -1)
            ErrExit("shmctl failed\n");
        else;
            //printf("shared memory segment removed successfully\n");

        /*
        char *path = "/home/l/boh";
        char *chunk = "Lorem ipsum";
        t_message msg;
        msg.pid = 123;
        strcpy(msg.path, path);
        strcpy(msg.chunk, chunk);
        write(fd, &msg, sizeof(msg));
        */

    }

    return 0;

}
