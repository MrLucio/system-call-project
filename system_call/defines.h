/// @file defines.h
/// @brief Contiene la definizioni di variabili
///         e funzioni specifiche del progetto.

#define PATH_MAX_ELAB 1024
#define CHUNK_MAX_ELAB 1024
#pragma once
#include <linux/limits.h>

typedef struct message{
    int pid;
    char path[PATH_MAX];
    char chunk[CHUNK_MAX_ELAB];
} t_message;

typedef struct messageQue{
    long mtype;
    t_message msg;
} t_messageQue;

typedef struct messageEnd{
    long mtype;
} t_messageEnd;
