/// @file defines.h
/// @brief Contiene la definizioni di variabili
///         e funzioni specifiche del progetto.

#pragma once
#include <linux/limits.h>

typedef struct message{
    int pid;
    char path[PATH_MAX];
    char chunk[1024];
} t_message;

typedef struct messageEnd{
    long mtype;
} t_messageEnd;
