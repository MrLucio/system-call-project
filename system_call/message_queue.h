/// @file message_queue.h
/// @brief Contiene la definizioni di variabili e funzioni
///         specifiche per la gestione della MESSAGE QUEUE.

#pragma once
#include <sys/types.h>
#include "defines.h"

typedef struct messageQueue{
    long mtype;
    message_t msg;
} messageQueue_t;

void remove_message_queue(int msqid);
int get_message_queue(key_t key);
int read_message_queue(int msqid, message_t *msg);
