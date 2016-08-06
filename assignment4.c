#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <signal.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/types.h>
#include <sys/time.h>

//define all possible types of messages that can be exchanged
#define CAR_IN_ARRIVAL_QUEUE 17 //car in arrival/late arrival queue
#define TRUCK_IN_ARRIVAL_QUEUE 18 //truck in arrival/late arrival queue
#define CAR_READY_TO_LOAD 1 //car in loading queue
#define TRUCK_READY_TO_LOAD 2 //truck in loading queue
#define START_LOADING_CAR 3
#define START_LOADING_TRUCK 4
#define FINISH_LOADING_CAR 5
#define FINISH_LOADING_TRUCK 6
#define START_UNLOADING_CAR 7
#define START_UNLOADING_TRUCK 8
#define FINISH_UNLOADING_CAR 9
#define FINISH_UNLOADING_TRUCK 10
#define FERRY_FINISHED_LOADING 11
#define FERRY_FINISHED_UNLOADING 12
#define FERRY_FINISHED_LOADING_ACK 13
#define FERRY_FINISHED_UNLOADING_ACK 14
#define FERRY_ARRIVED_AT_DESTINATION 15
#define FERRY_ARRIVED_AT_DESTINATION_ACK 20
#define FERRY_READY_TO_LOAD 16
#define FERRY_READY_TO_LOAD_ACK 17
#define FERRY_RETURNED 21
#define FERRY_RETURNED_ACK 22
#define EXIT 20

//define what data is exchanged in messages
typedef struct {
	
	long mtype; //type of message being exchanged
	int vehicleId; //id of vehicle that is loaded/unloaded, only used for certain types of messages
	
} Message;

//create a message that needs to be exchanged
Message message;

//id of each type of message queue
//this queue contains all messages that the captain sends to the vehicle
int idMessageFromCaptain;

//message queue from vehicle to captain, for acknowledgements
int idMessageVehicleToCaptain;

/*Message queues from vehiles to captain
vehicles arrive at the loading port and send messages to the arrival queue
when ferry begins loading, vehicles from the arrival queue will be placed into the loading queue.
vehicles from the loading queue will be loaded into the ferry.
Any vehicles that arrive once the ferry beings loading will be placed into the late arrival queue.
once the ferry is finished loading, the late arrival queue becomes the arrival queue, and the arrival queue becomes the late arrival queue.
*/
int idMessageVehicleArrivalQueue;
int idMessageVehicleLateArrivalQueue;
int idMessageVehicleLoadingQueue;

//length of message being exchanged
int messageLength;

//probablity of a truck arriving, as a percent
int probabiltyOfTruckArriving;

//maximum time interval between two vehicle arrivals, in MILLISECONDS
int maxTimeBetweenArrivals;

int vehicleProcessId = 0;

//calculate the elapsed time, in seconds, given the initial time
float calculateElapsedTime(struct timeval initialTime);
void initialize();//setup
void terminate();//takedown
void vehicle();
void captain();

int main() {
	
	srand(time(0));
	//let user specify parameters
	printf("Specify the probability of a truck arriving, as a percentage: ");
	scanf("%d", &probabiltyOfTruckArriving);
	
	printf("Specify the max time interval between two vehicle arrivals, in milliseconds: ");
	scanf("%d", &maxTimeBetweenArrivals);
	
	initialize();
	
	//create the vehicle
	int pid = fork();
	
	if(pid == 0) {
		
		//child, its vehicle
		vehicle();
		return 0;
	}
	
	vehicleProcessId = pid;
	captain();

	terminate();
	
	return 0;
}

float calculateElapsedTime(struct timeval initialTime) {
	
	//get current time
	struct timeval currentTime;
	gettimeofday(&currentTime, 0);
	
	float elapsedSeconds = currentTime.tv_sec - initialTime.tv_sec;
	float elapsedMicroseconds = currentTime.tv_usec - initialTime.tv_usec;
	
	//convert time to seconds and return
	return elapsedSeconds + elapsedMicroseconds / 1000000;
}

void initialize() {
	
	printf("INITIALIZING.\n");
	
	messageLength = sizeof(Message) - sizeof(long);
	
	//default vehicle id
	message.vehicleId = 1;
	
	//create the required queues
	idMessageFromCaptain = msgget(IPC_PRIVATE, IPC_CREAT | IPC_EXCL | 0660);
	idMessageVehicleToCaptain = msgget(IPC_PRIVATE, IPC_CREAT | IPC_EXCL | 0660);
	idMessageVehicleArrivalQueue = msgget(IPC_PRIVATE, IPC_CREAT | IPC_EXCL | 0660);
	idMessageVehicleLateArrivalQueue = msgget(IPC_PRIVATE, IPC_CREAT | IPC_EXCL | 0660);
	idMessageVehicleLoadingQueue = msgget(IPC_PRIVATE, IPC_CREAT | IPC_EXCL | 0660);
}

