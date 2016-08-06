#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <signal.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/types.h>
#include <sys/time.h>

/* States for captain */
#define DOCKED_LOADING 0
#define SAILING 1
#define DOCKED_UNLOADING 2
#define SAILING_BACK 3
#define IDLE 4

/* Message TYPES */
#define CAR_ARRIVING 1
#define TRUCK_ARRIVING 2
#define CAR_LOADING 3
#define TRUCK_LOADING 4
#define CAR_LOADED 5
#define TRUCK_LOADED 6
#define CAR_UNLOADING 7
#define TRUCK_UNLOADING 8
#define CAR_UNLOADED 9
#define TRUCK_UNLOADED 10
#define START_LOADING 12
#define LOADING_COMPLETE 13
#define LOADING_COMPLETE_ACK 14
#define UNLOADING_COMPLETE 15
#define UNLOADING_COMPLETE_ACK 15
#define FERRY_ARRIVED 20
#define FERRY_ARRIVED_ACK 20
#define TRUCK_ARRIVED 21
#define CAR_ARRIVED 22
#define FERRY_RETURNED 23
#define FERRY_RETURNED_ACK 23
#define CAR_TRAVELING 24
#define TRUCK_TRAVELING 25
#define TERMINATION 49

/* Constants */
#define TOTAL_SPOTS_ON_FERRY 6
#define MAX_LOADS 3
#define CROSSING_TIME 1000000

/* Redefines the message structure */
typedef struct mymsgbuf
{
    long mtype;
    int arrivalInfo[2];
} mess_t;


int timeChange( const struct timeval startTime );

