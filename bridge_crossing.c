// Your program should prevent collisions, deadlocks and the collapse of the bridge.
// Use pthread_cond_signal() for signaling any threads blocked on a condition variable. DO NOT USE BROADCAST
// pthread_join(threadId) = wait(), but waits until the thread with matching threadId terminates.
// pthread_create(ptrToThisPthread, NULL, func(whoseReturnTypeANDparamTypeIsVoidPtr)WhereThreadBeginsExecution, argsThatPthread'sFuncWillUse(typeIsVoidPtr))
	// https://stackoverflow.com/questions/6990888/c-how-to-create-thread-using-pthread-create-function
// pthread_cond_wait(&cond, &mutex)  ==  [pthread_mutex_unlock(&mutex), then wait for signal on $cond, then pthread_mutex_lock(&mutex)]
	// https://www.youtube.com/watch?v=0sVGnxg6Z3k&list=PLfqABt5AS4FmuQf70psXrsMLEDQXNkLq2&index=10
//Declaring two pointers:    struct movinglist *p1, *p2;
	//Declaring one pointer: struct movinglist* p1;   OR   struct movinglist *p1;
//Anytime a struct or a variable-size pointer is declared (made into an object or array (respectively) that uses allocated memory)
	//, it needs to eventually be freed. Otherwise, SEGMENTATION FAULT (i.e. memory leak) occurs.
	// int a[5]; doesn't need free(a) because it's not variable-size - it's fixed size.
// Waiting on threads (that haven't yet been created) via pthread_join() will cause a SEGMENTATION FAULT.
//OHHHH str MEANS struct
#include <stdio.h>
#include <pthread.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>


#define NORTHBOUND 0
#define SOUTHBOUND 1
#define TYPE_TRUCK 0
#define TYPE_CAR   1
#define MAX_NUM_VEHICLES 30
#define DEBUG 0
#define PRINT_LIST 1

#define useExecutableArgumentForOption 0










//Global structs and global variables (some of which are global struct variables (i.e. global objects in OOP terms))

//Having a waitinglist and movinglist prevents busy waiting.
typedef struct waitinglist
{
	int vehicle_id;
	int vehicle_type;	/*0 for truck, 1 for car;*/
	int direction;		/*0 for north, 1 for south*/
	struct  waitinglist* next;
} waitingvehiclelist;	/*This is a global variable named "waitingvehiclelist" with type "struct waitinglist"*/

typedef struct movinglist
{
	int vehicle_id;
	int vehicle_type;
	int direction;
	struct  movinglist* next;
} movingvehiclelist;	/*This is a global variable named "movingvehiclelist" with type "struct movinglist"*/

typedef struct pmstr
{
	int vehicle_id;
	int vehicle_type;
	int direction;
} pmstr_t;	/*This is a global variable named "pmstr_t" with type "struct pmstr"*/

pmstr_t vehicleList[MAX_NUM_VEHICLES];	//Declare (but not instantiate) fixed-sized array of Vehicles. Each vehicle has vehicle data but no associated TID.
//pmstr_t* vehicleList;	//Declare (but not instantiate) variable-sized array of Vehicles. Each vehicle has vehicle data but no associated TID.
						//  Couldn't use this because it caused a SEGMENTATION FAULT before I malloc()'d it. Must allocate new memory before assigning.

struct waitinglist* pw; //pointer to the waiting list
struct movinglist*  pm; //pointer to the moving list

//TID = ThreadIdentifier (not an official library thing, just something I assign to each thread)
pthread_t vehicleTIDs[MAX_NUM_VEHICLES];	//Declare (but not instantiate) array of pthreads.

int i,j;		//Loop variables
int numWaitingCarsSouthbound, numWaitingCarsNorthbound, numWaitingTrucksSouthbound, numWaitingTrucksNorthbound;
int numMovingCars, numMovingTrucks;
int currentmovingdir, previousmovingdir; //0: north, 1: south
int firstVehicleHasCrossed;
pthread_mutex_t lock;
pthread_cond_t TruckNorthboundMovable, TruckSouthboundMovable, CarNorthboundMovable, CarSouthboundMovable;


void createVehicleAndThreadAndArrive(float probabilityOfCreatingCar, int numVehiclesToCreate, int startingArrayIndex_TID);
void* vehicle_routine(/* pmstr_t* */ void* pmstrpara);	//vehicle_type: 0 for truck, 1 for car;
                                    					//direction:  0 for north, 1 for south;
/* ^Must have return type of void* instead of just void because pthreads require function return types of void* */
void vehicle_arrival(pmstr_t* pmstrpara);
/*The 2 function declarations above use the globally-instantiated struct variable named "pmstr_t"
  (find it immediately after where "typedef struct pmstr" is defined)*/
void waitinglistinsert(int vehicle_id,int vehicle_type, int direction);
void waitinglistdelete(int vehicle_id);
void movinglistinsert(int vehicle_id, int vehicle_type, int direction);
void movinglistdelete(int vehicle_id);

void printmoving(const bool printList);
void printwaiting(const bool printList);


int main(int numExecArgs, char** strAryOfExecArgs)
{
	int option;
	float carprob;
	int vehicle_type, direction, vehicle_id;

	
	pthread_mutex_init(&lock, NULL);
	pthread_cond_init(&TruckNorthboundMovable, NULL);
	pthread_cond_init(&TruckSouthboundMovable, NULL);
	pthread_cond_init(&CarNorthboundMovable, NULL);
	pthread_cond_init(&CarSouthboundMovable, NULL);


	numWaitingCarsNorthbound   = 0;
	numWaitingCarsSouthbound   = 0;
	numWaitingTrucksNorthbound = 0;
	numWaitingTrucksSouthbound = 0;
	numMovingCars   = 0;
	numMovingTrucks = 0;
	previousmovingdir = SOUTHBOUND;	//These two initial directions are important. previousmovingdir MUST NOT BE SAME VALUE AS currentmovingdir.
	currentmovingdir  = NORTHBOUND;	//Switching them causes the 1st ~3 trucks to have unexpected direction (not flip-flopping).
									//Implicitly, the variables mean previousVehicle'sMovingDirection and currentVehicle'sMovingDirection.
	firstVehicleHasCrossed = 0;
	option = -1;
	pw = NULL; //pointer to waiting list
	pm = NULL; //pointer to moving list

	fprintf(stderr,"***********************************************************************************************\n");
	fprintf(stderr,"Please select one Schedule from the following six options:\n");
	fprintf(stderr,"FORMAT: # vehicles that arrive simultaneously : delay in units of seconds : repeat in same order\n");
	fprintf(stderr,"1. 10 vehicles: DELAY(10 sec) : 10 vehicles\n");
	fprintf(stderr,"   car/truck probability: [1.0, 0.0]\n");
	fprintf(stderr,"2. 10 vehicles : DELAY(10 sec) : 10 vehicles\n");
	fprintf(stderr,"   car/truck probability: [0.0, 1.0]\n");
	fprintf(stderr,"3. 20 vehicles arrive simultaneously\n");
	fprintf(stderr,"   car/truck probability: [0.65, 0.35]\n");
	fprintf(stderr,"4. 10 vehicles : DELAY(25 sec) : 10 vehicles : DELAY(25 sec) : 10 vehicles\n");
	fprintf(stderr,"   car/truck probability: [0.5, 0.5]\n");
	fprintf(stderr,"5. 10 vehicles : DELAY(3 sec) : 10 vehicles : DELAY(10 sec): 10 vehicles\n");
	fprintf(stderr,"   car/truck probability: [0.65, 0.35]\n");
	fprintf(stderr,"6. 20 vehicles : DELAY(15 sec) : 10 vehicles\n");
	fprintf(stderr,"   car/truck probability: [0.75, 0.25]\n");
	if(!useExecutableArgumentForOption)
	{
		do
		{
			fprintf(stderr,"\nPlease select [1-6]:");
			scanf("%d", &option);
		}while((option<0) || (option>6));
	}
	else{option = atoi(strAryOfExecArgs[1]);}


	fprintf(stderr,"***********************************************************************************************\n");

	switch(option)
	{
		case 1: carprob = 1;    break;
		case 2: carprob = 0;    break;
		case 3: carprob = 0.65; break;
		case 4: carprob = 0.5;  break;
		case 5: carprob = 0.65; break;
		case 6: carprob = 0.75; break;
		default: carprob = 0.5;
	}

	srand( (unsigned int)( time( (time_t*)(NULL) ) ) );	//Sets the seed for the random # generator, using (current computer time?) (0?) as a seed

	if(option==1) // 20 vehicles
	{
		//Declare array of vehicle objects/structs by using global variable named "pmstr_t" with type "struct pmstr"
		//I'm not sure what would happen if I used "struct pmstr" instead of "pmstr_t".
		//pmstr_t vehicles[20];

		createVehicleAndThreadAndArrive(carprob, 10, 0);	//cVATAA(..., numVehiclesToCreate,startingArrayIndexOfThreadID)
		sleep(10);											//delay(10 seconds)
		createVehicleAndThreadAndArrive(carprob, 10, 10);	//cVATAA(..., numVehiclesToCreate,startingArrayIndexOfThreadID)
		for(j=0; j<20; j++)
			{pthread_join(vehicleTIDs[j], NULL);}	//THIS NEEDED TO BE COMMENTED OUT UNTIL I IMPLEMENTED/CREATED THE THREADS. THIS LINE CAUSED SEGMENTATION FAULTS.
		/*Though pthread_join() above uses an array of threads, it is actually waiting for threads with the same
		  ID because each array element is a pthread_t, which somehow gets resolved to an int*/

		//free(vehicles);	//This is done implicitly since the compiler knows the size of vehicles[], and hence how many bytes to free. Uncommenting this
		//  line will result in error "double free or corruption (out)".
	} //END OPTION 1

	else if(option==2) // 20 vehicles	//similar to option 1, as are the rest of the options
	{
		createVehicleAndThreadAndArrive(carprob, 10, 0);	//cVATAA(..., numVehiclesToCreate,startingArrayIndexOfThreadID)
		sleep(10);											//delay(10 seconds)
		createVehicleAndThreadAndArrive(carprob, 10, 10);	//cVATAA(..., numVehiclesToCreate,startingArrayIndexOfThreadID)
		for(j=0; j<20; j++)
			{pthread_join(vehicleTIDs[j], NULL);}	//THIS NEEDED TO BE COMMENTED OUT UNTIL I IMPLEMENTED/CREATED THE THREADS. THIS LINE CAUSED SEGMENTATION FAULTS.
		/*Though pthread_join() above uses an array of threads, it is actually waiting for threads with the same
		  ID because each array element is a pthread_t, which somehow gets resolved to an int*/
	} //END OPTION 2


	else if(option==3) // 20 vehicles
	{
		createVehicleAndThreadAndArrive(carprob, 20, 0);	//cVATAA(..., numVehiclesToCreate,startingArrayIndexOfThreadID)
		for(j=0; j<20; j++)
			{pthread_join(vehicleTIDs[j], NULL);}	//THIS NEEDED TO BE COMMENTED OUT UNTIL I IMPLEMENTED/CREATED THE THREADS. THIS LINE CAUSED SEGMENTATION FAULTS.
		/*Though pthread_join() above uses an array of threads, it is actually waiting for threads with the same
		  ID because each array element is a pthread_t, which somehow gets resolved to an int*/
	} //END OPTION 3

	else if(option==4) // 30 vehicles
	{
		createVehicleAndThreadAndArrive(carprob, 10, 0);	//cVATAA(..., numVehiclesToCreate,startingArrayIndexOfThreadID)
		sleep(25);											//delay(25 seconds)
		createVehicleAndThreadAndArrive(carprob, 10, 10);	//cVATAA(..., numVehiclesToCreate,startingArrayIndexOfThreadID)
		sleep(25);											//delay(25 seconds)
		createVehicleAndThreadAndArrive(carprob, 10, 20);	//cVATAA(..., numVehiclesToCreate,startingArrayIndexOfThreadID)

		for(j=0; j<30; j++)
			{pthread_join(vehicleTIDs[j], NULL);}	//THIS NEEDED TO BE COMMENTED OUT UNTIL I IMPLEMENTED/CREATED THE THREADS. THIS LINE CAUSED SEGMENTATION FAULTS.
		/*Though pthread_join() above uses an array of threads, it is actually waiting for threads with the same
		  ID because each array element is a pthread_t, which somehow gets resolved to an int*/
	} //END OPTION 4

	else if(option==5) // 30 vehicles
	{
		createVehicleAndThreadAndArrive(carprob, 10, 0);	//cVATAA(..., numVehiclesToCreate,startingArrayIndexOfThreadID)
	    sleep(3);											//delay(3 seconds)
		createVehicleAndThreadAndArrive(carprob, 10, 10);	//cVATAA(..., numVehiclesToCreate,startingArrayIndexOfThreadID)
		sleep(10);											//delay(10 seconds)
		createVehicleAndThreadAndArrive(carprob, 10, 20);	//cVATAA(..., numVehiclesToCreate,startingArrayIndexOfThreadID)

		for(j=0; j<30; j++)
			{pthread_join(vehicleTIDs[j], NULL);}	//THIS NEEDED TO BE COMMENTED OUT UNTIL I IMPLEMENTED/CREATED THE THREADS. THIS LINE CAUSED SEGMENTATION FAULTS.
		/*Though pthread_join() above uses an array of threads, it is actually waiting for threads with the same
		  ID because each array element is a pthread_t, which somehow gets resolved to an int*/

	} //END OPTION 5

	else //option6: 30 vehicles
	{
		createVehicleAndThreadAndArrive(carprob, 20, 0);	//cVATAA(..., numVehiclesToCreate,startingArrayIndexOfThreadID)
		sleep(15);											//delay(15 seconds)
		createVehicleAndThreadAndArrive(carprob, 10, 10);	//cVATAA(..., numVehiclesToCreate,startingArrayIndexOfThreadID)

		for(j=0; j<30; j++)
			{pthread_join(vehicleTIDs[j], NULL);}	//THIS NEEDED TO BE COMMENTED OUT UNTIL I IMPLEMENTED/CREATED THE THREADS. THIS LINE CAUSED SEGMENTATION FAULTS.
		/*Though pthread_join() above uses an array of threads, it is actually waiting for threads with the same
		  ID because each array element is a pthread_t, which somehow gets resolved to an int*/
	} //END OPTION 6



	//Apparently all these free()s caused THREE frees(), meaning the "double free or corruption (out)" error code
	//  wasn't called until these AND the starterCode thread_join(TIDlist[j],NULL)s were all commented out.
		//free(vehicleList);		//This is commented out because I am no longer using a variable-length array. I am now using a fixed-length array.
	//free(movingvehiclelist);		//Can't free an object. Can only free a variable-sized pointer
	//free(waitingvehiclelist);		//Can't free an object. Can only free a variable-sized pointer
	/*free(pm);
	free(pw);*/
	fprintf(stderr,"\nFinished execution.\n");



} // end of main function






