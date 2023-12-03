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

#define SHKEY 300

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

int sendMsg(int msgq_id, struct msgbuff *message, bool block)
{
    return msgsnd(msgq_id, message, sizeof(message->process), (block ? !IPC_NOWAIT : IPC_NOWAIT));
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
    while ((int)shmid == -1)
    {
        // Make sure that the clock exists
        printf("Wait! The clock not initialized yet!\n");
        sleep(1);
        shmid = shmget(SHKEY, 4, 0444);
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
