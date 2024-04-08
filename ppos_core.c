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

void task_setprio (task_t *task, int prio);
int task_getprio (task_t *task);
void dispatcher();
task_t *scheduler();

void ppos_init (){
    /* desativa o buffer da saida padrao (stdout), usado pela função printf */
    setvbuf(stdout, 0, _IONBF, 0);
    //valores da tarefa main
    mainTask.id = lastId;
    mainTask.next = NULL;
    mainTask.prev = NULL;
    mainTask.status = READY;
    mainTask.priority = 0;
    //adiciona na fila
    queue_append((queue_t **) &ready, (queue_t *) &mainTask);
    currentTask = &mainTask; //atual = main
    qntReady--; //-- pelo dispatcher, para a primeira vez que remover da fila
    task_init(&dispatcherTask, dispatcher, NULL); //inicia dispatcher
}

//inicializa uma tarefa
int task_init (task_t *task, void (*start_func)(void *), void *arg){
    //atualiza valores da tarefa
    lastId++;
    qntReady++;
    queue_append((queue_t **) &ready, (queue_t *) task);
    task->id = lastId;
    task->status = READY;
    task->priority = 0;
    task->priorityOriginal = 0;

    //copia contexto atual
    getcontext(&(task->context));
    
    //cria uma stack
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

    //guarda estado do contexto atual
    makecontext(&task->context, (void *)start_func, 1, arg);
    return 0;
}

//troca contexto executado
int task_switch (task_t *task){
    ucontext_t *aux = &currentTask->context; //aux = atual
    currentTask = task; //atual = novo atual
    task->status = EXECUTING;
    task->priority = task->priorityOriginal;
    int retorno = swapcontext(aux, &task->context); //muda de context
    //testa por erro
    if (retorno == -1){
        fprintf(stderr, "erro na troca de contexto");
        return -1;
    }
    else{
        return 0;
    }

}

//tarefa finalizada
void task_exit (int exit_code){
    currentTask->status = FINISHED;
    //remove da fila de prontas
    qntReady--;
    queue_remove((queue_t **) &ready, (queue_t *) currentTask);
    if (qntReady >= 0){ //se ainda ha tarefas, ir pro dispatcher
        if (task_switch(&dispatcherTask) == -1)
            exit(1);
    }
}

//retorna id da task atual
int task_id(){
    return currentTask->id;
}

void task_yield (){
    #ifdef DEBUG //imprime a fila
    queue_print ("#fila task_yield#",(queue_t *) ready, print_elem) ;
    #endif
    currentTask->status = READY;
    if (task_switch(&dispatcherTask) == -1)
        exit(1);
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
            if(task_switch (next) == -1)
                exit(1);

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
        else{ //nao existem mais tarefas na lista de prontas, finaliza o programa
            fprintf(stderr, "nao existe proxima tarefa");
            exit(1);
        }
    }

    // encerra a tarefa dispatcher
    task_exit(0);
}

task_t *scheduler(){
    //diminui prioridade de todas menos a que esta executando (Aging)
    task_t *taskAux = ready->next; //aux = segundo ; eh o iterador
    int id = ready->id; //id primeiro
    while (taskAux->id != id){ //quando der a volta
        taskAux->priority--;
        taskAux = taskAux->next; //avanca
    }
    #ifdef DEBUG
        printf("##task %d prioridade %d##\n", ready->id, ready->priority);
    #endif
    
    //encontra tarefa com mais prioridade
    taskAux = ready->next; //aux = segundo ; eh o iterador
    task_t *taskPrioMax = ready; //tarefa maior prioridade
    while (taskAux->id != id){ //quando der a volta
        if (taskAux->priority < taskPrioMax->priority){
            taskPrioMax = taskAux;
        }
        taskAux = taskAux->next; //avanca
    }
    #ifdef DEBUG
        printf("###scheduler retorna %d###\n", taskPrioMax->id);
    #endif
    //tarefa executando no inicio da fila
    if (taskPrioMax != ready){
        queue_remove((queue_t **) &ready, (queue_t *) taskPrioMax);
        queue_append((queue_t **) &ready, (queue_t *) taskPrioMax);
        ready = ready->prev;
    }
    return taskPrioMax;
}

void task_setprio (task_t *task, int prio){
    if (prio <= -20 || prio >= 20){ //testa validade de prio
        fprintf(stderr, "prioridade incorreta, deve estar entre -20 e +20");
        exit(1);
    }
    if (task == NULL) //NULL altera tarefa atual
        task = currentTask;
    task->priority = prio; //altera prioridade para prio
    task->priorityOriginal = prio;
    //primeira tarefa inicia como 0
    if (task->id == 2)
        task->priority--;
}
int task_getprio (task_t *task){
    if (task == NULL)
        task = currentTask;
    return task->priority;
}