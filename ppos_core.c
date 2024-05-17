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
task_t *ready, *suspended, *sleeping;
int lastId = 0;
int amtReady = 0;
char sistemFunction = 0;
int time = 0;

void task_setprio(task_t *task, int prio);
int task_getprio(task_t *task);
void dispatcher();
task_t *scheduler(int type);
void interrupt_handler(int signum);
unsigned int systime();
int task_wait(task_t *task);
void task_suspend(task_t **queue);
void task_awake(task_t * task, task_t **queue);
void task_sleep(int t);

void ppos_init(){
    sistemFunction = 1;
    /* desativa o buffer da saida padrao (stdout), usado pela função printf */
    setvbuf(stdout, 0, _IONBF, 0);

    // registra a ação para o sinal de timer SIGALRM (sinal do timer)
    action.sa_handler = interrupt_handler;
    sigemptyset(&action.sa_mask);
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
        exit(2);
    }

    //valores da tarefa main
    mainTask.id = lastId;
    mainTask.next = NULL;
    mainTask.prev = NULL;
    mainTask.status = READY;
    mainTask.priority = 0;
    mainTask.sistemTask = 0;
    mainTask.startTime = time;
    mainTask.processorTime = 0;
    mainTask.activations = 1;
    mainTask.childId = -1;
    mainTask.childReturn = 0;
    mainTask.wakeupTime = 0;
    queue_append((queue_t **) &ready, (queue_t *) &mainTask); //adiciona no final da fila de prontas
    currentTask = &mainTask; //atual = main
    task_init(&dispatcherTask, dispatcher, NULL); //inicia dispatcher
    sistemFunction = 1;
    dispatcherTask.sistemTask = 1;
    sistemFunction = 0;
}