/*		PSEUDOCODE

//Southbound = vehicle is WAITING on the NORTH side of the bridge, trying to cross to get to the South side of the bridge
//isWaiting = vehicle has arrived at one of the two bridge entrances (North,South), waiting to cross the bridge
//1-lane bridge, so vehicles must match North or South directions to travel across an already-occupied bridge



//DO NOT CONFUSE CAR FOR TRUCK. THEY ARE COMPLETELY DIFFERENT TYPES.
vehicle_routine():
if(waitinglist isNotEmpty)
{
	int directionOfMostRecentlyCrossedVehicle;
	int directionOfNextVehicle;

	//PRIORITY 1
	//Direction doesn't matter because only 1 truck max can be crossing the bridge at any given time, BUT IT DOES MATTER BECAUSE
	//  DIRECTION OF EACH TRUCK MUST SWITCH EVERY TIME A TRUCK CROSSES THE BRIDGE
	if(atLeastOneXthboundTruckIsWaiting)	//Don't mandate that currVehicle==TRUCK to enter this block. Mandating that would let cars go before trucks, which isn't allowed.
	{
		//I'm arbitrarily choosing that Southbound trucks get priority over Northbound trucks by putting the if() and else if() (immediately below) first.
		//if(noCarsCrossingBridge && noTrucksCrossingBridge){letUpToOneTruckOnBridge; break;}
		if(		movingList == NULL && currTruckIsSouthbound   &&   atLeastOneNorthboundTruckIsWaiting)// && (usefulForCarsButNotTrucks)directionOfMostRecentlyCrossedVehicleBeforeCurrVehicle==NORTHBOUND) 
		{
			//letUpToOneTruckOnBridge; break;
			addCurrVehicleToMovinglist(currVehicle_Data);		//addThisSouthboundTruckToMovinglist
			removeCurrVehicleFromWaitinglist(currVehicle_Data);	//removeThisSouthboundTruckFromWaitinglist
			sleep(2 seconds);									//Time spent crossing bridge
			removeCurrVehicleFromMovinglist(currVehicle_Data);	//removeThisSouthboundTruckFromMovinglist sinceCurrVehicleFinishedCrossingBridge

			//Call next truck
			directionOfMostRecentlyCrossedVehicle = SOUTHBOUND;	//Setup for next vehicle. prevCrossedVehicle_Dir = dirOfCurrVehicle(NotTheOneAboutToBeSignaled).	Called previousmovingdir in code.
			directionOfNextVehicle = NORTHBOUND;				//Setup for next vehicle. nextVehicle_Dir = dirOfVehicleAboutToBeSignaled.							Called currentmovingdir in code.
			pthread_signal(&NorthboundTruckCanMove);	//CurrTruckIsSouthbound, so next truck will be going opposite direction to have fair/just truck traffic, which is North.
		}
		else if(movingList == NULL && currTruckIsSouthbound   &&   ZEROnorthboundTrucksAreWaiting && atLeastOneSouthboundTruckIsWaiting)
		{
			//letUpToOneTruckOnBridge; break;
			addCurrVehicleToMovinglist(currVehicle_Data);		//addThisSouthboundTruckToMovinglist
			removeCurrVehicleFromWaitinglist(currVehicle_Data);	//removeThisSouthboundTruckFromWaitinglist
			sleep(2 seconds);									//Time spent crossing bridge
			removeCurrVehicleFromMovinglist(currVehicle_Data);	//removeThisSouthboundTruckFromMovinglist sinceCurrVehicleFinishedCrossingBridge

			//Call next truck
			directionOfMostRecentlyCrossedVehicle = SOUTHBOUND;	//Setup for next vehicle. prevCrossedVehicle_Dir = dirOfCurrVehicle(NotTheOneAboutToBeSignaled).	Called previousmovingdir in code.
			directionOfNextVehicle = SOUTHBOUND;				//Setup for next vehicle. nextVehicle_Dir = dirOfVehicleAboutToBeSignaled.							Called currentmovingdir in code.
			pthread_signal(&SouthboundTruckCanMove);	//CurrTruckIsSouthbound, so next truck will be going opposite direction to have fair/just truck traffic, which is North
														//, but there is no opposing truck traffic, so instead deplete the list of trucks that are still waiting (waitingSouthboundTrucks).
		}

		else if(movingList == NULL && currTruckIsNorthbound   &&   atLeastOneSouthboundTruckIsWaiting)// && (usefulForCarsButNotTrucks)directionOfMostRecentlyCrossedVehicleBeforeCurrVehicle==NORTHBOUND)
		{
			//letUpToOneTruckOnBridge; break;
			addCurrVehicleToMovinglist(currVehicle_Data);		//addThisNorthboundTruckToMovinglist
			removeCurrVehicleFromWaitinglist(currVehicle_Data);	//removeThisNorthboundTruckFromWaitinglist
			sleep(2 seconds);									//Time spent crossing bridge
			removeCurrVehicleFromMovinglist(currVehicle_Data);	//removeThisNorthboundTruckFromMovinglist sinceCurrVehicleFinishedCrossingBridge

			//Call next truck
			directionOfMostRecentlyCrossedVehicle = NORTHBOUND;	//Setup for next vehicle. prevCrossedVehicle_Dir = dirOfCurrVehicle(NotTheOneAboutToBeSignaled).	Called previousmovingdir in code.
			directionOfNextVehicle = SOUTHBOUND;				//Setup for next vehicle. nextVehicle_Dir = dirOfVehicleAboutToBeSignaled.							Called currentmovingdir in code.
			pthread_signal(&SouthboundTruckCanMove);	//CurrTruckIsNorthbound, so next truck will be going opposite direction to have fair/just truck traffic, which is South.
		}
		else if(movingList == NULL && currTruckIsNorthbound   &&   ZEROsouthboundTrucksAreWaiting && atLeastOneNorthboundTruckIsWaiting)
		{
			//letUpToOneTruckOnBridge; break;
			addCurrVehicleToMovinglist(currVehicle_Data);		//addThisNorthboundTruckToMovinglist
			removeCurrVehicleFromWaitinglist(currVehicle_Data);	//removeThisNorthboundTruckFromWaitinglist
			sleep(2 seconds);									//Time spent crossing bridge
			removeCurrVehicleFromMovinglist(currVehicle_Data);	//removeThisNorthboundTruckFromMovinglist sinceCurrVehicleFinishedCrossingBridge

			//Call next truck
			directionOfMostRecentlyCrossedVehicle = NORTHBOUND;	//Setup for next vehicle. prevCrossedVehicle_Dir = dirOfCurrVehicle(NotTheOneAboutToBeSignaled).	Called previousmovingdir in code.
			directionOfNextVehicle = NORTHBOUND;				//Setup for next vehicle. nextVehicle_Dir = dirOfVehicleAboutToBeSignaled.							Called currentmovingdir in code.
			pthread_signal(&NorthboundTruckCanMove);	//CurrTruckIsNorthbound, so next truck will be going opposite direction to have fair/just truck traffic, which is South
														//, but there is no opposing truck traffic, so instead deplete the list of trucks that are still waiting (waitingNorthboundTrucks).
		}
	}

	//else AND NOT if() BECAUSE TRUCKS MUST GO ____BEFORE____ CARS
	else if(atLeastOneXthboundCarIsWaiting && numMovingCars<3)//&& (implicit)currVehicleType==CAR)
	{
		//Can now assume that there are no waiting trucks at all thanks to the above block
		addCurrVehicleToMovinglist(currVehicle_Data);		//addThisXthboundCarToMovinglist
		removeCurrVehicleFromWaitinglist(currVehicle_Data);	//removeThisXthboundCarFromWaitinglist
		sleep(2 seconds);									//Time spent crossing bridge
		removeCurrVehicleFromMovinglist(currVehicle_Data);	//removeThisXthboundCarFromMovinglist sinceCurrVehicleFinishedCrossingBridge
		
		//BRIDGE SHOULD ALWAYS BE AT MAX CAPACITY WHEN THERE ARE VEHICLES WAITING
		//I'm arbitrarily choosing that Southbound cars get priority over Northbound cars by putting this if() first.
		if(currCarIsSouthbound && noCarsOnBridge   &&   atLeastThreeSouthboundCarsAreWaiting)// && (implicitly)zeroTrucksAreWaiting && ZEROnorthboundCarsAlreadyOnBridge)
		{
			//letUpToThreeSouthboundCarsOnBridge; break;
			//Call next cars
			directionOfMostRecentlyCrossedVehicle = SOUTHBOUND;	//Setup for next vehicle. prevCrossedVehicle_Dir = dirOfCurrVehicle(NotTheOneAboutToBeSignaled).	Called previousmovingdir in code.
			directionOfNextVehicle = SOUTHBOUND;				//Setup for next vehicle. nextVehicle_Dir = dirOfVehicleAboutToBeSignaled.							Called currentmovingdir in code.
			for(i=#SbCarsAlreadyOnBridge; i<3; i++)		//Executes 3 times, signaling 3 southboundCars.
				{pthread_signal(&SouthboundCarCanMove);}		//Deplete the list of southbound cars that are still waiting (waitingSouthboundCars).
		}
		else if(currCarIsSouthbound && oneSouthboundCarAlreadyOnBridge   &&   atLeastTwoSouthboundCarsAreWaiting)// && (implicitly)zeroTrucksAreWaiting && ZEROnorthboundCarsAlreadyOnBridge)
		{
			//letUpToTwoSouthboundCarsOnBridge; break;
			//Call next cars
			directionOfMostRecentlyCrossedVehicle = SOUTHBOUND;	//Setup for next vehicle. prevCrossedVehicle_Dir = dirOfCurrVehicle(NotTheOneAboutToBeSignaled).	Called previousmovingdir in code.
			directionOfNextVehicle = SOUTHBOUND;				//Setup for next vehicle. nextVehicle_Dir = dirOfVehicleAboutToBeSignaled.							Called currentmovingdir in code.
			for(i=#SbCarsAlreadyOnBridge; i<3; i++)		//Executes 2 times, signaling 2 southboundCars.
				{pthread_signal(&SouthboundCarCanMove);}		//Deplete the list of southbound cars that are still waiting (waitingSouthboundCars).
		}
		else if(currCarIsSouthbound && twoSouthboundCarsAlreadyOnBridge   &&   atLeastOneSouthboundCarIsWaiting)// && (implicitly)zeroTrucksAreWaiting && ZEROnorthboundCarsAlreadyOnBridge)
		{
			//letUpToOneSouthboundCarsOnBridge; break;
			//Call next cars
			directionOfMostRecentlyCrossedVehicle = SOUTHBOUND;	//Setup for next vehicle. prevCrossedVehicle_Dir = dirOfCurrVehicle(NotTheOneAboutToBeSignaled).	Called previousmovingdir in code.
			directionOfNextVehicle = SOUTHBOUND;				//Setup for next vehicle. nextVehicle_Dir = dirOfVehicleAboutToBeSignaled.							Called currentmovingdir in code.
			for(i=#SbCarsAlreadyOnBridge; i<3; i++)		//Executes 1 time, signaling 1 southboundCar.
				{pthread_signal(&SouthboundCarCanMove);}		//Deplete the list of southbound cars that are still waiting (waitingSouthboundCars).
		}

		else if(currCarIsNorthbound && noCarsOnBridge   &&   atLeastThreeNorthboundCarsAreWaiting)// && (implicitly)zeroTrucksAreWaiting && ZEROsouthboundCarsAlreadyOnBridge)
		{
			//letUpToThreeNorthboundCarsOnBridge; break;
			//Call next cars
			directionOfMostRecentlyCrossedVehicle = NORTHBOUND;	//Setup for next vehicle. prevCrossedVehicle_Dir = dirOfCurrVehicle(NotTheOneAboutToBeSignaled).	Called previousmovingdir in code.
			directionOfNextVehicle = NORTHBOUND;				//Setup for next vehicle. nextVehicle_Dir = dirOfVehicleAboutToBeSignaled.							Called currentmovingdir in code.
			for(i=#NbCarsAlreadyOnBridge; i<3; i++)		//Executes 3 times, signaling 3 northboundCars.
				{pthread_signal(&NorthboundCarCanMove);}		//Deplete the list of northbound cars that are still waiting (waitingNorthboundCars).
		}
		else if(currCarIsNorthbound && oneNorthboundCarAlreadyOnBridge   &&   atLeastTwoNorthboundCarsAreWaiting)// && (implicitly)zeroTrucksAreWaiting && ZEROsouthboundCarsAlreadyOnBridge)
		{
			//letUpToTwoNorthboundCarsOnBridge; break;
			//Call next cars
			directionOfMostRecentlyCrossedVehicle = NORTHBOUND;	//Setup for next vehicle. prevCrossedVehicle_Dir = dirOfCurrVehicle(NotTheOneAboutToBeSignaled).	Called previousmovingdir in code.
			directionOfNextVehicle = NORTHBOUND;				//Setup for next vehicle. nextVehicle_Dir = dirOfVehicleAboutToBeSignaled.							Called currentmovingdir in code.
			for(i=#NbCarsAlreadyOnBridge; i<3; i++)		//Executes 3 times, signaling 3 northboundCars.
				{pthread_signal(&NorthboundCarCanMove);}		//Deplete the list of northbound cars that are still waiting (waitingNorthboundCars).
		}
		else if(currCarIsNorthbound && twoNorthboundCarsAlreadyOnBridge   &&   atLeastOneNorthboundCarIsWaiting)// && (implicitly)zeroTrucksAreWaiting && ZEROsouthboundCarsAlreadyOnBridge)
		{
			//letUpToOneNorthboundCarOnBridge; break;
			//Call next cars
			directionOfMostRecentlyCrossedVehicle = NORTHBOUND;	//Setup for next vehicle. prevCrossedVehicle_Dir = dirOfCurrVehicle(NotTheOneAboutToBeSignaled).	Called previousmovingdir in code.
			directionOfNextVehicle = NORTHBOUND;				//Setup for next vehicle. nextVehicle_Dir = dirOfVehicleAboutToBeSignaled.							Called currentmovingdir in code.
			for(i=#NbCarsAlreadyOnBridge; i<3; i++)		//Executes 3 times, signaling 3 northboundCars.
				{pthread_signal(&NorthboundCarCanMove);}		//Deplete the list of northbound cars that are still waiting (waitingNorthboundCars).
		}


		//THE else if()s BELOW SHOULD ONLY BE REACHED ONCE ONE LIST OF CARS EMPTIES ENTIRELY. All that changes in the two blocks below is the ????????
		//THE PSEUDOCODE BELOW IS WRONG> FIXME
		else if(currCarIsSouthbound && noCarsOnBridge   &&   atLeastThreeSouthboundCarsAreWaiting)// && (implicitly)zeroTrucksAreWaiting && ZEROnorthboundCarsAlreadyOnBridge)
		{
			//letUpToThreeSouthboundCarsOnBridge; break;
			//Call next cars
			directionOfMostRecentlyCrossedVehicle = SOUTHBOUND;	//Setup for next vehicle. prevCrossedVehicle_Dir = dirOfCurrVehicle(NotTheOneAboutToBeSignaled).	Called previousmovingdir in code.
			directionOfNextVehicle = SOUTHBOUND;				//Setup for next vehicle. nextVehicle_Dir = dirOfVehicleAboutToBeSignaled.							Called currentmovingdir in code.
			for(i=#SbCarsAlreadyOnBridge; i<3; i++)		//Executes 3 times, signaling 3 southboundCars.
				{pthread_signal(&SouthboundCarCanMove);}		//Deplete the list of southbound cars that are still waiting (waitingSouthboundCars).
		}
		else if(currCarIsSouthbound && oneSouthboundCarAlreadyOnBridge   &&   atLeastTwoSouthboundCarsAreWaiting)// && (implicitly)zeroTrucksAreWaiting && ZEROnorthboundCarsAlreadyOnBridge)
		{
			//letUpToTwoSouthboundCarsOnBridge; break;
			//Call next cars
			directionOfMostRecentlyCrossedVehicle = SOUTHBOUND;	//Setup for next vehicle. prevCrossedVehicle_Dir = dirOfCurrVehicle(NotTheOneAboutToBeSignaled).	Called previousmovingdir in code.
			directionOfNextVehicle = SOUTHBOUND;				//Setup for next vehicle. nextVehicle_Dir = dirOfVehicleAboutToBeSignaled.							Called currentmovingdir in code.
			for(i=#SbCarsAlreadyOnBridge; i<3; i++)		//Executes 2 times, signaling 2 southboundCars.
				{pthread_signal(&SouthboundCarCanMove);}		//Deplete the list of southbound cars that are still waiting (waitingSouthboundCars).
		}
		else if(currCarIsSouthbound && twoSouthboundCarsAlreadyOnBridge   &&   atLeastOneSouthboundCarIsWaiting)// && (implicitly)zeroTrucksAreWaiting && ZEROnorthboundCarsAlreadyOnBridge)
		{
			//letUpToOneSouthboundCarsOnBridge; break;
			//Call next cars
			directionOfMostRecentlyCrossedVehicle = SOUTHBOUND;	//Setup for next vehicle. prevCrossedVehicle_Dir = dirOfCurrVehicle(NotTheOneAboutToBeSignaled).	Called previousmovingdir in code.
			directionOfNextVehicle = SOUTHBOUND;				//Setup for next vehicle. nextVehicle_Dir = dirOfVehicleAboutToBeSignaled.							Called currentmovingdir in code.
			for(i=#SbCarsAlreadyOnBridge; i<3; i++)		//Executes 1 time, signaling 1 southboundCar.
				{pthread_signal(&SouthboundCarCanMove);}		//Deplete the list of southbound cars that are still waiting (waitingSouthboundCars).
		}

		else if(currCarIsNorthbound && noCarsOnBridge   &&   atLeastThreeNorthboundCarsAreWaiting)// && (implicitly)zeroTrucksAreWaiting && ZEROsouthboundCarsAlreadyOnBridge)
		{
			//letUpToThreeNorthboundCarsOnBridge; break;
			//Call next cars
			directionOfMostRecentlyCrossedVehicle = NORTHBOUND;	//Setup for next vehicle. prevCrossedVehicle_Dir = dirOfCurrVehicle(NotTheOneAboutToBeSignaled).	Called previousmovingdir in code.
			directionOfNextVehicle = NORTHBOUND;				//Setup for next vehicle. nextVehicle_Dir = dirOfVehicleAboutToBeSignaled.							Called currentmovingdir in code.
			for(i=#NbCarsAlreadyOnBridge; i<3; i++)		//Executes 3 times, signaling 3 northboundCars.
				{pthread_signal(&NorthboundCarCanMove);}		//Deplete the list of northbound cars that are still waiting (waitingNorthboundCars).
		}
		else if(currCarIsNorthbound && oneNorthboundCarAlreadyOnBridge   &&   atLeastTwoNorthboundCarsAreWaiting)// && (implicitly)zeroTrucksAreWaiting && ZEROsouthboundCarsAlreadyOnBridge)
		{
			//letUpToTwoNorthboundCarsOnBridge; break;
			//Call next cars
			directionOfMostRecentlyCrossedVehicle = NORTHBOUND;	//Setup for next vehicle. prevCrossedVehicle_Dir = dirOfCurrVehicle(NotTheOneAboutToBeSignaled).	Called previousmovingdir in code.
			directionOfNextVehicle = NORTHBOUND;				//Setup for next vehicle. nextVehicle_Dir = dirOfVehicleAboutToBeSignaled.							Called currentmovingdir in code.
			for(i=#NbCarsAlreadyOnBridge; i<3; i++)		//Executes 3 times, signaling 3 northboundCars.
				{pthread_signal(&NorthboundCarCanMove);}		//Deplete the list of northbound cars that are still waiting (waitingNorthboundCars).
		}
		else if(currCarIsNorthbound && twoNorthboundCarsAlreadyOnBridge   &&   atLeastOneNorthboundCarIsWaiting)// && (implicitly)zeroTrucksAreWaiting && ZEROsouthboundCarsAlreadyOnBridge)
		{
			//letUpToOneNorthboundCarOnBridge; break;
			//Call next cars
			directionOfMostRecentlyCrossedVehicle = NORTHBOUND;	//Setup for next vehicle. prevCrossedVehicle_Dir = dirOfCurrVehicle(NotTheOneAboutToBeSignaled).	Called previousmovingdir in code.
			directionOfNextVehicle = NORTHBOUND;				//Setup for next vehicle. nextVehicle_Dir = dirOfVehicleAboutToBeSignaled.							Called currentmovingdir in code.
			for(i=#NbCarsAlreadyOnBridge; i<3; i++)		//Executes 3 times, signaling 3 northboundCars.
				{pthread_signal(&NorthboundCarCanMove);}		//Deplete the list of northbound cars that are still waiting (waitingNorthboundCars).
		}
		//if(threeCarsOnBridge){ waitForAtLeastOneCarToFinishCrossing; (This means to exit the if() and not signal any other vehicles)}

		//My code later: twoSouthboundCarsAlreadyOnBridge   ==   (numMovingCars==2 && previousmovingdir==SOUTHBOUND)
	}

	if(vehiclesSimultaneouslyOccupyingBridge areTravelingDifferentDirections)
		{								throw error and exit entire program;}	//Collision due to 1-lane bridge
	if(fewerThan zeroCarsOnBridge){		throw error and exit entire program;}	//-# of vehicles is impossible
	if(greaterThan threeCarsOnBridge){	throw error and exit entire program;}	//Bridge breaks
	if(greaterThan oneTruckOnBridge){	throw error and exit entire program;}	//Bridge breaks
}
*/


