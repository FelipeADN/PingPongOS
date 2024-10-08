// PingPongOS - PingPong Operating System
// Prof. Carlos A. Maziero, DINF UFPR
// Versão 1.5 -- Março de 2023

// Estruturas de dados internas do sistema operacional

#ifndef __PPOS_DATA__
#define __PPOS_DATA__

#include <ucontext.h>		// biblioteca POSIX de trocas de contexto

// Estrutura que define um Task Control Block (TCB)
typedef struct task_t
{
  struct task_t *prev, *next ;  // ponteiros para usar em filas
  int id ;				              // identificador da tarefa
  ucontext_t context ;			    // contexto armazenado da tarefa
  short status ;			          // pronta, rodando, suspensa, ...
  short priority;               //prioridade da tarefa
  short priorityOriginal;       //prioridade inicial da tarefa
  int quantum;                  //quantum atual da tarefa
  char sistemTask;              //1 pertence ao sistema, 0 nao
  int startTime;                //timestamp do inicio da tarefa
  int processorTime;            //soma do tempo de execucao
  int activations;              //quantidade de ativacoes
  int childId;                  //id da tarefa que deve terminar antesd e voltar
  int childReturn;              //exit_code da tarefa filho
  int wakeupTime;               //quando que deve acordar
  // ... (outros campos serão adicionados mais tarde)
} task_t ;

// estrutura que define um semáforo
typedef struct
{
  int counter; //contador
  task_t *queue; //fila de tarefas suspensas
  short destroyed; //se foi destruido
  int lock; //lock para test-and-set
} semaphore_t ;

// estrutura que define um mutex
typedef struct
{
  // preencher quando necessário
} mutex_t ;

// estrutura que define uma barreira
typedef struct
{
  // preencher quando necessário
} barrier_t ;

// estrutura que define uma fila de mensagens
typedef struct
{ 
  void *queueBuffer; //buffer da fila
  int capacity; //tamanho maximo da fila
  short size; //tamanho por mensagem
  int amtMessage; //quantidade de mensagens
  short destroyed; //se foi destruido

  semaphore_t semaphorBuffer; //semaforo para o buffer
  semaphore_t semaphorProducer; //semaforo para o produtor
  semaphore_t semaphorConsumer; //semaforo para o consumidor
} mqueue_t ;

#endif

