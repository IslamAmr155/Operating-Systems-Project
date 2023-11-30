#include "headers.h"


int main(int argc, char * argv[])
{
    initClk();
    
    //TODO implement the scheduler :)
    //upon termination release the clock resources.
    key_t key_id_up, key_id_down;

    key_id_up = ftok("keyfile", 65);
    int msgq_id_up = msgget(key_id_up, 0666 | IPC_CREAT);

    struct msgbuff message;

    while(1)
    {
        /* receive all types of messages */
        int receive_val = msgrcv(msgq_id_up, &message, sizeof(message.process), 0, !IPC_NOWAIT); // Receive message from client through message queue
        if(receive_val == -1)
        {
            perror("Error in receiving!");
            exit(-1);
        }
        printf("Process received -> P_ID = %d, P_arr: %d, P_run: %d, P_pri: %d at %d\n", message.process.id, message.process.arrivaltime, message.process.runningtime, message.process.priority, getClk());
    }
    return 0;
    
    destroyClk(true);
}