void* vehicle_routine(/* pmstr_t* */ void* pmstrpara)
{
	char* strdir;		//What the heck is this for????? structDirection....??
	struct pmstr* currVehicle = (struct pmstr*)(pmstrpara);
	int currVehicleID = currVehicle->vehicle_id;
	int currVehicleType = currVehicle->vehicle_type;


	//CAR   ------------------------------------------------------------------------------------
	if(currVehicleType == TYPE_CAR)
	{
		pthread_mutex_lock(&lock);

		//Try to cross
		//while(this vehicle cannot cross)
		//     {wait for proper moving signal}
		//while()//5 conditions because #moving, #waiting, directionOfCurrentMovingVehicle
		//TRUCKS MUST GO ____BEFORE____ CARS
		while(numMovingCars>2 || numMovingTrucks>0 || (numWaitingCarsNorthbound<1 && numWaitingCarsSouthbound<1)
			&& !((currVehicle->direction!=previousmovingdir && (numWaitingCarsNorthbound==0 || numWaitingCarsSouthbound==0))//differentDirectionsIsOnlyLegalIfOneWaitingListIsEmpty
			||( currVehicle->direction==previousmovingdir)))//noConflictingDirectionsBetweenVehiclesOnBridge
		{
			if(currVehicle->direction == NORTHBOUND){	pthread_cond_wait(&CarNorthboundMovable, &lock); if(DEBUG)fprintf(stderr,"_car#%d(N)_\n",currVehicleID);}
			else{										pthread_cond_wait(&CarSouthboundMovable, &lock); if(DEBUG)fprintf(stderr,"_car#%d(S)_\n",currVehicleID);}
		}


			//Now begin crossing
			//Can now assume that there are no waiting trucks at all thanks to the above block
			//update global variables
			if(DEBUG)
			{
				fprintf(stderr,"\n\n\n\nVehicleRoutine: BEFORE CAR#%d(%s) CROSSES:\n", currVehicleID, (currVehicle->direction==NORTHBOUND ? "N":"S"));
				printwaiting(true);
				printmoving(true);
			}

			movinglistinsert(currVehicleID,currVehicleType,currVehicle->direction);		//addThisXthboundCarToMovinglist
			waitinglistdelete(currVehicleID);											//removeThisXthboundCarFromWaitinglist
			numMovingCars++;					//if(DEBUG)fprintf(stderr,"VehicleRoutine: #numMovingCars++\n");
			if(		currVehicle->direction == NORTHBOUND)
				{numWaitingCarsNorthbound--;	}//if(DEBUG)fprintf(stderr,"VehicleRoutine: #WaitingCarsNorthbound--\n");}
			else if(currVehicle->direction == SOUTHBOUND)
				{numWaitingCarsSouthbound--;	}//if(DEBUG)fprintf(stderr,"VehicleRoutine: #WaitingCarsSouthbound--\n");}
			//previousmovingdir = currVehicle->direction;	//Updates global var previousmovingdir for usage later during deciding who to signal.
				//MUST BE UPDATED ONLY __AFTER__ USED
			
			
				fprintf(stderr,"\n\nVehicleRoutine: DURING CAR#%d(%s) CROSSING:\n", currVehicleID, (currVehicle->direction==NORTHBOUND ? "N":"S"));
				printwaiting(true);
				printmoving(true);
			

			pthread_mutex_unlock(&lock);

			sleep(2);																	//Time spent crossing bridge
			
			pthread_mutex_lock(&lock);

			//update global variables
			movinglistdelete(currVehicleID);											//removeThisXthboundCarFromMovinglist sinceCurrVehicleFinishedCrossingBridge
			numMovingCars--; //if(DEBUG)fprintf(stderr,"\nVehicleRoutine: numMovingCars--\n");

			//print out proper message
			fprintf(stderr,"Car #%d(%s) exited the bridge.\n", currVehicleID, (currVehicle->direction==NORTHBOUND ? "Nb":"Sb"));
			if(DEBUG)
			{
				fprintf(stderr,"\n\nVehicleRoutine: AFTER CAR#%d(%s) CROSSED:\n", currVehicleID, (currVehicle->direction==NORTHBOUND ? "N":"S"));
				printwaiting(true);
				printmoving(true);
			}
			
			//send out signals to wake up vehicle(s) accordingly
			//BRIDGE SHOULD ALWAYS BE AT MAX CAPACITY WHEN THERE ARE VEHICLES WAITING
			//I'm arbitrarily choosing that Southbound vehicles get priority over Northbound vehicles by putting their if()s first.
			if(numWaitingTrucksSouthbound>0)
			{
				previousmovingdir = currVehicle->direction;	//Setup for next vehicle. prevCrossedVehicle_Dir = dirOfCurrVehicle(NotTheOneAboutToBeSignaled).
				currentmovingdir  = SOUTHBOUND;				//Setup for next vehicle. nextVehicleToCross_Dir = dirOfVehicleAboutToBeSignaled.
				pthread_cond_signal(&TruckSouthboundMovable);
				if(DEBUG)fprintf(stderr,"VehicleRoutine: Car#%d(%s) sent signal(TruckSouthboundMovable)  (#waitingTrucksSouthbound>0)\n",currVehicleID, (currVehicle->direction==NORTHBOUND ? "N":"S"));
			}
			else if(numWaitingTrucksNorthbound>0)
			{
				previousmovingdir = currVehicle->direction;	//Setup for next vehicle. prevCrossedVehicle_Dir = dirOfCurrVehicle(NotTheOneAboutToBeSignaled).
				currentmovingdir  = NORTHBOUND;				//Setup for next vehicle. nextVehicleToCross_Dir = dirOfVehicleAboutToBeSignaled.
				pthread_cond_signal(&TruckNorthboundMovable);
				if(DEBUG)fprintf(stderr,"VehicleRoutine: Car#%d(%s) sent signal(TruckNorthboundMovable)  (#waitingTrucksNorthbound>0)\n",currVehicleID, (currVehicle->direction==NORTHBOUND ? "N":"S"));
			}
			
			//Direction of traffic can only flip once bridge is empty
			else if(numMovingCars==0)// && (implicitly)zeroTrucksAreWaiting && ZEROcarsAlreadyOnBridge)
			{
				//letUpToThreeSouthboundCarsOnBridge; break;
				//Call next cars

				//Move cars onto bridge from a non-empty list
				if(		currVehicle->direction==SOUTHBOUND  &&  numWaitingCarsSouthbound>0)
				{
					previousmovingdir = currVehicle->direction;	//Setup for next vehicle. prevCrossedVehicle_Dir = dirOfCurrVehicle(NotTheOneAboutToBeSignaled).
					currentmovingdir  = SOUTHBOUND;				//Setup for next vehicle. nextVehicleToCross_Dir = dirOfVehicleAboutToBeSignaled.
					for(i=numMovingCars, j=0; (i<3 && j<numWaitingCarsSouthbound)/*#carsNecessary<=#carsLeft*/; i++, j++)		//Executes 3 times, signaling 3 southboundCars.
					{
						pthread_cond_signal(&CarSouthboundMovable);		//Deplete the list of southbound cars that are still waiting (waitingSouthboundCars).
						if(DEBUG)fprintf(stderr,"VehicleRoutine: Car#%d(Sb) sent signal(CarSouthboundMovable)  (currMovDir==S && ZERO_movingCars && #SbCarsWaiting>2)\n",currVehicleID);
					}
				}
				else if(currVehicle->direction==NORTHBOUND  &&  numWaitingCarsNorthbound>0)
				{
					previousmovingdir = currVehicle->direction;	//Setup for next vehicle. prevCrossedVehicle_Dir = dirOfCurrVehicle(NotTheOneAboutToBeSignaled).
					currentmovingdir  = NORTHBOUND;				//Setup for next vehicle. nextVehicleToCross_Dir = dirOfVehicleAboutToBeSignaled.
					for(i=numMovingCars, j=0; (i<3 && j<numWaitingCarsNorthbound)/*#carsNecessary<=#carsLeft*/; i++, j++)		//Executes 3 times, signaling 3 northboundCars.
					{
						pthread_cond_signal(&CarNorthboundMovable);		//Deplete the list of northbound cars that are still waiting (waitingNorthboundCars).
						if(DEBUG)fprintf(stderr,"VehicleRoutine: Car#%d(Nb) sent signal(CarNorthboundMovable)  (currMovDir==N && ZERO_movingCars && #NbCarsWaiting>2)\n",currVehicleID);
					}
				}

				//ONLY REACHED ONCE bridge is empty && the current direction's list of cars is empty
				//Start moving cars from the other direction's (nonempty) list
				else if(currVehicle->direction==SOUTHBOUND  &&  numWaitingCarsSouthbound==0)
				{
					previousmovingdir = currVehicle->direction;	//Setup for next vehicle. prevCrossedVehicle_Dir = dirOfCurrVehicle(NotTheOneAboutToBeSignaled).
					currentmovingdir  = NORTHBOUND;				//Setup for next vehicle. nextVehicleToCross_Dir = dirOfVehicleAboutToBeSignaled.
					for(i=numMovingCars, j=0; (i<3 && j<numWaitingCarsNorthbound)/*#carsNecessary<=#carsLeft*/; i++, j++)		//Executes 3 times, signaling 3 northboundCars.
					{
						pthread_cond_signal(&CarNorthboundMovable);		//Deplete the list of northbound cars that are still waiting (waitingNorthboundCars).
						if(DEBUG)fprintf(stderr,"VehicleRoutine: Car#%d(Sb) sent signal(CarNorthboundMovable)  (currMovDir==S && ZERO_movingCars && ZERO_sbCarsWaiting)\n",currVehicleID);
					}
				}
				else if(currVehicle->direction==NORTHBOUND  &&  numWaitingCarsNorthbound==0)
				{
					previousmovingdir = currVehicle->direction;	//Setup for next vehicle. prevCrossedVehicle_Dir = dirOfCurrVehicle(NotTheOneAboutToBeSignaled).
					currentmovingdir  = SOUTHBOUND;				//Setup for next vehicle. nextVehicleToCross_Dir = dirOfVehicleAboutToBeSignaled.
					for(i=numMovingCars, j=0; (i<3 && j<numWaitingCarsSouthbound)/*#carsNecessary<=#carsLeft*/; i++, j++)		//Executes 3 times, signaling 3 soutthboundCars.
					{
						pthread_cond_signal(&CarSouthboundMovable);		//Deplete the list of southbound cars that are still waiting (waitingSouthboundCars).
						if(DEBUG)fprintf(stderr,"VehicleRoutine: Car#%d(Nb) sent signal(CarSouthboundMovable)  (currMovDir==N && ZERO_movingCars && ZERO_nbCarsWaiting)\n",currVehicleID);
					}
				}
				else
				{if(DEBUG)fprintf(stderr,"\nCar#%d(%s) did NOT signal because ????????????????????????????.  VehicleRoutine: Car: else if(numMovingCars==0)\n\n", currVehicleID, (currVehicle->direction==NORTHBOUND ? "N":"S"));}
			}
			else if(numMovingCars==1)// && (implicitly)zeroTrucksAreWaiting && ZEROnorthboundCarsAlreadyOnBridge)
			{
				//letUpToTwoSouthboundCarsOnBridge; break;
				//Call next cars
				//if(thisIsNb && onlyCarOnBridgeIsNb && atLeast2NbCarsWaiting)
				if(		currVehicle->direction==NORTHBOUND && previousmovingdir==NORTHBOUND   &&   numWaitingCarsNorthbound>1)
				{
					previousmovingdir = currVehicle->direction;	//Setup for next vehicle. prevCrossedVehicle_Dir = dirOfCurrVehicle(NotTheOneAboutToBeSignaled).
					currentmovingdir = NORTHBOUND;				//Setup for next vehicle. nextVehicleToCross_Dir = dirOfVehicleAboutToBeSignaled.
					for(i=numMovingCars; i<3; i++)		//Executes 2 times, signaling 2 southboundCars.
					{
						pthread_cond_signal(&CarNorthboundMovable);		//Deplete the list of northbound cars that are still waiting (waitingNorthboundCars).
						if(DEBUG)fprintf(stderr,"VehicleRoutine: Car#%d(Nb) sent signal(CarNorthboundMovable)  (currMovDir==N && prevMovDir==S && 1SbMovingCars && #SbCarsWaiting>1)\n",currVehicleID);
					}
				}
				//if(thisIsSb && onlyCarOnBridgeIsSb && atLeast2SbCarsWaiting)
				else if(currVehicle->direction==SOUTHBOUND && previousmovingdir==SOUTHBOUND   &&   numWaitingCarsSouthbound>1)
				{
					previousmovingdir = currVehicle->direction;	//Setup for next vehicle. prevCrossedVehicle_Dir = dirOfCurrVehicle(NotTheOneAboutToBeSignaled).
					currentmovingdir = SOUTHBOUND;				//Setup for next vehicle. nextVehicleToCross_Dir = dirOfVehicleAboutToBeSignaled.
					for(i=numMovingCars; i<3; i++)		//Executes 2 times, signaling 2 southboundCars.
					{
						pthread_cond_signal(&CarSouthboundMovable);		//Deplete the list of southbound cars that are still waiting (waitingSouthboundCars).
						if(DEBUG)fprintf(stderr,"VehicleRoutine: Car#%d(Sb) sent signal(CarSouthboundMovable)  (currMovDir==S && prevMovDir==S && 1SbMovingCars && #SbCarsWaiting>1)\n",currVehicleID);
					}
				}

				//if(thisIsNb && onlyCarOnBridgeIsNb && exactly1NbCarWaiting)
				else if(currVehicle->direction==NORTHBOUND && previousmovingdir==NORTHBOUND   &&   numWaitingCarsNorthbound>0)
				{
					previousmovingdir = currVehicle->direction;	//Setup for next vehicle. prevCrossedVehicle_Dir = dirOfCurrVehicle(NotTheOneAboutToBeSignaled).
					currentmovingdir = NORTHBOUND;				//Setup for next vehicle. nextVehicleToCross_Dir = dirOfVehicleAboutToBeSignaled.
					pthread_cond_signal(&CarNorthboundMovable);		//Deplete the list of northbound cars that are still waiting (waitingNorthboundCars).
					if(DEBUG)fprintf(stderr,"VehicleRoutine: Car#%d(Nb) sent signal(CarNorthboundMovable)  (currMovDir==N && prevMovDir==S && 1SbMovingCars && #SbCarsWaiting>1)\n",currVehicleID);
				}
				//if(thisIsSb && onlyCarOnBridgeIsSb && exactly1SbCarWaiting)
				else if(currVehicle->direction==SOUTHBOUND && previousmovingdir==SOUTHBOUND   &&   numWaitingCarsSouthbound>0)
				{
					previousmovingdir = currVehicle->direction;	//Setup for next vehicle. prevCrossedVehicle_Dir = dirOfCurrVehicle(NotTheOneAboutToBeSignaled).
					currentmovingdir = SOUTHBOUND;				//Setup for next vehicle. nextVehicleToCross_Dir = dirOfVehicleAboutToBeSignaled.
					pthread_cond_signal(&CarSouthboundMovable);		//Deplete the list of southbound cars that are still waiting (waitingSouthboundCars).
					if(DEBUG)fprintf(stderr,"VehicleRoutine: Car#%d(Sb) sent signal(CarSouthboundMovable)  (currMovDir==S && prevMovDir==S && 1SbMovingCars && #SbCarsWaiting>1)\n",currVehicleID);
				}
				else
				{if(DEBUG)fprintf(stderr,"\n\nCar#%d(%s) did NOT signal because\n1) Their direction's list of cars is empty and there's still another car left on the bridge (opposite directions can't travel simultaneously)   or\n2) A direction switch happened with the previous vehicle. DEBUG: previousmovingdir==%s, currentmovingdir==%s  VehicleRoutine: Car: else if(numMovingCars==1)\n\n", currVehicleID, (currVehicle->direction==NORTHBOUND ? "N":"S"), (previousmovingdir==NORTHBOUND ? "N":"S"), (currentmovingdir==NORTHBOUND ? "N":"S"));}
			}
			else if(numMovingCars==2)// && (implicitly)zeroTrucksAreWaiting && ZEROnorthboundCarsAlreadyOnBridge)
			{
				//letUpToOneSouthboundCarsOnBridge; break;
				//Call next cars
				//if(thisIsSb && theTwoCarsOnBridgeAreSb && atLeast1SbCarWaiting)
				if(		currVehicle->direction==SOUTHBOUND && previousmovingdir==SOUTHBOUND   &&   numWaitingCarsSouthbound>0)
				{
					previousmovingdir = currVehicle->direction;	//Setup for next vehicle. prevCrossedVehicle_Dir = dirOfCurrVehicle(NotTheOneAboutToBeSignaled).
					currentmovingdir = SOUTHBOUND;				//Setup for next vehicle. nextVehicleToCross_Dir = dirOfVehicleAboutToBeSignaled.
					for(i=numMovingCars; i<3; i++)		//Executes 1 time, signaling 1 southboundCar.
					{
						pthread_cond_signal(&CarSouthboundMovable);		//Deplete the list of southbound cars that are still waiting (waitingSouthboundCars).
						if(DEBUG)fprintf(stderr,"VehicleRoutine: Car#%d(Sb) sent signal(CarSouthboundMovable)  (currMovDir==S && prevMovDir==S && 2SbMovingCars && #SbCarsWaiting>0)\n",currVehicleID);
					}
				}
				//if(thisIsNb && theTwoCarsOnBridgeAreNb && atLeast1NbCarWaiting)
				else if(currVehicle->direction==NORTHBOUND && previousmovingdir==NORTHBOUND   &&   numWaitingCarsNorthbound>0)
				{
					previousmovingdir = currVehicle->direction;	//Setup for next vehicle. prevCrossedVehicle_Dir = dirOfCurrVehicle(NotTheOneAboutToBeSignaled).
					currentmovingdir = NORTHBOUND;				//Setup for next vehicle. nextVehicleToCross_Dir = dirOfVehicleAboutToBeSignaled.
					for(i=numMovingCars; i<3; i++)		//Executes 1 time, signaling 1 northboundCar.
					{
						pthread_cond_signal(&CarNorthboundMovable);		//Deplete the list of northbound cars that are still waiting (waitingNorthboundCars).
						if(DEBUG)fprintf(stderr,"VehicleRoutine: Car#%d(Nb) sent signal(CarNorthboundMovable)  (currMovDir==N && prevMovDir==N && 2NbMovingCars && #NbCarsWaiting>0)\n",currVehicleID);
					}
				}
				
				//if(directionSwitchJustHappened){theseTwoCodeBlocksBelow}
				//if(thisIsSb && directionSwitchDueToAnEmptyWaitingListJustHappened && atLeast1SbCarWaiting)
				else if(currVehicle->direction==SOUTHBOUND && (previousmovingdir==NORTHBOUND && numWaitingCarsNorthbound==0)   &&   numWaitingCarsSouthbound>0)
				{
					previousmovingdir = currVehicle->direction;	//Setup for next vehicle. prevCrossedVehicle_Dir = dirOfCurrVehicle(NotTheOneAboutToBeSignaled).
					currentmovingdir = SOUTHBOUND;				//Setup for next vehicle. nextVehicleToCross_Dir = dirOfVehicleAboutToBeSignaled.
					for(i=numMovingCars; i<3; i++)		//Executes 1 time, signaling 1 southboundCar.
					{
						pthread_cond_signal(&CarSouthboundMovable);		//Deplete the list of southbound cars that are still waiting (waitingSouthboundCars).
						if(DEBUG)fprintf(stderr,"VehicleRoutine: Car#%d(Sb) sent signal(CarSouthboundMovable)  (currMovDir==S && prevMovDir==S && 2SbMovingCars && #SbCarsWaiting>0)\n",currVehicleID);
					}
				}
				//if(thisIsNb && directionSwitchDueToAnEmptyWaitingListJustHappened && atLeast1NbCarWaiting)
				else if(currVehicle->direction==NORTHBOUND && (previousmovingdir==SOUTHBOUND && numWaitingCarsSouthbound==0)   &&   numWaitingCarsNorthbound>0)
				{
					previousmovingdir = currVehicle->direction;	//Setup for next vehicle. prevCrossedVehicle_Dir = dirOfCurrVehicle(NotTheOneAboutToBeSignaled).
					currentmovingdir = NORTHBOUND;				//Setup for next vehicle. nextVehicleToCross_Dir = dirOfVehicleAboutToBeSignaled.
					for(i=numMovingCars; i<3; i++)		//Executes 1 time, signaling 1 northboundCar.
					{
						pthread_cond_signal(&CarNorthboundMovable);		//Deplete the list of northbound cars that are still waiting (waitingNorthboundCars).
						if(DEBUG)fprintf(stderr,"VehicleRoutine: Car#%d(Nb) sent signal(CarNorthboundMovable)  (currMovDir==N && prevMovDir==N && 2NbMovingCars && #NbCarsWaiting>0)\n",currVehicleID);
					}
				}
				else
				{if(DEBUG)fprintf(stderr,"\n\nCar#%d did NOT signal because\n1) Their direction's list of cars is empty AND there are still two more cars left on the bridge (opposite directions can't travel simultaneously).  DEBUG:previousmovingdir==%s, currentmovingdir==%s  VehicleRoutine: Car: else if(numMovingCars==2)\n\n", currVehicleID, (previousmovingdir==NORTHBOUND ? "N":"S"), (currentmovingdir==NORTHBOUND ? "N":"S"));}
			}
			//if(threeCarsOnBridge){ waitForAtLeastOneCarToFinishCrossing; (This means to exit the if() and not signal any other vehicles)}
			else
			{if(DEBUG)fprintf(stderr,"BRUH HOW WAS THIS REACHED??? VehicleRoutine: Car#%d(%s) sent NO signal. Why?\n",currVehicleID, (currVehicle->direction==NORTHBOUND ? "N":"S"));}
			
			//Comments that may find a good place to be (somewhere that isn't here):
			//THE else if()s BELOW SHOULD ONLY BE REACHED ONCE ONE LIST OF CARS EMPTIES ENTIRELY
			//The blocks below AREN'T necessarily the cause of mixing directions on vehicles
		
		if(  !((currVehicle->direction!=previousmovingdir && (numWaitingCarsNorthbound==0 || numWaitingCarsSouthbound==0)/*atLeastOne_CarWaitingList_IsEmpty*/)//differentDirectionsIsOnlyLegalIfOneWaitingListIsEmpty. else if(illegalDirection)
			||( currVehicle->direction==previousmovingdir)))
		{if(DEBUG)fprintf(stderr,"VehicleRoutine: Car#%d(%s) is not allowed to cross since its direction would oppose a vehicle in movinglist. previousmovingdir==%s, currentmovingdir==%s\n",currVehicleID, (currVehicle->direction==NORTHBOUND ? "N":"S"), (previousmovingdir==NORTHBOUND ? "N":"S"), (currentmovingdir==NORTHBOUND ? "N":"S"));}
		

		//Needs to be in a different place but idk where.
		//if(numMovingCars>0 && currentmovingdir!=previousmovingdir)		//BridgeIsNotEmpty && directionsOfTwoMostRecentVehicles_ARE_DIFFERENT
		//	{											perror("Collision due to 1-lane bridge");}					//Collision due to 1-lane bridge

		if(numMovingCars<0 || numMovingTrucks<0){		perror("Vehicles cannot exist in negative quantities");}	//-# of vehicles is impossible
		if(numMovingCars>3){							perror("Bridge breaks from >3 cars on bridge at same time");}	//Bridge breaks
		if(numMovingTrucks>1){							perror("Bridge breaks from >1 truck on bridge");}				//Bridge breaks


		pthread_mutex_unlock(&lock);
	}


	//TRUCK   -------------------------------------------------------------------------------------
	else
	{
		pthread_mutex_lock(&lock);
		//Try to cross
		//if(there are any moving cars or any moving trucks [i.e., thereAreVehiclesOnTheBridge]){thisTruckCannotCross}
		//while(this vehicle cannot cross){	wait for proper moving signal}
		while(numMovingCars>0 || numMovingTrucks>0)	//These are global variables and can change as other vehicles cross.
		{
			if(currVehicle->direction == NORTHBOUND)	pthread_cond_wait(&TruckNorthboundMovable, &lock);
			else										pthread_cond_wait(&TruckSouthboundMovable, &lock);
		}

		//Now begin crossing
		//Direction doesn't matter since only 1 truck max can be crossing the bridge at any given time
		if(numWaitingTrucksNorthbound>0 || numWaitingTrucksSouthbound>0)	//if(atLeastOneTruck isWaiting)
		{
			//if(noCarsCrossingBridge && noTrucksCrossingBridge){letUpToOneTruckOnBridge;}
			if(numMovingCars==0 && numMovingTrucks==0)
			{
				fprintf(stderr,"VehicleRoutine: Will now move truck #%d (%s) across bridge:\n", currVehicleID, (currVehicle->direction==NORTHBOUND ? "Nb":"Sb"));
				if(DEBUG)
				{
					//fprintf(stderr,"VehicleRoutine START: BEFORE movinglistinsert() and waitinglistdelete() on vehicleID==%d (%s), numMovingTrucks++, numWaitingTrucksXbound--\n"
					fprintf(stderr,"VehicleRoutine START: BEFORE movinglistinsert() and waitinglistdelete() on vehicleID==%d (%s)\n"
									, currVehicleID, (currVehicle->direction==NORTHBOUND ? "Nb":"Sb"));
					printmoving(PRINT_LIST);
				}
				printwaiting(PRINT_LIST);

				//update global variables
				movinglistinsert( currVehicleID, currVehicleType, currVehicle->direction);
				waitinglistdelete(currVehicleID);
				numMovingTrucks++;					//if(DEBUG)fprintf(stderr,"VehicleRoutine: #numMovingTrucks++\n");
				if(		currVehicle->direction == NORTHBOUND)
					{numWaitingTrucksNorthbound--;	}//if(DEBUG)fprintf(stderr,"VehicleRoutine: #WaitingTrksNorthbound--\n");}
				else if(currVehicle->direction == SOUTHBOUND)
					{numWaitingTrucksSouthbound--;	}//if(DEBUG)fprintf(stderr,"VehicleRoutine: #WaitingTrksSouthbound--\n");}
				previousmovingdir = currVehicle->direction;	//Updates global var previousmovingdir for usage later during deciding who to signal
				
				if(DEBUG)
				{
					//fprintf(stderr,"\nVehicleRoutine DURING: AFTER movinglistinsert() and waitinglistdelete() on vehicleID==%d (%s), numMovingTrucks++, numWaitingTrucksXbound--\n"
					fprintf(stderr,"\nVehicleRoutine DURING: AFTER movinglistinsert() and waitinglistdelete() on vehicleID==%d (%s)\n"
									, currVehicleID, (currVehicle->direction==NORTHBOUND ? "Nb":"Sb"));
					printmoving(PRINT_LIST);
					printwaiting(PRINT_LIST);
				}
				else{printmoving(PRINT_LIST);}


				pthread_mutex_unlock(&lock);
				sleep(2);	//delay(2 seconds)  Simulates time taken to traverse the bridge
				pthread_mutex_lock(&lock);
				//Commenting out the mutex_unlock and mutex_lock immediately above causes twice-as-inaccurate direction switching. Idk why.


				//update global variables
				numMovingTrucks--; //if(DEBUG)fprintf(stderr,"VehicleRoutine: numMovingTrucks--\n");
				movinglistdelete(currVehicleID);

				//print out proper message
				//if(DEBUG)fprintf(stderr,"\n\nVehicleRoutine END: AFTER movinglistdelete() on vehicleID==%d, numMovingTrucks--\n", currVehicleID);
				if(DEBUG)fprintf(stderr,"\n\nVehicleRoutine END: AFTER movinglistdelete() on vehicleID==%d\n", currVehicleID);
				fprintf(stderr,"VehicleRoutine: Truck #%d (%s) exited the bridge.\n", currVehicleID, (currVehicle->direction==NORTHBOUND ? "Nb":"Sb"));
				if(DEBUG)printmoving(PRINT_LIST);
				printwaiting(PRINT_LIST);
			}
		}


		//send out signals to wake up vehicle(s) accordingly
		//I'm arbitrarily letting Southbound vehicles go first. Notice how these are ELSE IFs, not just IFs. EXACTLY one of these should execute per vehicleStateChange.
		//(currentmovingdir != xBOUND): Alternate between letting Southbound and Northbound trucks cross the bridge. I.e., S,N,S,N,... or N,S,N,S,...
		if(DEBUG)fprintf(stderr,"VehicleRoutine: (currentmovingdir == NORTHBOUND)==%s, (currVehicle->direction == NORTHBOUND)==%s\n",  (currentmovingdir == NORTHBOUND) ? "True":"False", (currVehicle->direction == NORTHBOUND) ? "True":"False");
		if(DEBUG)fprintf(stderr,"VehicleRoutine: (previousmovingdir == NORTHBOUND)==%s\n",(previousmovingdir == NORTHBOUND) ? "True":"False");


		if(numMovingTrucks == 0)	//Extra safety. Must be true for ANY vehicle to start moving
		{
			if(numWaitingTrucksNorthbound > 0  &&  previousmovingdir == SOUTHBOUND)
			{
				currentmovingdir = NORTHBOUND; //sets the direction to the signaled truck's dir, required for alternation
				previousmovingdir = NORTHBOUND;
				pthread_cond_signal(&TruckNorthboundMovable);
				fprintf(stderr,"VehicleRoutine: Truck#%d sent signal(TrkNorthboundMovable) (prevMovDir==S && thereAreNorthTrucksWaiting)\n",currVehicleID);
			}
			else if(numWaitingTrucksSouthbound > 0  &&  previousmovingdir == NORTHBOUND)//&& implicitly(noWaitingNorthboundTrucks  ||  previousmovingdir == NORTHBOUND)
			{
				currentmovingdir = SOUTHBOUND; //sets the direction to the signaled truck's dir, required for alternation
				previousmovingdir = SOUTHBOUND;
				pthread_cond_signal(&TruckSouthboundMovable); 
				fprintf(stderr,"VehicleRoutine: Truck#%d sent signal(TrkSouthboundMovable) (prevMovDir==N && thereAreSouthTrucksWaiting)\n",currVehicleID);
			}

			else if(numWaitingTrucksNorthbound > 0)//  && (implicit) previousmovingdir == NORTHBOUND)	//Should only be reached when southboundTruckWaitingList is empty.
			{
				currentmovingdir = NORTHBOUND;
				previousmovingdir = NORTHBOUND;
				pthread_cond_signal(&TruckNorthboundMovable);
				fprintf(stderr,"VehicleRoutine: Truck#%d sent signal(TrkNorthboundMovable)  (prevMovDir==N && noSouthTrucksWaiting && thereAreNorthTrucksWaiting)\n",currVehicleID);
			}
			else if(numWaitingTrucksSouthbound > 0)//  &&  (implicit) previousmovingdir == SOUTHBOUND)	//Should only be reached when northboundTruckWaitingList is empty.
			{
				currentmovingdir = SOUTHBOUND;
				previousmovingdir = SOUTHBOUND;
				pthread_cond_signal(&TruckSouthboundMovable);
				fprintf(stderr,"VehicleRoutine: Truck#%d sent signal(TrkSouthboundMovable)  (prevMovDir==S && noNorthTrucksWaiting && thereAreSouthTrucksWaiting)\n",currVehicleID);
			}

			//Signal a car if no trucks are waiting
			else if(numWaitingCarsSouthbound > 0)// && (implicit) noTrucksWaiting)
			{
				currentmovingdir = SOUTHBOUND;
				previousmovingdir = SOUTHBOUND;
				pthread_cond_signal(&CarSouthboundMovable);
				fprintf(stderr,"VehicleRoutine: Truck#%d sent signal(CarSouthboundMovable)  (noTrucksWaiting && thereAreSouthCarsWaiting)\n",currVehicleID);
			}
			else if(numWaitingCarsNorthbound > 0)// && (implicit) noTrucksWaiting)
			{
				currentmovingdir = NORTHBOUND;
				previousmovingdir = NORTHBOUND;
				pthread_cond_signal(&CarNorthboundMovable);
				fprintf(stderr,"VehicleRoutine: Truck#%d sent signal(CarNorthboundMovable)  (noTrucksWaiting && thereAreNorthCarsWaiting)\n",currVehicleID);
			}
		}

		if(DEBUG)fprintf(stderr,"\n\n\n\n");
		else{fprintf(stderr,"\n");}

		pthread_mutex_unlock(&lock);
	}
} // end of thread_routine()




