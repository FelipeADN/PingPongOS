#include <stdio.h>
#include <stdlib.h>
#include <ucontext.h>
#include "ppos.h"
#include "queue.h"

#define STACKSIZE 64*1024	/* tamanho de pilha das threads */

task_t MainTask, *CurrentTask;
int lastId = 0;

void ppos_init (){
    /* desativa o buffer da saida padrao (stdout), usado pela função printf */
    setvbuf(stdout, 0, _IONBF, 0);
    MainTask.id = lastId;
    CurrentTask = &MainTask;
    MainTask.next = NULL;
    MainTask.prev = NULL;
    MainTask.status = 0;
}

int task_init (task_t *task, void (*start_func)(void *), void *arg){

    lastId++;
    task->id = lastId;    

    getcontext(&(task->context));
    
    char *stack = malloc (STACKSIZE);
    if (stack)
    {
        task->context.uc_stack.ss_sp = stack;
        task->context.uc_stack.ss_size = STACKSIZE;
        task->context.uc_stack.ss_flags = 0;
        task->context.uc_link = 0;
    }
    else
    {
        perror("Erro na criação da pilha: ");
        return(-1);
    }

    makecontext(&task->context, (void *)start_func, 1, arg);
    return 0;
}

int task_switch (task_t *task){

    ucontext_t *aux = &CurrentTask->context;
    CurrentTask = task;
    int retorno = swapcontext(aux, &task->context);
    if (retorno == -1){
        fprintf(stderr, "erro na troca de contexto");
        return -1;
    }
    else{
        return 0;
    }

}

void task_exit (int exit_code){
    task_switch(&MainTask);
}

int task_id(){
    return CurrentTask->id;
}