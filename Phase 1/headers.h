#include <stdio.h> //if you don't use scanf/printf change this include
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <sys/msg.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <unistd.h>
#include "cc_dst/cc_pqueue.c"
#include "cc_dst/cc_list.c"
#include "cc_dst/cc_ring_buffer.c"
#include "CQ.h"
#include <signal.h>

// typedef short bool;
#define true 1
#define false 0

#define GENKEY 200
#define SHKEY 300
#define SEMKEY 400
#define MSGKEY 500

struct pcb
{
    int pid;
    short id;
    short arrivaltime;
    short runningtime;
    short priority;
    short starttime;
    short remainingtime;
    short finishtime;
    char* state;
};

union Semun
{
    int val;               /* Value for SETVAL */
    struct semid_ds *buf;  /* Buffer for IPC_STAT, IPC_SET */
    unsigned short *array; /* Array for GETALL, SETALL */
    struct seminfo *__buf; /* Buffer for IPC_INFO (Linux-specific) */
};

struct msgbuff
{
    long mtype;
    struct pcb process;
};

int receiveMsg(int msgq_id, struct msgbuff *message, bool block)
{
    return msgrcv(msgq_id, message, sizeof(message->process), 0, (block ? !IPC_NOWAIT : IPC_NOWAIT)); // Receive message from client through message queue
}

int receiveSpecificMsg(int msgq_id, struct msgbuff *message, bool block, int mtype)
{
    int temp = msgrcv(msgq_id, message, sizeof(message->process), mtype, (block ? !IPC_NOWAIT : IPC_NOWAIT)); // Receive message from client through message queue
    if(temp == -1)
    {
        perror("Error in receiveSpecificMsg()");
        exit(-1);
    }
    return temp;
}

int sendMsg(int msgq_id, struct msgbuff *message, bool block)
{
    int temp = msgsnd(msgq_id, message, sizeof(message->process), (block ? !IPC_NOWAIT : IPC_NOWAIT));
    if(temp == -1)
    {
        perror("Error in sendMsg()");
        exit(-1);
    }
    return temp;
}

int semclk;

void down(int sem)
{
    struct sembuf op;
    union Semun semun;

    op.sem_num = 0;
    op.sem_op = -1;
    op.sem_flg = !IPC_NOWAIT;

    // printf("semclk = %d before down = %d\n", sem, semctl(sem, 0, GETVAL));
    if (semop(sem, &op, 1) == -1) //semctl(sem, 0, GETVAL, semun) != -1 && semun.val == 0 && 
    {
        perror("Error in down()");
        exit(-1);
    }
    // printf("semclk = %d after down = %d\n", sem, semctl(sem, 0, GETVAL));
}

void up(int sem)
{
    struct sembuf op;
    union Semun semun;

    op.sem_num = 0;
    op.sem_op = 1;
    op.sem_flg = !IPC_NOWAIT;

    // printf("semclk = %d before up = %d\n", sem, semctl(sem, 0, GETVAL));
    if (semctl(sem, 0, GETVAL) == 0 && semop(sem, &op, 1) == -1)
    {
        perror("Error in up()");
        exit(-1);
    }
    // printf("semclk = %d after up = %d\n", sem, semctl(sem, 0, GETVAL));
}


///==============================
// don't mess with this variable//
int *shmaddr; //
//===============================

int getClk()
{
    return *shmaddr;
}

/*
 * All process call this function at the beginning to establish communication between them and the clock module.
 * Again, remember that the clock is only emulation!
 */
void initClk()
{
    int shmid = shmget(SHKEY, 4, 0444);
    semclk = semget(SEMKEY, 1, 0666);
    while ((int)shmid == -1 || semclk == -1)
    {
        // Make sure that the clock exists
        printf("Wait! The clock not initialized yet!\n");
        sleep(1);
        shmid = shmget(SHKEY, 4, 0444);
        semclk = semget(SEMKEY, 1, 0666);
    }
    shmaddr = (int *)shmat(shmid, (void *)0, 0);
}

/*
 * All process call this function at the end to release the communication
 * resources between them and the clock module.
 * Again, Remember that the clock is only emulation!
 * Input: terminateAll: a flag to indicate whether that this is the end of simulation.
 *                      It terminates the whole system and releases resources.
 */

void destroyClk(bool terminateAll)
{
    shmdt(shmaddr);
    if (terminateAll)
    {
        killpg(getpgrp(), SIGINT);
    }
}