void createVehicleAndThreadAndArrive(float probabilityOfCreatingCar, int numVehiclesToCreate, int startingArrayIndex_TID)
{
	pthread_mutex_lock(&lock);		//Acquire mutex lock

	//pmstr is a vehicle
	//vehicleList=(struct pmstr*)(  malloc( (startingArrayIndex_TID+numVehiclesToCreate+1) * sizeof(struct pmstr) )  );	//+1 because of array indexing beginning at 0

	float carprob = probabilityOfCreatingCar;
	for(j=startingArrayIndex_TID; j<(startingArrayIndex_TID+numVehiclesToCreate); j++)
	{
		//call rand() to decide vehicle type and direction
		int travelDir = rand()%2;	//As declared earlier, 0=Northbound, 1=Southbound
		short numFrom0to99 = rand()%100;
		float randomFrac = numFrom0to99 / (float)(100.0);	//Get normalized # from 0 to 0.99, This will be compared against the passed-in probabilityOfCreatingCar.
		//Why float? Just in case dividing makes it a double and consumes a few extra bytes of memory. Not really that important but eh.
		//numFrom0to99 instead of numFrom0to100 because comparing 1.0random < 1.0chanceOfCar == false, which means that truck gets assigned despite a 100% probability of car

		//Initalize (not declare) pmstr_t struct to save the vehicle type, direction, and id
		//vehicleList=(struct pmstr*)(  malloc( (j+1) * sizeof(struct pmstr) )  );	//j+1 because of array indexing beginning at 0
		//vehicleList[j] == one specific pmstr in the array of pmstrs
		vehicleList[j].vehicle_id   = j;
		vehicleList[j].vehicle_type = (randomFrac < carprob) ? TYPE_CAR : TYPE_TRUCK;	//if(randomFrac <= carprob){vehicleType=Car;} else{vehicleType=truck;}
		vehicleList[j].direction    = (randomFrac < .5) ? NORTHBOUND : SOUTHBOUND;	//if(randomFrac <= 50%){vehicleDirection=North;} else{vehicleDirection=South;}
		//(randomFrac < carprob) instead of (randomFrac <= carprob) because the latter allows cars to be created with carprob==0.0  !!!

		//call vehicle_arrival()
		vehicle_arrival( &(vehicleList[j]) );

		//create a pthread to represent the vehicle, vehicle_routine() is the start function of a pthread
		if( 0 != pthread_create(&(vehicleTIDs[j]), NULL, (void*)(&vehicle_routine), &(vehicleList[j])) )
			{perror("Failed to create thread");}
	}

	pthread_mutex_unlock(&lock);	//Release mutex lock
}


