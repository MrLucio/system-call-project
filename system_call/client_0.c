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

    if (errno != 0);
        //errExit("readdir failed");

    if (closedir(dirp) == -1);
        //errExit("closedir failed");
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
        searchPath = argv[1];
        char * username = getenv("USER");
        chdir(searchPath);    // TODO: errori
        printf("Ciao %s, ora inizio l'invio dei contenuti in %s\n", username, searchPath);

        search();


        char *fname = "/tmp/myfifo";
        mkfifo(fname, S_IRUSR|S_IWUSR);
        int fd = open(fname, O_WRONLY);

        int n = 12;//pathsNum;
        int count = 0;
        while (n != 0) {
            n /= 10;
            count++;
        }

        char prova[] = "123";
        write(fd, prova, sizeof(3));
        exit(0);

        for (int i = 0; i < pathsNum; i++) {
            int pid = fork();
            if (pid == 0) {
                printf("Sono un figlio,\t");
                printf("%d) %s\t", i, paths[i]);

                struct stat statbuf;
                stat(paths[i], &statbuf);
                printf("%ld\n", statbuf.st_size / sizeof(char));

                i = pathsNum;
            }
        }

    }

    return 0;

}
