/// @file message_queue.c
/// @brief Contiene l'implementazione delle funzioni
///         specifiche per la gestione della MESSAGE QUEUE.

#include "message_queue.h"
#include "err_exit.h"
#include "defines.h"
#include  <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <sys/msg.h>

// Get the message queue
int get_message_queue(key_t key){
    int msqid = msgget(key, IPC_CREAT | S_IRUSR | S_IWUSR);
    if (msqid == -1)
        ErrExit("msgget failed");
    return msqid;
}

// Delete the message queue
void remove_message_queue(int msqid){
    if (msgctl(msqid, IPC_RMID, NULL) == -1) printf("msg queue already deleted\n");
}

// Read a message from message queue
int read_message_queue(int msqid, message_t *msg){
    messageQueue_t msgQueue;
    int response = msgrcv(msqid, &msgQueue, sizeof(message_t), 1, IPC_NOWAIT);
    if (response != -1)
        memcpy(msg, &msgQueue.msg, sizeof(message_t));
    return response;
}