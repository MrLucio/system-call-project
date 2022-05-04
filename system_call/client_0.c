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
#include <sys/sem.h>
#include "err_exit.h"

char *searchPath;
char *searchPrefix = "sendme_";
int pathsNum = 0;
char *paths[100];

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

        char *fname = "/tmp/myfifo";
        mkfifo(fname, S_IRUSR|S_IWUSR);
        int fd = open(fname, O_WRONLY);

        char c[4] = {0};
        sprintf(c, "%d", pathsNum);

        write(fd, c, sizeof(3));

        struct sembuf sem_p = {0, -1, 0};
        struct sembuf sem_v = {0, 1, 0};

        int mutex_id = semget(IPC_PRIVATE, 1, IPC_CREAT | S_IRUSR | S_IWUSR);
        int sem_id = semget(IPC_PRIVATE, 1, IPC_CREAT | S_IRUSR | S_IWUSR);
        semctl(mutex_id, 0, SETVAL, 1); // TODO
        semctl(sem_id, 0, SETVAL, 0); // TODO

        printf("%s %d\n", cwd, strlen(cwd));
        key_t key = ftok(cwd, 1);
        printf("key1: %d\n", key);

        key_t key2 = ftok(cwd, 1);
        printf("key2: %d\n", key2);

        for (int i = 0; i < pathsNum; i++) {
            int pid = fork();
            if (pid == 0) {
                char* file_buffers[4];
                //printf("%d) %s\t", i, paths[i]);

                int fd = open(paths[i], S_IRUSR);
                off_t end_offset = lseek(fd, 0, SEEK_END);
                //printf("c: %ld %ld\n", end_offset, end_offset/4);

                if (end_offset >= 4000) {
                    ErrExit("file size larger than 4KB\n");
                    //exit(0);
                }

                int position = 0;
                int window_size;
                for (int j = 4; j > 0; j--) {
                    lseek(fd, position, SEEK_SET);
                    window_size = (end_offset - position + j - 1) / j; // ceil division of (end_offset - position) with j
                    position += window_size;

                    file_buffers[4 - i] = (char*) malloc(sizeof(char) * window_size + 1);
                    ssize_t num_read = read(fd, file_buffers[4 - i], window_size);
                    if (num_read == -1)
                        ErrExit("read");
                    file_buffers[4 - i][num_read] = '\0';
                }

                semop(mutex_id, &sem_p, 1);
                if (semctl(sem_id, 0, GETNCNT) == pathsNum - 1)
                    semctl(sem_id, 0, SETVAL, pathsNum);
                semop(mutex_id, &sem_v, 1);

                semop(sem_id, &sem_p, 1);

                /*
                    TODO: INVIO
                */
                
                for (int j = 0; j < 4; j++) free(file_buffers[j]);

                i = pathsNum;
            }
        }
        while ( wait(NULL) != -1);
    }

    return 0;

}
