#include <sys/types.h>
#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>

int main(){
    char path[4096];
    int fileOpen;
    for (int i = 6; i < 100; i++){
        sprintf(path, "tests/sendme_%d", i);
        fileOpen = open(path, O_WRONLY | O_CREAT, 0666);
        write(fileOpen, "AAAAAABBBBBBCCCCCCD", 19);
    }
    
}