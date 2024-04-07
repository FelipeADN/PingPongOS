#include <stdio.h>
#include <stdlib.h>
#include <ucontext.h>
#include "ppos.h"
#include "queue.h"

#define STACKSIZE 64*1024	/* tamanho de pilha das threads */
#define READY 0
#define EXECUTING 1
#define FINISHED 2
#define SUSPENDED 3

#ifdef DEBUG
typedef struct filaint_t
{
   struct filaint_t *prev ;  // ptr para usar cast com queue_t
   struct filaint_t *next ;  // ptr para usar cast com queue_t
   int id ;
   // outros campos podem ser acrescidos aqui
} filaint_t ;

// imprime na tela um elemento da fila (chamada pela função queue_print)
void print_elem (void *ptr)
{
   filaint_t *elem = ptr ;

   if (!elem)
      return ;

   elem->prev ? printf ("%d", elem->prev->id) : printf ("*") ;
   printf ("<%d>", elem->id) ;
   elem->next ? printf ("%d", elem->next->id) : printf ("*") ;
}
#endif

task_t mainTask, dispatcherTask, *currentTask;
task_t *ready;
int lastId = 0;
int qntReady = 0;

void dispatcher();
task_t *scheduler();

void ppos_init (){
    /* desativa o buffer da saida padrao (stdout), usado pela função printf */
    setvbuf(stdout, 0, _IONBF, 0);
    mainTask.id = lastId;
    mainTask.next = NULL;
    mainTask.prev = NULL;
    mainTask.status = READY;
    queue_append((queue_t **) &ready, (queue_t *) &mainTask);
    currentTask = &mainTask;
    qntReady--; //-- pelo dispatcher, para a primeira vez que sair da fila
    task_init(&dispatcherTask, dispatcher, NULL);
}

int task_init (task_t *task, void (*start_func)(void *), void *arg){

    lastId++;
    qntReady++;
    queue_append((queue_t **) &ready, (queue_t *) task);
    task->id = lastId;
    task->status = READY;

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

    ucontext_t *aux = &currentTask->context;
    currentTask = task;
    task->status = EXECUTING;
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
    #ifdef DEBUG
    printf("qntReady task_exit = %d\n", qntReady);
    #endif
    currentTask->status = FINISHED;
    qntReady--;
    queue_remove((queue_t **) &ready, (queue_t *) currentTask);
    if (qntReady >= 0){
        task_switch(&dispatcherTask);
    }
}

int task_id(){
    return currentTask->id;
}

void task_yield (){
    #ifdef DEBUG
    queue_print ("fila",(queue_t *) ready, print_elem) ;
    #endif
    queue_remove((queue_t **) &ready, (queue_t *) currentTask);
    queue_append((queue_t **) &ready, (queue_t *) currentTask);
    currentTask->status = READY;
    task_switch(&dispatcherTask);
}

void dispatcher(){
    // retira o dispatcher da fila de prontas, para evitar que ele ative a si próprio
    queue_remove((queue_t **) &ready, (queue_t *) &dispatcherTask);

    // enquanto houverem tarefas de usuário
    while (qntReady >= 0){

        // escolhe a próxima tarefa a executar
        task_t *next = scheduler ();
        // escalonador escolheu uma tarefa?      
        if (next != NULL){
            // transfere controle para a próxima tarefa
            next->status = EXECUTING;
            task_switch (next);

            // voltando ao dispatcher, trata a tarefa de acordo com seu estado
            switch (next->status){
            case READY:
                
                break;
            case FINISHED:
                
                break;
            case SUSPENDED:
                
                break;
            
            default:
                break;
            }   
        }
        else{
            fprintf(stderr, "nao existe proxima tarefa");
            exit(0);
        }
    }

    // encerra a tarefa dispatcher
    task_exit(0);
}

task_t *scheduler(){
    return ready;
}