void vehicle_arrival(pmstr_t* pmstr_param)
{
	//Adjust vehicle counter
	if(pmstr_param->vehicle_type == TYPE_CAR)
	{
		if(		pmstr_param->direction == SOUTHBOUND)	//if(vehicleThatJustArrived{theOnePassedIntoFunc}_direction != 0{northbound})
			numWaitingCarsSouthbound++;
		else if(pmstr_param->direction == NORTHBOUND)
			numWaitingCarsNorthbound++;
		else{perror("ARRIVAL: Vehicle neither northbound nor southbound");}
	}
	else if(pmstr_param->vehicle_type == TYPE_TRUCK)
	{
		if(		pmstr_param->direction == SOUTHBOUND)	//if(vehicleThatJustArrived{theOnePassedIntoFunc}_direction != 0{northbound})
			numWaitingTrucksSouthbound++;
		else if(pmstr_param->direction == NORTHBOUND)
			numWaitingTrucksNorthbound++;
		else{perror("ARRIVAL: Vehicle neither northbound nor southbound");}
	}
	else{perror("ARRIVAL: Using nonexistent vehicle type (neither truck nor car)");}
	

	//Add arrived vehicle to waitingList
	waitinglistinsert(pmstr_param->vehicle_id, pmstr_param->vehicle_type, pmstr_param->direction);

	//Announce arrival to user
	if(pmstr_param->vehicle_type == TYPE_CAR)
	{
		if(		pmstr_param->direction == SOUTHBOUND)
			fprintf(stderr,"Car #%d (southbound) arrived.\n",pmstr_param->vehicle_id);
		else if(pmstr_param->direction == NORTHBOUND)
			fprintf(stderr,"Car #%d (northbound) arrived.\n",pmstr_param->vehicle_id);
		else{perror("ARRIVAL: Vehicle neither northbound nor southbound");}
	}
	else if(pmstr_param->vehicle_type == TYPE_TRUCK)
	{
		if(		pmstr_param->direction == SOUTHBOUND)
			fprintf(stderr,"Truck #%d (southbound) arrived.\n",pmstr_param->vehicle_id);
		else if(pmstr_param->direction == NORTHBOUND)
			fprintf(stderr,"Truck #%d (northbound) arrived.\n",pmstr_param->vehicle_id);
		else{perror("ARRIVAL: Vehicle neither northbound nor southbound");}
	}
	else{perror("ARRIVAL: Printing nonexistent vehicle type (neither truck nor car)");}
} // end of vehicle_arrival