//inicializa uma tarefa
int task_init(task_t *task, void (*start_func)(void *), void *arg){   
    #ifdef DEBUG //imprime a fila
    queue_print("#fila ready dispatcher#",(queue_t *) ready, print_elem);
    queue_print("#fila sleeping dispatcher#",(queue_t *) sleeping, print_elem);
    queue_print("#fila suspended dispatcher#",(queue_t *) suspended, print_elem);
    #endif
    sistemFunction = 1;
    //atualiza valores da tarefa
    lastId++;
    amtReady++;
    queue_append((queue_t **) &ready, (queue_t *) task); //adiciona no final da fila de prontas
    task->id = lastId;
    task->status = READY;
    task->priority = 0;
    task->priorityOriginal = 0;
    task->sistemTask = 0;
    task->quantum = 0;
    task->startTime = time;
    task->processorTime = 0;
    task->activations = 0;
    task->childId = -1;
    task->childReturn = 0;
    task->wakeupTime = 0;

    //copia contexto atual
    getcontext(&(task->context));
    
    //cria uma stack
    char *stack = malloc(STACKSIZE);
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
int task_switch(task_t *task){
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
void task_exit(int exit_code){
    sistemFunction = 1;
    currentTask->status = FINISHED;
    printf("Task %d exit: execution time %d ms, processor time %d, %d activations\n", currentTask->id, time - currentTask->startTime, currentTask->processorTime, currentTask->activations);
    //acorda todas as tarefas que foram suspensas por quem terminou
    if (suspended != NULL){ //existem tarefas suspensas
        task_t *aux = suspended;
        do{
            if (aux->childId == currentTask->id){ //tarefas suspensas pela currentTask
                task_t *next = aux->next;
                queue_remove((queue_t **) &suspended, (queue_t *) aux);
                queue_append((queue_t **) &ready, (queue_t *) aux);
                amtReady++;
                aux->status = READY;
                aux->childReturn = exit_code;
                aux->childId = -1;
                aux = next;
            }
            else{
                aux = aux->next;
            }
            if (suspended == NULL) //fila esvaziou
                break;
        }while (aux->id != suspended->id); //deu a volta na fila
    }
    //remove da fila de prontas
    if (ready != NULL)
        queue_remove((queue_t **) &ready, (queue_t *) currentTask); //remove da fila de prontas
    amtReady--;
    sistemFunction = 0;
    if (task_switch(&dispatcherTask) == -1)
        exit(3);
}

//retorna id da task atual
int task_id(){
    return currentTask->id;
}

void task_yield(){
    if (task_switch(&dispatcherTask) == -1)
        exit(3);
}

void dispatcher(){
    // retira o dispatcher da fila de prontas, para evitar que ele ative a si próprio
    queue_remove((queue_t **) &ready, (queue_t *) &dispatcherTask);

    // enquanto houverem tarefas prontas
    while (amtReady > 0 || suspended != NULL || sleeping != NULL){
        #ifdef DEBUG //imprime a fila
        queue_print("#fila ready dispatcher#",(queue_t *) ready, print_elem);
        task_t *aux = ready;
        do{
            printf("%d ", aux->status);
            aux = aux->next;
        }while (aux->id != ready->id); //deu a volta na fila
        queue_print("#fila sleeping dispatcher#",(queue_t *) sleeping, print_elem);
        queue_print("#fila suspended dispatcher#",(queue_t *) suspended, print_elem);
        #endif

        do{
            //acorda tarefas dormindo
            if (sleeping != NULL){ //existem tarefas dormindo
                task_t *aux = sleeping;
                do{
                    if (time >= aux->wakeupTime){ //ja deu a hora de acordar
                        task_t *next = aux->next;
                        queue_remove((queue_t **) &sleeping, (queue_t *) aux);
                        queue_append((queue_t **) &ready, (queue_t *) aux);
                        amtReady++;
                        aux->status = READY;
                        aux = next;
                    }
                    else{
                        aux = aux->next;
                    }
                    if (sleeping == NULL) //fila esvaziou
                        break;
                }while (aux->id != sleeping->id); //deu a volta na fila
            }
        }while (amtReady == 0);

        // escolhe a próxima tarefa a executar
        task_t *next = scheduler (AGING);
        // escalonador escolheu uma tarefa?
        if (next != NULL){
            // transfere controle para a próxima tarefa
            next->quantum = QUANTUM;
            next->status = EXECUTING;
            if (task_switch(next) == -1)
                exit(4);
            
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
            exit(5);
        }
    }
    
    // encerra a tarefa dispatcher
    task_exit(0);
}

task_t *scheduler(int type){
    int id = ready->id; //id primeiro 

    //AGING
    if (type == 1 && ready->status == EXECUTING){ 
        task_t *taskAux = ready->next; //aux = segundo ; eh o iterador
        //diminui prioridade de todas menos a que esta executando (Aging)
        while (taskAux->id != id){ //quando der a volta
            taskAux->priority--;
            taskAux = taskAux->next; //avanca
        }
    }

    //encontra tarefa com mais prioridade
    //se tiverem a mesma, pega a mais antiga (FCFS)
    task_t *taskAux = ready->next; //aux iterador
    task_t *taskPrioMax = ready; //tarefa maior prioridade
    //acha o minimo = maior prioridade
    do{
        if (taskAux->priority < taskPrioMax->priority)
            taskPrioMax = taskAux;
        taskAux = taskAux->next; //avanca
    }while (taskAux->id != id); //quando der a volta

    ready->status = READY; //tarefa antiga volta a ser ready
    taskPrioMax->status = EXECUTING; //tarefa escolhida passa a ser executada

    if (taskPrioMax != ready){
        //tarefa escolhida/executando pro inicio da fila
        queue_remove((queue_t **) &ready, (queue_t *) taskPrioMax);
        queue_append((queue_t **) &ready, (queue_t *) taskPrioMax); //final da fila
        ready = ready->prev; //inicio da fila
    }
    return taskPrioMax;
}

//define a prioridade da tarefa
void task_setprio(task_t *task, int prio){
    sistemFunction = 1;
    if (prio <= -20 || prio >= 20){ //testa validade de prio
        fprintf(stderr, "prioridade incorreta, deve estar entre -20 e +20");
        exit(6);
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
int task_getprio(task_t *task){
    sistemFunction = 1;
    if (task == NULL)
        task = currentTask;
    sistemFunction = 0;
    return task->priority;
}

// tratador de ticks
void interrupt_handler(int signum){
    currentTask->processorTime++;
    time++; //a cada tick aumenta o tempo
    if (currentTask->sistemTask == 1 || sistemFunction == 1) //tarefa de sistema
        return;
    if (currentTask->quantum == 0) //acabou seu tempo
        task_yield(); //volta a ser ready e vai pro dispatcher
    currentTask->quantum--; //diminui seu tempo
}

//retorna o tempo do sistema
unsigned int systime(){
    return time;
}

//suspende a tarefa atual ate a outra tarefa terminar
int task_wait(task_t *task){
    sistemFunction = 1;
    if (task == NULL || task->status == FINISHED){ //existe e nao foi encerrada
        sistemFunction = 0;
        return -1;
    }
    currentTask->status = SUSPENDED;
    currentTask->childId = task->id;
    queue_remove((queue_t **) &ready, (queue_t *) currentTask);
    queue_append((queue_t **) &suspended, (queue_t *) currentTask);
    amtReady--;
    if (task_switch(&dispatcherTask) == -1)
        exit(3);
    int value = currentTask->childReturn;
    currentTask->childId = 0; //reseta o valor
    sistemFunction = 0;
    return value;
}

//suspende tarefa atual, e guarda em queue
void task_suspend(task_t **queue){
    sistemFunction = 1;
    if (queue == NULL) //se fila nao existir
        exit(7);
    currentTask->status = SUSPENDED;
    queue_remove((queue_t **) &ready, (queue_t *) currentTask);
    queue_append((queue_t **) queue, (queue_t *) currentTask);
    amtReady--;
    sistemFunction = 0;
    if (task_switch(&dispatcherTask) == -1)
        exit(3);
}

//acorda tarefa task da fila queue, se estiver la, e adiciona na fila ready
void task_awake(task_t * task, task_t **queue){
    sistemFunction = 1;
    //testa se fila/elemento existe e esta na fila
    if (queue_remove((queue_t **) &queue, (queue_t *) task) < 0)
        exit(8);
    task->status = READY;
    queue_append((queue_t **) &ready, (queue_t *) task);
    sistemFunction = 0;
}

//suspende tarefa atual por t milissegundos
void task_sleep(int t){
    sistemFunction = 1;
    currentTask->wakeupTime = time + t;
    task_suspend(&sleeping);
    sistemFunction = 0;
}