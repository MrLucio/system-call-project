/// @file fifo.h
/// @brief Contiene la definizioni di variabili e
///         funzioni specifiche per la gestione delle FIFO.

#pragma once
#include <sys/types.h>

void create_fifo();
int open_fifo(int flag);
void read_fifo(int fifo, void *buffer, size_t size);