#include "headers.h"

/* Modify this file as needed*/
int shm_id;
int msgqIdProcess;

int main(int agrc, char * argv[])
{
    initClk();
    // Retrieve mtype from argv
    int mtype = atoi(argv[1]);
    printf("Process starting at time: %d\n", getClk());
    
    msgqIdProcess = msgget(MSGKEY, 0666);
    struct msgbuff message;

    //TODO it needs to get the remaining time from somewhere
    do
    {
        receiveSpecificMsg(msgqIdProcess, &message, true, getpid());
        message.process.remainingtime--;
        printf("Process %d: Remaining time = %d\n", mtype, message.process.remainingtime);
        printf("Clock = %d\n", getClk());
        printf("-------------------------------------------------\n");
        message.mtype = getppid();
        sendMsg(msgqIdProcess, &message, true);
    } while(message.process.remainingtime);

    exit(mtype);
    
    printf("Process %d finished at time: %d\n", mtype, getClk());
    destroyClk(false);
    
    return 0;
}