void terminate() {
	
	printf("TERMINATING.\n");
	
	//tell vehicles the simulation is finished
	message.mtype = EXIT;
	msgsnd(idMessageFromCaptain, &message, messageLength, 0);
	msgrcv(idMessageVehicleToCaptain, &message, messageLength, EXIT, 0);//wait for confirmation from vehicle about closing the program
	
	//remove all the queues
	msgctl(idMessageFromCaptain, IPC_RMID, 0);
	msgctl(idMessageVehicleToCaptain, IPC_RMID, 0);
	msgctl(idMessageVehicleArrivalQueue, IPC_RMID, 0);
	msgctl(idMessageVehicleLateArrivalQueue, IPC_RMID, 0);
	msgctl(idMessageVehicleLoadingQueue, IPC_RMID, 0);
	
	//kill the vehicle
	kill(vehicleProcessId, SIGKILL);
}

void vehicle() {
	
	//calculate interval between vehicle spawns, in seconds
	float nextVehicleSpawnTime = (rand() % maxTimeBetweenArrivals) / 1000.f;
	
	struct timeval timeOfLastVehicleSpawn;
	gettimeofday(&timeOfLastVehicleSpawn, 0);
	
	int vehicleId = 1;
	while(1) {
		
		if(msgrcv(idMessageFromCaptain, &message, messageLength, EXIT, IPC_NOWAIT) != -1) {
			
			printf("Terminated\n");
			//captain says kill program, send acknowledgement
			msgsnd(idMessageVehicleToCaptain, &message, messageLength, 0);
			return;
		}
		
		float elapsedTimeSinceLastSpawn = calculateElapsedTime(timeOfLastVehicleSpawn);
		
		//check if its time to spawn a vehicle
		if(elapsedTimeSinceLastSpawn > nextVehicleSpawnTime) {
			
			//determine whether we should spawn a truck or a car
			int spawnNumber = rand() % 100 + 1;
			
			message.vehicleId = vehicleId++;
			
			//spawn a truck
			if(spawnNumber <= probabiltyOfTruckArriving) {
				
				//place truck in arrival lane
				message.mtype = TRUCK_IN_ARRIVAL_QUEUE;
				msgsnd(idMessageVehicleArrivalQueue, &message, messageLength, 0);
				printf("Truck number %d has arrived at the dock.\n", message.vehicleId);
				
			} else {//spawn a car
				
				message.mtype = CAR_IN_ARRIVAL_QUEUE;
				msgsnd(idMessageVehicleArrivalQueue, &message, messageLength, 0);
				printf("Car number %d has arrived at the dock.\n", message.vehicleId);
			}
			
			nextVehicleSpawnTime = (rand() % maxTimeBetweenArrivals) / 1000.f;
			gettimeofday(&timeOfLastVehicleSpawn, 0);
		}
		
		//check if captain wants ferry to load
		if(msgrcv(idMessageFromCaptain, &message, messageLength, FERRY_READY_TO_LOAD, IPC_NOWAIT) != -1) {
			
			//move all vehicles to loading queue
			while(msgrcv(idMessageVehicleArrivalQueue, &message, messageLength, TRUCK_IN_ARRIVAL_QUEUE, IPC_NOWAIT) != -1) {
				
				message.mtype = TRUCK_READY_TO_LOAD;
				msgsnd(idMessageVehicleLoadingQueue, &message, messageLength, 0);
				printf("Truck number %d is ready for loading.\n", message.vehicleId);
			}
			
			while(msgrcv(idMessageVehicleArrivalQueue, &message, messageLength, CAR_IN_ARRIVAL_QUEUE, IPC_NOWAIT) != -1) {
				
				message.mtype = CAR_READY_TO_LOAD;
				msgsnd(idMessageVehicleLoadingQueue, &message, messageLength, 0);
				printf("Car number %d is ready for loading.\n", message.vehicleId);
			}
			
			printf("Swapping arrival queue and late arrival queue.\n");
			
			/*swap the arrival queue and the late arrival queue.
			the queues are swaped here, instead of after the ferry finishes loading because if a vehicle arrives while the ferry is loading
			it needs to go to the late arrival queue, which means a separate flag needs to be checked when a vehicle arrives in order to
			determine if the vehicle should be placed into the arrival queue, or the late arrival queue.
			if we swap the queues here then we can always put new vehicles in the arrival queue since 
			late arrivals automatically end up in an empty queue*/
			int temp = idMessageVehicleArrivalQueue;
			idMessageVehicleArrivalQueue = idMessageVehicleLateArrivalQueue;
			idMessageVehicleLateArrivalQueue = temp;
			
			//tell captain vehicles are ready for loading
			message.mtype = FERRY_READY_TO_LOAD_ACK;
			msgsnd(idMessageVehicleToCaptain, &message, messageLength, 0);
		}
		
		//if captain calls a vehicle to load, then send a confirmation
		if(msgrcv(idMessageFromCaptain, &message, messageLength, START_LOADING_TRUCK, IPC_NOWAIT) != -1) {
			
			message.mtype = FINISH_LOADING_TRUCK;
			msgsnd(idMessageVehicleToCaptain, &message, messageLength, 0);
			printf("Loaded truck number %d into the ferry.\n", message.vehicleId);
		}
		
		if(msgrcv(idMessageFromCaptain, &message, messageLength, START_LOADING_CAR, IPC_NOWAIT) != -1) {
			
			message.mtype = FINISH_LOADING_CAR;
			msgsnd(idMessageVehicleToCaptain, &message, messageLength, 0);
			printf("Loaded Car number %d into the ferry.\n", message.vehicleId);
		}
		
		if(msgrcv(idMessageFromCaptain, &message, messageLength, FERRY_FINISHED_LOADING, IPC_NOWAIT) != -1) {
			
			printf("Confirming ferry finished loading\n");
			message.mtype = FERRY_FINISHED_LOADING_ACK;
			msgsnd(idMessageVehicleToCaptain, &message, messageLength, 0);
		}
		
		if(msgrcv(idMessageFromCaptain, &message, messageLength, FERRY_ARRIVED_AT_DESTINATION, IPC_NOWAIT) != -1) {
			
			printf("confirming ferry arrived at destination\n");
			message.mtype = FERRY_ARRIVED_AT_DESTINATION_ACK;
			msgsnd(idMessageVehicleToCaptain, &message, messageLength, 0);
		}
		
		//received request to unload a vehicle, send confirmation
		if(msgrcv(idMessageFromCaptain, &message, messageLength, START_UNLOADING_TRUCK, IPC_NOWAIT) != -1) {
			
			message.mtype = FINISH_UNLOADING_TRUCK;
			msgsnd(idMessageVehicleToCaptain, &message, messageLength, 0);
			printf("Unloaded a truck\n");
		}
		
		if(msgrcv(idMessageFromCaptain, &message, messageLength, START_UNLOADING_CAR, IPC_NOWAIT) != -1) {
			
			message.mtype = FINISH_UNLOADING_CAR;
			msgsnd(idMessageVehicleToCaptain, &message, messageLength, 0);
			printf("Unloaded a car\n");
		}
		
		if(msgrcv(idMessageFromCaptain, &message, messageLength, FERRY_FINISHED_UNLOADING, IPC_NOWAIT) != -1) {
			
			printf("Confirming ferry finished unloading.\n");
			message.mtype = FERRY_FINISHED_UNLOADING_ACK;
			msgsnd(idMessageVehicleToCaptain, &message, messageLength, 0);
		}
		
		if(msgrcv(idMessageFromCaptain, &message, messageLength, FERRY_RETURNED, IPC_NOWAIT) != -1) {
			
			printf("Confirming ferry has returned.\n");
			message.mtype = FERRY_RETURNED_ACK;
			msgsnd(idMessageVehicleToCaptain, &message, messageLength, 0);
		}
	}
	/* create a timer
	
	while(forever) {
		
		if captain sends terminate message
			send acknowledgement
			exit
		
		if its time to spawn a vehicle
			determine what type of vehicle to spawn
			spawn vechicle by sending message to vechileArrivalQueue
			reset spawn timer
		
		if captain says ferry is ready to load
			
			move vehicles from arrivalQueue to loading queue
			
			swap the arrival queue and the late arrival queue
			the queues are swaped here, instead of after the ferry finishes loading because if a 
			vehicle arrives while the ferry is loading it needs to go to the late arrival queue, which means a separate flag needs to be checked when a vehicle arrives in order to determine
			if the vechile should be placed into the arrival queue, or the late arrival queue.
			if we swap the queues here then we can always put new vehicles in the arrival queue since 
			late arrivals automatically end up in an empty queue
			
			tell captain the vehicles are ready to load
			
		
		if a message to load a vechile arrives
			send a confirmation indicating the vehicle finished loading
			
		if captain says the ferry finished loading
			
			send a confirmation that message was received
		
		if captin says ferry arrived at destination
			send a confirmation that message was received
		
		if captain says to unload a vechile
			send a confirmation indicating the vehicle unloaded
		
		if captain says ferry finished unloading
			send confirmation
		
		if captain says the ferry has returned
			send confirmation
		
	} */
}

