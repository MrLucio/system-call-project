/// @file defines.c
/// @brief Contiene l'implementazione delle funzioni
///         specifiche del progetto.

#include "defines.h"

size_t append2Path(char *directory, char *searchPath) {
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

void search(char *searchPath, char *searchPrefix, char **paths, int *pathsNum) {
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
            size_t lastPath = append2Path(dentry->d_name, searchPath);

            if (strncmp(dentry->d_name, searchPrefix, strlen(searchPrefix)) == 0
                && !checkFileSize(searchPath, 4096))
            {
                paths[*pathsNum] = (char *)(malloc(strlen(searchPath)));
                strcpy(paths[(*pathsNum)++], searchPath);
            }

            // reset current searchPath
            searchPath[lastPath] = '\0';
        // is the current dentry a directory
        } else if (dentry->d_type == DT_DIR) {
            // exetend current searchPath with the directory name
            size_t lastPath = append2Path(dentry->d_name, searchPath);
            // call search method
            search(searchPath, searchPrefix, paths, pathsNum);
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