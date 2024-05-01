#include <stdio.h>
#include <stdlib.h>
#include <ucontext.h>
#include <signal.h>
#include <sys/time.h>
#include "ppos.h"
#include "queue.h"

#define STACKSIZE 64*1024	/* tamanho de pilha das threads */
#define READY 0
#define EXECUTING 1
#define FINISHED 2
#define SUSPENDED 3
#define TICKRATE 1000 //microsegundos = 1 milisegundo
#define QUANTUM 10 //quantum tem 20 ticks
#define FCFS 0
#define AGING 1

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

// estrutura que define um tratador de sinal (deve ser global ou static)
struct sigaction action ;

// estrutura de inicialização do timer
struct itimerval timer;

task_t mainTask, dispatcherTask, *currentTask;
task_t *ready;
int lastId = 0;
int qntReady = 0;
char sistemFunction = 0;
int time = 0;

void task_setprio (task_t *task, int prio);
int task_getprio (task_t *task);
void dispatcher();
task_t *scheduler(int type);
void tratador (int signum);
unsigned int systime ();

void ppos_init (){
    sistemFunction = 1;
    /* desativa o buffer da saida padrao (stdout), usado pela função printf */
    setvbuf(stdout, 0, _IONBF, 0);

    // registra a ação para o sinal de timer SIGALRM (sinal do timer)
    action.sa_handler = tratador;
    sigemptyset (&action.sa_mask);
    action.sa_flags = 0;
    if (sigaction(SIGALRM, &action, 0) < 0){
        perror("Erro em sigaction: ");
        exit(1);
    }

    // ajusta valores do temporizador
    timer.it_value.tv_usec = TICKRATE;  // primeiro disparo, em micro-segundos
    timer.it_interval.tv_usec = TICKRATE;   // disparos subsequentes, em micro-segundos

    // arma o temporizador ITIMER_REAL
    if (setitimer(ITIMER_REAL, &timer, 0) < 0){
        perror("Erro em setitimer: ");
        exit(1);
    }

    //valores da tarefa main
    mainTask.id = lastId;
    mainTask.next = NULL;
    mainTask.prev = NULL;
    mainTask.status = READY;
    mainTask.priority = 0;
    mainTask.sistemTask = 0;
    mainTask.startTime = time;
    mainTask.activations = 1;
    queue_append((queue_t **) &ready, (queue_t *) &mainTask); //adiciona no final da fila de prontas
    currentTask = &mainTask; //atual = main
    qntReady--; //-- pelo dispatcher, para a primeira vez que remover da fila
    task_init(&dispatcherTask, dispatcher, NULL); //inicia dispatcher
    sistemFunction = 1;
    dispatcherTask.sistemTask = 1;
    sistemFunction = 0;
}

//inicializa uma tarefa
int task_init (task_t *task, void (*start_func)(void *), void *arg){
    sistemFunction = 1;
    //atualiza valores da tarefa
    lastId++;
    qntReady++;
    queue_append((queue_t **) &ready, (queue_t *) task); //adiciona no final da fila de prontas
    task->id = lastId;
    task->status = READY;
    task->priority = 0;
    task->priorityOriginal = 0;
    task->sistemTask = 0;
    task->quantum = 0;
    task->startTime = time;

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
    sistemFunction = 0;
    return 0;
}

//troca contexto executado
int task_switch (task_t *task){
    sistemFunction = 1;
    ucontext_t *aux = &currentTask->context; //aux = atual
    currentTask = task; //atual = novo atual
    task->status = EXECUTING;
    task->activations++; //ativou novamente
    task->priority = task->priorityOriginal;
    sistemFunction = 0;
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
    sistemFunction = 1;
    currentTask->status = FINISHED;
    printf("Task %d exit: execution time %d ms, processor time %d, %d activations\n", currentTask->id, time - currentTask->startTime, currentTask->processorTime, currentTask->activations);
    //remove da fila de prontas
    qntReady--;
    if (ready != NULL)
        queue_remove((queue_t **) &ready, (queue_t *) currentTask); //remove da fila de prontas
    sistemFunction = 0;
    if (task_switch(&dispatcherTask) == -1)
        exit(1);
}

//retorna id da task atual
int task_id(){
    return currentTask->id;
}

void task_yield (){
    sistemFunction = 1;
    #ifdef DEBUG //imprime a fila
    queue_print ("#fila task_yield#",(queue_t *) ready, print_elem) ;
    #endif
    currentTask->status = READY;
    sistemFunction = 0;
    if (task_switch(&dispatcherTask) == -1)
        exit(1);
}

void dispatcher(){
    // retira o dispatcher da fila de prontas, para evitar que ele ative a si próprio
    queue_remove((queue_t **) &ready, (queue_t *) &dispatcherTask);

    // enquanto houverem tarefas de usuário
    while (qntReady >= 0){

        // escolhe a próxima tarefa a executar
        task_t *next = scheduler (AGING);
        // escalonador escolheu uma tarefa?      
        if (next != NULL){
            // transfere controle para a próxima tarefa
            next->quantum = QUANTUM;
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

task_t *scheduler(int type){
    task_t *taskAux = ready->next; //aux = segundo ; eh o iterador
    int id = ready->id; //id primeiro   

    //AGING
    if (type == 1){ 
        //diminui prioridade de todas menos a que esta executando (Aging)
        while (taskAux->id != id){ //quando der a volta
            taskAux->priority--;
            taskAux = taskAux->next; //avanca
        }
    }

    //encontra tarefa com mais prioridade
    //se tiverem a mesma, pega a mais antiga (FCFS)
    taskAux = ready->next; //aux = segundo ; eh o iterador
    task_t *taskPrioMax = ready; //tarefa maior prioridade
    while (taskAux->id != id){ //quando der a volta
        if (taskAux->priority < taskPrioMax->priority)
            taskPrioMax = taskAux;
        taskAux = taskAux->next; //avanca
    }

    #ifdef DEBUG
        printf("##task %d prioridade %d##\n", ready->id, ready->priority);
    #endif
    
    #ifdef DEBUG
        printf("###scheduler retorna %d###\n", taskPrioMax->id);
    #endif
    //tarefa escolhida/executando pro inicio da fila
    if (taskPrioMax != ready){
        queue_remove((queue_t **) &ready, (queue_t *) taskPrioMax);
        queue_append((queue_t **) &ready, (queue_t *) taskPrioMax);
        ready = ready->prev;
    }
    return taskPrioMax;
}

//define a prioridade da tarefa
void task_setprio (task_t *task, int prio){
    sistemFunction = 1;
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
    sistemFunction = 0;
}

//retorna a prioridade da tarefa
int task_getprio (task_t *task){
    sistemFunction = 1;
    if (task == NULL)
        task = currentTask;
    sistemFunction = 0;
    return task->priority;
}

// tratador de ticks
void tratador (int signum){
    currentTask->processorTime++;
    time++; //a cada tick aumenta o tempo
    if (currentTask->sistemTask == 1 || sistemFunction == 1) //tarefa de sistema
        return;
    if (currentTask->quantum == 0) //acabou seu tempo
        task_yield(); //volta a ser ready e vai pro dispatcher
    currentTask->quantum--; //diminui seu tempo
}

unsigned int systime (){
    return time;
}