void captain() {
	
	sleep(3);
	
	int numberOfLoadsTransported = 0;
	while(numberOfLoadsTransported < 10) {
		
		//begin loading ferry
		printf("Ferry is ready to load\n");
		message.mtype = FERRY_READY_TO_LOAD;
		msgsnd(idMessageFromCaptain, &message, messageLength, 0);
		msgrcv(idMessageVehicleToCaptain, &message, messageLength, FERRY_READY_TO_LOAD_ACK, 0);
		
		//swap arrival and late arrival queue ids because vehicles do this
		int temp = idMessageVehicleArrivalQueue;
		idMessageVehicleArrivalQueue = idMessageVehicleLateArrivalQueue;
		idMessageVehicleLateArrivalQueue = temp;
		
		int fullSpotsOnFerry = 0;
		int numTrucksOnFerry = 0;
		
		//ready to load vehicles, first load as many trucks as possible from the loading queue
		while(numTrucksOnFerry < 2 && msgrcv(idMessageVehicleLoadingQueue, &message, messageLength, TRUCK_READY_TO_LOAD, IPC_NOWAIT) != -1) {
			
			//truck available, tell it to load
			message.mtype = START_LOADING_TRUCK;
			printf("Begin loading truck number %d\n", message.vehicleId);
			msgsnd(idMessageFromCaptain, &message, messageLength, 0);
			numTrucksOnFerry++;
			fullSpotsOnFerry += 2;
		}
		
		//load as many cars as possible from loading queue
		while(fullSpotsOnFerry < 6 && msgrcv(idMessageVehicleLoadingQueue, &message, messageLength, CAR_READY_TO_LOAD, IPC_NOWAIT) != -1) {
			
			//car available, tell it to load
			message.mtype = START_LOADING_CAR;
			printf("Begin loading car number %d\n", message.vehicleId);
			msgsnd(idMessageFromCaptain, &message, messageLength, 0);
			fullSpotsOnFerry += 1;
		}
		
		//if ferry isn't fully loaded, accept the late cars into ferry
		//the late cars should have arrived into the late arrival queue
		while(fullSpotsOnFerry < 6) {
		
			//look for trucks if theres room for trucks
			if(numTrucksOnFerry < 2 && fullSpotsOnFerry < 5 &&
				msgrcv(idMessageVehicleArrivalQueue, &message, messageLength, TRUCK_IN_ARRIVAL_QUEUE, IPC_NOWAIT) != -1) {
				
				printf("Begin loading LATE ARRIVAL truck number %d\n", message.vehicleId);
				message.mtype = START_LOADING_TRUCK;
				msgsnd(idMessageFromCaptain, &message, messageLength, 0);
				numTrucksOnFerry++;
				fullSpotsOnFerry += 2;
				
			} else if(msgrcv(idMessageVehicleArrivalQueue, &message, messageLength, CAR_IN_ARRIVAL_QUEUE, IPC_NOWAIT) != -1) {
				
				printf("Begin loading LATE ARRIVAL car number %d\n", message.vehicleId);
				message.mtype = START_LOADING_CAR;
				msgsnd(idMessageFromCaptain, &message, messageLength, 0);
				fullSpotsOnFerry += 1;
			}
		}
		
		//wait for confirmation messages
		printf("Confirming that vehicles have loaded.\n");
		fullSpotsOnFerry = 0;
		
		while(fullSpotsOnFerry < 6) {
			
			if(msgrcv(idMessageVehicleToCaptain, &message, messageLength, FINISH_LOADING_CAR, IPC_NOWAIT) != -1) {
				
				printf("Captain knows car number %d has finished loading.\n", message.vehicleId);
				fullSpotsOnFerry++;
			}
			
			if(msgrcv(idMessageVehicleToCaptain, &message, messageLength, FINISH_LOADING_TRUCK, IPC_NOWAIT) != -1) {
				
				printf("Captain knows truck number %d has finished loading.\n", message.vehicleId);
				fullSpotsOnFerry += 2;
			}
		}
		
		printf("All vehicles have been loaded. Informing port that ferry is fully loaded and ready to sail.\n");
		message.mtype = FERRY_FINISHED_LOADING;
		msgsnd(idMessageFromCaptain, &message, messageLength, 0);
		msgrcv(idMessageVehicleToCaptain, &message, messageLength, FERRY_FINISHED_LOADING_ACK, 0);
		
		printf("The ferry has left the port.\n");
		sleep(3);
		
		printf("Ferry has arrived at the destination.\n");
		message.mtype = FERRY_ARRIVED_AT_DESTINATION;
		msgsnd(idMessageFromCaptain, &message, messageLength, 0);
		msgrcv(idMessageVehicleToCaptain, &message, messageLength, FERRY_ARRIVED_AT_DESTINATION_ACK, 0);
		
		//unload trucks and cars
		//first unload all the trucks
		while(numTrucksOnFerry > 0) {
			
			printf("start unloading a truck from the ferry\n");
			message.mtype = START_UNLOADING_TRUCK;
			msgsnd(idMessageFromCaptain, &message, messageLength, 0);
			numTrucksOnFerry -= 1;
			fullSpotsOnFerry -= 2;
		}
		
		while(fullSpotsOnFerry > 0) {
			
			printf("start unloading a car from the ferry\n");
			message.mtype = START_UNLOADING_CAR;
			msgsnd(idMessageFromCaptain, &message, messageLength, 0);
			fullSpotsOnFerry -= 1;
		}
		
		//await confirmation of unloaded vehicles
		while(fullSpotsOnFerry < 6) {
			
			if(msgrcv(idMessageVehicleToCaptain, &message, messageLength, FINISH_UNLOADING_TRUCK, IPC_NOWAIT) != -1) {
				
				printf("captain knows a truck finished unloading.\n");
				fullSpotsOnFerry += 2;
			}
			
			if(msgrcv(idMessageVehicleToCaptain, &message, messageLength, FINISH_UNLOADING_CAR, IPC_NOWAIT) != -1) {
				
				printf("captain knows a car finished unloading.\n");
				fullSpotsOnFerry += 1;
			}
		}
		
		printf("Captain knows ship has finished unloading.\n");
		printf("Informing port that the ferry finished unloading and is ready to return.\n");
		
		message.mtype = FERRY_FINISHED_UNLOADING;
		msgsnd(idMessageFromCaptain, &message, messageLength, 0);
		msgrcv(idMessageVehicleToCaptain, &message, messageLength, FERRY_FINISHED_UNLOADING_ACK, 0);
		
		printf("Ferry sets sail.\n");
		sleep(3);
		
		printf("Ferry has returned to port.\n");
		message.mtype = FERRY_RETURNED;
		msgsnd(idMessageFromCaptain, &message, messageLength, 0);
		msgrcv(idMessageVehicleToCaptain, &message, messageLength, FERRY_RETURNED_ACK, 0);
	
		numberOfLoadsTransported++;
		printf("~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\n");
		printf("FERRY HAS FINISHED TRANSPORTING %d LOADS.\n", numberOfLoadsTransported);
		printf("~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\n");
		
		/* printf("Captain says terminate.\n");
		message.mtype = EXIT;
		msgsnd(idMessageFromCaptain, &message, messageLength, 0);
		break; */
	}
	
	printf("The ferry has transported 10 loads and has returned for the 11th.\n");
	
	
	/* while(forever) {
		
		break up the loop as follows
		
		top of the loop is for when ferry is at the dock, ready to load
		
		middle of loop is when ferry is loading vehicles
		
		middle part 2 of lopo is when ferry is unloading at the destination
		
		end of loop is when ferry is returning
		
		send message to vehicles that ferry is loading
		wait for acknowledgement
		
		time to begin loading
		go through trucks in loading queue and load up to two
		go through cars in loading queue and load as many as possible
		
		while there is still free space in ferry
			
			go through arrivalQueue (which should have switched with the lateArrivalQueue)
				
				load as many trucks as possible to fill missing trucks
				load as many cars as possible
		
		send message that ferry is done loading
		wait for acknowledgement
		
		begin sailing, sleep for however long
		
		arrived at destination, send message
		wait for acknowledgement
		
		send message to unload a vehicle, wait for acknowledgement from each one
		
		send message indicating unloading is complete
		wait for acknowledgement
		
		begin returning, lseep for however long
		
		send message indicating ferry has returned
		wait for acknowledgement
	} */
}