//Insert new vehicle into waitinglist
void waitinglistinsert(int vehicle_id, int vehicle_type, int direction)
{
	struct waitinglist *p;
	p = (struct waitinglist*)( malloc( sizeof(struct waitinglist) ) );
	p->vehicle_id   = vehicle_id;
	p->vehicle_type = vehicle_type;
	p->direction    = direction;
	p->next = pw;
	pw = p;
}
//Remove vehicle from waitinglist
void waitinglistdelete(int vehicle_id)
{
	struct waitinglist *p, *pprevious;
	p = pw;	/*pw==globalPtrToWaitingList*/
	pprevious = p;
	while(p)
	{
		/*if(currVehicleIDinList == vehicleToDelete_ID){stopSearchingByBreakingOutOfWhileLoop;}*/
		if((p->vehicle_id)==(vehicle_id))
			break;
		else
		{
			pprevious = p;
			p=p->next;
		}
	}

	if(p==pw)
		pw = p->next;
	else
		pprevious->next = p->next;

	free(p);	//Release the memory that was allocated by waitinglistinsert()
}


//Insert new vehicle into movinglist
void movinglistinsert(int vehicle_id, int vehicle_type, int direction)
{
	struct movinglist *p;
	p=(struct movinglist*)( malloc( sizeof(struct movinglist) ) );
	p->vehicle_id = vehicle_id;
	p->vehicle_type = vehicle_type;
	p->direction = direction;
	p->next = pm;	/*pm==globalPtrToMovingList*/
	pm = p;
}
//Remove vehicle from movinglist
void movinglistdelete(int vehicle_id)
{
	struct movinglist *p, *pprevious;
	p = pm;	/*pm==globalPtrToMovingList*/
	pprevious = p;
	while(p)
	{
		if((p->vehicle_id)==(vehicle_id))
			break;
		else
		{
			pprevious = p;
			p = p->next;
		}
	}

	if(p==pm)
		pm = p->next;
	else
		pprevious->next  =  p->next;

	free(p);		//Release the memory that was allocated by movinglistinsert()
}