int main()
{
    pid_t pid;                  /* process ID of vehicle process in*/
                                /* captain process, 0 in vehicle process */
    int captainState = IDLE;    /* state variable for the captain */
    int k;                      /* counter */


    /* message properties */
    mess_t buf;                 /* structure to hold each message sent*/
                                /* or received (see initialization below) */
    int length;                 /* length of each message structure */
    int msgType;                /* temporary variable to hold type */


    /* timing of car and truck creation */
    struct timeval startTime;   /* time at start of program execution */
    int elapsed = 0;            /* time from start of program execution */
    int maxTimeToNextArrival;   /* maximum number of milliseconds */
                                /* between vehicle arrivals*/
    int lastArrivalTime = 0;    /* time from start of program execution */
                                /* at which the last vehicle arrived */
    int truckArrivalProb;       /* User supplied probability that the */
                                /* next vehicle is a truck */
    int isTruck = 0;           


    /* queue information */                    
    int qidToCaptainA = 0;      /* ID of loading queue in waiting area */
    int qidToCaptainB = 0;      /* ID of first external waiting lane */
    int qidToCaptainC = 0;      /* ID of second external waiting lane */
    int qidToVehicle = 0;       /* ID of queue used to send messages to */
                                /* vehicles */
    int waitingLane;            /* Lane that new arrivals are entering */
    int emptyLane;  


    /* counting of cars and trucks to determine full ferry load */
    int loads = 0;              /* # of loads of vehicles delivered*/
    int trucksOn;               /* Number of trucks that have been loaded */
    int fullSpotsOnFerry = 0;  



    /*  Initialize message buffer and start time */
    buf.arrivalInfo[0] = 0;  /* arrival time */
    buf.arrivalInfo[1] = 0;  /* car and truck fullSpotsOnFerryer */
    length = sizeof(mess_t) - sizeof(long);
    gettimeofday(&startTime, NULL);
    
    printf("Please enter integer values for the following variables\n");
    printf("Enter the %% probability that the next vehicle is a truck\n");
    scanf("%d", &truckArrivalProb );
    printf("Enter the maximum length of the interval between vehicles\n");
    scanf("%d", &maxTimeToNextArrival );


    /* Create 4 queues */
    /* 1 from captain to vehicles, 3 from vehicles captain */
    
    /* Each queue from the vehicles to the capatain represents a lane */
    /* in the ferry waiting area. There is one lane, called the loading */
    /* lane inside the waiting area. The loading lane this lane holds the */
    /* vehicles that arrived since the previous ferry departed */
    /* There are two lanes, called the empty lane and the waiting lane */
    /* outside of the loading area. Arriving Vehicles are added to the */
    /* end of the loading lane */
    /* When loading message is received the gate to the loading area open */
    /*     1)the vehicles from the waiting lane are permitted to join the*/
    /*       end of the loading lane. */
    /*      2)Any further vehicles that arrive wait in the empty lane */
    /*      3)When all vehicles from the loading lane have entered the  */
    /*        loading area the gates are closed. */
    /*      4)The empty lane becomes the waiting lane, the waiting lane */
    /*        becomes the empty lane */
    /*      5)The loading of vehicles from the loading area begins */
    /*      6)If there are not enough vehicles for a load in the waiting */
    /*        area the gates are opened and the cars and trucks needed */
    /*        to complete filling the ferry are taken from the front of */
    /*        the queue in the waiting lane */


    /* Create queues */
    /* Be sure to verify if the queue has been created and to */
    /* clean up and terminate the program if the queue is not created */


    qidToCaptainA = msgget(IPC_PRIVATE, IPC_CREAT | IPC_EXCL | 0660);
    if(qidToCaptainA == -1) {
        printf("Loading queue to captain not created\n");
        exit(0);
    }
    printf("Qid for loading lane to captain %d \n", qidToCaptainB); 


    qidToCaptainB = msgget(IPC_PRIVATE, IPC_CREAT | IPC_EXCL | 0660);
    if(qidToCaptainB <= 0) {
        printf("First outside queue to captain not created\n");
        msgctl(qidToCaptainA, IPC_RMID, 0);
        exit(0);
    }
    printf("Qid for initial waiting lane to captain %d \n", qidToCaptainB); 

    qidToVehicle = msgget(IPC_PRIVATE, IPC_CREAT | IPC_EXCL | 0660);
    if(qidToVehicle <= 0) {
        printf("Queue to vehicles not created\n");
        msgctl(qidToCaptainA, IPC_RMID, 0);
        msgctl(qidToCaptainB, IPC_RMID, 0);
        exit(0);
    }
    printf("Qid from captain to vehicles %d \n", qidToVehicle); 


    qidToCaptainC = msgget(IPC_PRIVATE, IPC_CREAT | IPC_EXCL | 0660);
    if(qidToCaptainC <= 0) {
        printf("Queue C to captain not created\n");
        msgctl(qidToCaptainA, IPC_RMID, 0);
        msgctl(qidToCaptainB, IPC_RMID, 0);
        msgctl(qidToVehicle, IPC_RMID, 0);
        exit(0);
    }
    printf("Qid for initial empty lane to captain %d \n", qidToCaptainB); 

    /* Set initial waiting lane to be waiting lane */
    waitingLane = qidToCaptainB;


    /* Spawn the vehicle process */
    if(!(pid = fork())){
        printf("Vehicle process - QID = %d\n", qidToVehicle);
        srand (time(0));
        while(1) { 

            /* If termination message is received from the captain */
            /* Reply to the captain to indicate you heard then exit program*/
            if(msgrcv(qidToVehicle, &buf, length, TERMINATION, IPC_NOWAIT) 
             != -1 ) {
                msgsnd(qidToCaptainA, &buf, length, 0);
                break;
            }


            /* If present time is later than arrival time of the next vehicle */
            /*         Determine if the vehicle is a car or truck */
            /*         Have the vehicle put a message in the queue indicating */
            /*         that it is in line */
            /* Then determine the arrival time of the next vehicle */ 
      
                
                        /* This is a car */
                        buf.mtype = CAR_ARRIVING;
                        buf.arrivalInfo[0] = lastArrivalTime;
                        buf.arrivalInfo[1]++; 
                        printf("Vehicle - MSG FROM CAR NUMBER %d: %ld %d\n", 
                        buf.arrivalInfo[1], buf.mtype, buf.arrivalInfo[0]);
						msgsnd(waitingLane, &buf, length, 0);
                       
					   /* This is a truck */
                        buf.mtype = TRUCK_ARRIVING;
                        buf.arrivalInfo[0] = lastArrivalTime;
                        buf.arrivalInfo[1]++;
                        printf("Vehicle - MSG FROM TRUCK NUMBER %d: %ld %d\n",
                        buf.arrivalInfo[1], buf.mtype, buf.arrivalInfo[0]);
                    }
                    
                printf("present time %d, next arrival time %d\n", 
                elapsed, lastArrivalTime);
            }


            if(msgrcv(qidToVehicle, &buf, length, START_LOADING, IPC_NOWAIT) 
                != -1 ) {
                /* Captain has sent a message that loading should begin */
                /* This message has just been received. */
                /* Have any further vehicles that arrive wait in the */
                /* empty lane by making the waiting lane the empty lane */
                /* and making the empty lane the waiting lane */
                captainState = DOCKED_LOADING;
                printf("Captain state is DOCKED_LOADING\n");
                emptyLane = waitingLane;
                if(waitingLane == qidToCaptainB ) {
                    waitingLane = qidToCaptainC;
                }
                else {
                    waitingLane = qidToCaptainB;
                }

                /* gate is open, new vehicles are going into the waiting lane */
                /* let vehicles in the empty lane join the loading queue */
                while( msgrcv(emptyLane, &buf, length, 0, IPC_NOWAIT) != -1) {
                    msgsnd(qidToCaptainA, &buf, length, 0);
                }

                /* Tell captain vehicles are ready to load */
                buf.mtype = START_LOADING;
                printf("msg received by queue: switching\n");
                msgsnd(qidToCaptainA, &buf, length, 0);
            }

    
            /* This can only happen after Captain has been told vehicles */
            /* are ready to load and before the captain is SAILING*/
            if(captainState == DOCKED_LOADING ) {    
                /* The captain must also have sent a message indicating this */
                /* vehicle should load. The vehicle process replys with a */
                /* message indicating that the vehicle is loaded */
                if(msgrcv(qidToVehicle, &buf, length, CAR_LOADING, IPC_NOWAIT)
                    != -1) {
                    buf.mtype = CAR_LOADED;
                    printf("Car loaded\n");
                    msgsnd(qidToCaptainA, &buf, length, 0);
                }
                if(msgrcv(qidToVehicle, &buf, length, TRUCK_LOADING, IPC_NOWAIT)
                    != -1 ) {
                    buf.mtype = TRUCK_LOADED;
                    printf("Truck loaded\n");
                    msgsnd(qidToCaptainA, &buf, length, 0);
                }


                /* When all vehicles in the load have replied to the captain, */
                /* and the captain has sent the vehicle process a message */
                /* indicating that the ferry is traveling across the river */
                /* then this process (the vehicles) will receive that message */
                /* and reply indicating that it knows that loading is complete*/
                /* and the ferry is sailing */
		/* The ferry can sail when this ACK is received */
                if(msgrcv(qidToVehicle, &buf, length, LOADING_COMPLETE, 
                    IPC_NOWAIT) != -1 ) {
                    buf.mtype = LOADING_COMPLETE_ACK;
                    captainState = SAILING;
                    printf("Captain state is SAILING\n");
                    msgsnd(qidToCaptainA, &buf, length, 0);
                }
            }

            
            /* Captain has sent a message that unloading should begin */
            /* This message has just been received. */
	    /* reply to captain indicating message was received */
            if(msgrcv(qidToVehicle, &buf, length, FERRY_ARRIVED,
                IPC_NOWAIT) != -1 ) {
                captainState = DOCKED_UNLOADING;
                fullSpotsOnFerry = 0;
                printf("Captain state is DOCKED_UNLOADING\n");
                buf.mtype = FERRY_ARRIVED_ACK;
                msgsnd(qidToCaptainA, &buf, length, 0);
            }


 :
                if(msgrcv(qidToVehicle, &buf, length, UNLOADING_COMPLETE, 
                    IPC_NOWAIT) != -1 ) {
                    buf.mtype = UNLOADING_COMPLETE_ACK;
                    captainState = SAILING_BACK;
                    printf("Captain state is SAILING_BACK\n");
                    msgsnd(qidToCaptainA, &buf, length, 0);
                }
            }


	    /* received message that ferry has arrived at loading area */
	    /* reply to captain indicating message was received */
            if(msgrcv(qidToVehicle, &buf, length, FERRY_RETURNED, 
            IPC_NOWAIT) != -1 ) {
                captainState = DOCKED_LOADING;
                printf("Captain state is DOCKED_LOADING\n");
                buf.mtype = FERRY_RETURNED_ACK;
                msgsnd(qidToCaptainA, &buf, length, 0);
            }

        }
        exit(0);
    }
    /* End of Vehicle Process */



    /* Beginning captain process */
    while (1) {
        /* Send a message to the vehicles that loading is about to begin */
        /* Opening loading area gates */
        buf.mtype = START_LOADING;
        msgsnd(qidToVehicle, &buf, length, 0);
        msgrcv(qidToCaptainA, &buf, length, START_LOADING, 0);
        /* all cars from the waiting lane are now in the loading queue */ 
        /* The gate is closed */
        /* make captain remember which lane outside the loading area */
        /* is the empty lane and which is the waiting lane */ 
        if(waitingLane == qidToCaptainB ) {
            waitingLane = qidToCaptainC;
        }
        else {
            waitingLane = qidToCaptainB;
        }


        /* Signal enough vehicles to fill the ferry */
        /* To signal remove message from the CaptainA queue */
        /* Then send a message to vehicle process asking that vehicle to load */
        captainState = DOCKED_LOADING;
        printf("                                            ");
        printf("captainState is DOCKED_LOADING\n");
        trucksOn = 0;
            fullSpotsOnFerry = 0;
        for ( k = 0; k < 2; k++ ) {
            if(msgrcv(qidToCaptainA, &buf, length, TRUCK_ARRIVING, 
            IPC_NOWAIT) != -1) {
                printf("                                            ");
                printf("CAPTAIN - TRUCK NUMBER %d: %ld %d\n", 
                    buf.arrivalInfo[1], buf.mtype, buf.arrivalInfo[0]);
                    fullSpotsOnFerry+=2;
                    trucksOn++;
                buf.mtype = TRUCK_LOADING;
                msgsnd(qidToVehicle, &buf, length, 0);
            }
        }
        for( k = 0; k < TOTAL_SPOTS_ON_FERRY; k++ ) {
            if(msgrcv(qidToCaptainA, &buf, length, CAR_ARRIVING, 
            IPC_NOWAIT) != -1) {
                printf("                                            ");
                printf("CAPTAIN - CAR NUMBER %d: %ld %d\n",
                    buf.arrivalInfo[1], buf.mtype, buf.arrivalInfo[0]);
                    fullSpotsOnFerry++;
                buf.mtype = CAR_LOADING;
                msgsnd(qidToVehicle, &buf, length, 0);
                if(fullSpotsOnFerry == 6) break;
            }
        }
        while( fullSpotsOnFerry < 6 ) {
                if( trucksOn == 2 || fullSpotsOnFerry == 5 ){
                msgType = 1;
            }
            else {
                msgType = 0;
            }
            msgrcv(waitingLane, &buf, length, msgType, 0);
            printf("                                            ");
            printf("CAPTAIN NEW ARRIVALS - MESSAGE NUMBER %d: %ld %d\n",
            buf.arrivalInfo[1], buf.mtype, buf.arrivalInfo[0]);
            fullSpotsOnFerry += buf.mtype;
            if(buf.mtype == 2) trucksOn++;
            buf.mtype += 2;
            msgsnd(qidToVehicle, &buf, length, 0);
            if(fullSpotsOnFerry == 6) break;
        }
        
        printf("                                            ");
        printf("Waiting for vehicles to load\n");
        /* Enough vehicles to fill the ferry have been signalled */
        /* Wait for replies indicating the vehicles have loaded */
        /* As each such reply message arrives reply with a message telling */
        /* the vehicle process that the vehicle is about to begin travelling */
        /* across the river */
        fullSpotsOnFerry = 0;
        while( fullSpotsOnFerry < 6) {    
            if(msgrcv(qidToCaptainA, &buf, length, CAR_LOADED, 
            IPC_NOWAIT) != -1 ) {
                printf("                                            ");
                printf("Captain knows car loaded\n");
                fullSpotsOnFerry++; 
                buf.mtype = CAR_TRAVELING;
                msgsnd(qidToVehicle, &buf, length, 0);
            }
            if(msgrcv(qidToCaptainA, &buf, length, TRUCK_LOADED, 
            IPC_NOWAIT) != -1 ) {
                printf("                                            ");
                printf("aptain knows truck loaded\n");
                    fullSpotsOnFerry+=2; 
                buf.mtype = TRUCK_TRAVELING;
                msgsnd(qidToVehicle, &buf, length, 0);
            }
        }
        printf("                                            ");
        printf("Captain knows ferry is full\n");


        /* When the captian knows that all vehicles signalled have completed */
        /* loaded onto the ferry he sends a message to the vehicle process */
        /* that the ferry is now fully loaded */
        /* He waits for a response before continuing */
        buf.mtype = LOADING_COMPLETE;
        msgsnd(qidToVehicle, &buf, length, 0);
        msgrcv(qidToCaptainA, &buf, length, LOADING_COMPLETE_ACK, 0);
        printf("                                            ");
        printf(
        "Captain notified: vehicle process knows all vehicles are loaded\n");

        /* When the captain receives the reply to the fully loaded message  */
        /* from the vehicle process he sets sail across the river */
        captainState = SAILING;
        printf("                                            ");
        printf("All vehicles for load %d are loaded, captain sets sail\n",
            loads);
        printf("                                            ");
        printf("captain state is SAILING\n");

        /* Wait while the ferry crosses the river */
        usleep(CROSSING_TIME);

        /* The ferry has crossed the river, docking at destination */
        /* Send a message to the vehicle process telling it the ferry */
        /* has docked. Wait for a response before continuing */
        printf("                                            ");
        printf("Ferry has crossed the  river, notifying vehicle\n");
        buf.mtype = FERRY_ARRIVED;
        msgsnd(qidToVehicle, &buf, length, 0);
        msgrcv(qidToCaptainA, &buf, length, FERRY_ARRIVED_ACK, 0);


        /* When the captain receives the reply to the docked at destination */
        /* message from the vehicle process he prepares to unload */
        captainState = DOCKED_UNLOADING;
        printf("                                            ");
        printf("Ferry has arrived at the destination\n");
        printf("                                            ");
        printf("captain state is DOCKED_UNLOADING\n");

        
        /* Signal vehicles to tell them they have arrived and should*/
        /* start unloading */
        printf("                                            ");
        printf("beginning to unload\n");
        fullSpotsOnFerry = 0;
        while( fullSpotsOnFerry < 6) {    
            if(msgrcv(qidToCaptainA, &buf, length, CAR_ARRIVED, 
            IPC_NOWAIT) != -1 ) {
                printf("                                            ");
                printf("Car unloading\n");
                    fullSpotsOnFerry++; 
                buf.mtype = CAR_UNLOADING;
                msgsnd(qidToVehicle, &buf, length, 0);
            }
            if(msgrcv(qidToCaptainA, &buf, length, TRUCK_ARRIVED, 
            IPC_NOWAIT) != -1 ) {
                printf("                                            ");
                printf("Truck unloading\n");
                    fullSpotsOnFerry+=2; 
                buf.mtype = TRUCK_UNLOADING;
                msgsnd(qidToVehicle, &buf, length, 0);
            }
        }


        /* Wait for replys indicating the vehicles have unloaded */
        /* When all vehicles have replied the captain knows the ferry */
        /* is empty */
        fullSpotsOnFerry = 0;
        while( fullSpotsOnFerry < 6) {    
            if(msgrcv(qidToCaptainA, &buf, length, CAR_UNLOADED, 
            IPC_NOWAIT) != -1 ) {
                printf("                                            ");
                printf("Captain knows car is unloaded\n");
                    fullSpotsOnFerry++; 
            }
            if(msgrcv(qidToCaptainA, &buf, length, TRUCK_UNLOADED, 
            IPC_NOWAIT) != -1 ) {
                printf("                                            ");
                printf("Captain knows truck is unloaded\n");
                    fullSpotsOnFerry+=2; 
            }
        }
        printf("                                            ");
        printf("Captain knows ferry is empty\n");


        /* Then the ferry can leave the destination dock and return to the */
        /* loading dock */
        /* the captain signals the vehicle process to tell it the ferry is */
        /* on the way back to the loading dock*/
        buf.mtype = UNLOADING_COMPLETE;
        captainState = SAILING_BACK;
        printf("                                            ");
        printf("Captain state is SAILING_BACK\n");
        msgsnd(qidToVehicle, &buf, length, 0);
        msgrcv(qidToCaptainA, &buf, length, UNLOADING_COMPLETE_ACK, 0);
        printf("                                            ");
        printf("Captain is sailing back to loading dock\n");

        /* Wait while the ferry crosses the river */
        usleep(CROSSING_TIME);

        buf.mtype = FERRY_RETURNED;
        captainState = DOCKED_LOADING;
        printf("                                            ");
        printf("Captain state is DOCKED_LOADING\n");
        msgsnd(qidToVehicle, &buf, length, 0);
        msgrcv(qidToCaptainA, &buf, length, FERRY_RETURNED_ACK, 0);
        printf("                                            ");
        printf("Ferry has returned for another load\n");

        if (loads >= MAX_LOADS ){
            break;
        }
        loads++;
    }
    buf.mtype = TERMINATION;
    msgsnd(qidToVehicle, &buf, length, 0);
    msgrcv(qidToCaptainA, &buf, length, TERMINATION, 0);
    msgctl(qidToCaptainA, IPC_RMID, 0);
    msgctl(qidToCaptainB, IPC_RMID, 0);
    msgctl(qidToCaptainC, IPC_RMID, 0);
    msgctl(qidToVehicle, IPC_RMID, 0);
    kill(pid, SIGKILL);
    return 0;
}
    
int timeChange( const struct timeval startTime )
{
    struct timeval nowTime;
    long int elapsed;
    int elapsedTime;

    gettimeofday(&nowTime, NULL);
    elapsed = (nowTime.tv_sec - startTime.tv_sec) * 1000000
            + (nowTime.tv_usec - startTime.tv_usec);
    elapsedTime = elapsed / 1000;
    return elapsedTime;
}