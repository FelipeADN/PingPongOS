//Aluno: Felipe Augusto Dittert Noleto GRR:20205689
#include "queue.h"
#include <stdio.h>
#include <stdlib.h>

int queue_size(queue_t *queue){
    int size = 0;
    if (queue != NULL){
        if (queue->next != NULL){ //elemento esta em uma fila e a fila existe
        queue_t *aux = queue;
            while(queue != aux->next){ //percorre ate se encontrar
                aux = aux->next;
                size++;
            }
            size++; //+1 no final
        }
    }
    return size;
}

void queue_print(char *name, queue_t *queue, void print_elem (void*)){
    printf("%s: [", name);
    if (queue != NULL){ // fila existe
        queue_t *aux = queue;
        if (aux->next != NULL){ //fila nao esta vazia
            while(queue != aux->next){ //da a volta na fila
                print_elem(aux); //imprime o elemento da vez
                printf(" ");
                aux = aux->next;
            }
            print_elem(aux); //imprime ultimo
        }
    }
    printf("]\n");
}

int queue_append(queue_t **queue, queue_t *elem){
    if (queue == NULL){
        fprintf(stderr, "fila nao existe");
        return -1;
    }
    else if(elem == NULL){
        fprintf(stderr, "elemento nao existe");
        return -2;
    }
    else if(elem->next != NULL || elem->prev != NULL){ //elemento possui next ou prev
        fprintf(stderr, "elemento ja esta em uma fila");
        return -3;
    }
    if (*queue == NULL){// fila esta vazia
        *queue = elem;
        elem->next = elem;
        elem->prev = elem;
    }
    else{ //fila tem pelo menos 1 elemento
        elem->next = *queue;
        elem->prev = (*queue)->prev;
        (*queue)->prev->next = elem;
        (*queue)->prev = elem;
    }
    return 0;
}

int queue_remove (queue_t **queue, queue_t *elem){
    if (queue == NULL){
        fprintf(stderr, "fila nao existe");
        return -1;
    }
    else if (*queue == NULL){
        fprintf(stderr, "fila esta vazia");
        return -1;
    }
    else if(elem == NULL){
        fprintf(stderr, "elemento nao existe");
        return -2;
    }
    else if(elem->next == NULL || elem->prev == NULL){ //elemento nao possui next ou prev
        fprintf(stderr, "elemento nao esta em uma fila");
        return -3;
    }
    queue_t *aux = *queue; //primeiro da fila
    while(elem != aux){ //percorre ate encontrar elem ou dar a volta
        aux = aux->next;
        if (aux == *queue){ //deu a volta
            fprintf(stderr, "elemento nao esta na fila");
            return -4;
        }
    }
    //elemento encontrado
    if (elem->next == elem || elem->prev == elem){ //fila so possui 1 elemento
        *queue = NULL;
        elem->next = NULL;
        elem->prev = NULL;
    }
    else{ 
        if(*queue == elem) //eliminando o primeiro
            *queue = elem->next;
        elem->prev->next = elem->next;
        elem->next->prev = elem->prev;
        elem->next = NULL;
        elem->prev = NULL;
    }
    return 0;
}