void printmoving(const bool printList)
{
	struct movinglist* p;
	p = pm;

	fprintf(stderr,"Now %d cars and %d trucks are moving.\n", numMovingCars, numMovingTrucks);
	//fprintf(stderr,"Current moving direction: %d.\n", currentmovingdir);

	if(printList)
	{
		fprintf(stderr,"Vehicles on the bridge: [");
		if(p==NULL)
		{
			fprintf(stderr,"]\n");
			return;	//Exit this function
		}

		while(p)	//Still executes when p==NULL??
		{

			if(		p->vehicle_type==TYPE_CAR)	//if(currVehicle_type isCar)
			{
				if(		p->direction==NORTHBOUND){fprintf(stderr,"Car #%d(N),",  p->vehicle_id);}
				else if(p->direction==SOUTHBOUND){fprintf(stderr,"Car #%d(S),",  p->vehicle_id);}
				else{perror("Printing nonexistent vehicle direction");}
			}
			else if(p->vehicle_type==TYPE_TRUCK)	//if(currVehicle_type isTruck)
			{
				if(		p->direction==NORTHBOUND){fprintf(stderr,"Truck #%d(N),",  p->vehicle_id);}
				else if(p->direction==SOUTHBOUND){fprintf(stderr,"Truck #%d(S),",  p->vehicle_id);}
				else{perror("Printing nonexistent vehicle direction");}
			}
			else{perror("PRINTMOVING: Printing nonexistent vehicle type (neither truck nor car)");}
			p = p->next;
		}

		fprintf(stderr,"]\n");
	}
}
void printwaiting(const bool printList)
{
	struct waitinglist* p;
	p = pw;	/*pw==globalPtrToWaitingList*/

	fprintf(stderr,"Now %d southbound cars and %d northbound cars are waiting.\n", numWaitingCarsSouthbound, numWaitingCarsNorthbound);
	fprintf(stderr,"Now %d southbound trucks and %d northbound trucks are waiting.\n", numWaitingTrucksSouthbound, numWaitingTrucksNorthbound);

	if(printList)
	{
		fprintf(stderr,"Waiting Vehicles (Northbound): [");
		if(p==NULL)
		{
			fprintf(stderr,"]\n");
			return;	//Exit this function
		}
		else
		{
			while(p)	//while(p) still executes when p==NULL??
			{
				if(		p->vehicle_type==TYPE_CAR)	//if(currVehicle_type isCar)
				{
					if(		p->direction==NORTHBOUND){fprintf(stderr,"Car #%d(N),",  p->vehicle_id);}
					else if(p->direction==SOUTHBOUND){}
					else{perror("Printing nonexistent vehicle direction");}
				}
				else if(p->vehicle_type==TYPE_TRUCK)	//if(currVehicle_type isTruck)
				{
					if(		p->direction==NORTHBOUND){fprintf(stderr,"Truck #%d(N),",  p->vehicle_id);}
					else if(p->direction==SOUTHBOUND){}
					else{perror("Printing nonexistent vehicle direction");}
				}
				else{perror("Printing nonexistent vehicle type (neither truck nor car)");}
				p = p->next;
			}
			fprintf(stderr,"]\n");

			p = pw;
			fprintf(stderr,"Waiting Vehicles (Southbound): [");
			while(p)	//while(p) still executes when p==NULL??
			{
				if(		p->vehicle_type==TYPE_CAR)	//if(currVehicle_type isCar)
				{
					if(		p->direction==NORTHBOUND){}
					else if(p->direction==SOUTHBOUND){fprintf(stderr,"Car #%d(S),",  p->vehicle_id);}
					else{perror("Printing nonexistent vehicle direction");}
				}
				else if(p->vehicle_type==TYPE_TRUCK)	//if(currVehicle_type isTruck)
				{
					if(		p->direction==NORTHBOUND){}
					else if(p->direction==SOUTHBOUND){fprintf(stderr,"Truck #%d(S),",  p->vehicle_id);}
					else{perror("Printing nonexistent vehicle direction");}
				}
				else{perror("Printing nonexistent vehicle type (neither truck nor car)");}
				p = p->next;
			}
			fprintf(stderr,"]\n");
		}
	}
}

