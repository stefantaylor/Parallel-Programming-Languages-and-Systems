#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <mpi.h>
#include "stack.h"

#define EPSILON 1e-3
#define F(arg)  cosh(arg)*cosh(arg)*cosh(arg)*cosh(arg)
#define A 0.0
#define B 5.0

#define SLEEPTIME 1

int *tasks_per_process;

double farmer(int);

void worker(int);

int main(int argc, char **argv ) {
  int i, myid, numprocs;
  double area, a, b;

  MPI_Init(&argc, &argv);
  MPI_Comm_size(MPI_COMM_WORLD,&numprocs);
  MPI_Comm_rank(MPI_COMM_WORLD,&myid);

  if(numprocs < 2) {
    fprintf(stderr, "ERROR: Must have at least 2 processes to run\n");
    MPI_Finalize();
    exit(1);
  }

  if (myid == 0) { // Farmer
    // init counters
    tasks_per_process = (int *) malloc(sizeof(int)*(numprocs));
    for (i=0; i<numprocs; i++) {
      tasks_per_process[i]=0;
    }
  }

  if (myid == 0) { // Farmer
    area = farmer(numprocs);
  } else { //Workers
    worker(myid);
  }

  if(myid == 0) {
    fprintf(stdout, "Area=%lf\n", area);
    fprintf(stdout, "\nTasks Per Process\n");
    for (i=0; i<numprocs; i++) {
      fprintf(stdout, "%d\t", i);
    }
    fprintf(stdout, "\n");
    for (i=0; i<numprocs; i++) {
      fprintf(stdout, "%d\t", tasks_per_process[i]);
    }
    fprintf(stdout, "\n");
    free(tasks_per_process);
  }
  MPI_Finalize();
  return 0;
  
}

double farmer(int numprocs) {
  // Stack to store tasks
	stack *bag = new_stack();
  // Some variables we will need
	double points[2];
  double *data;
  double newdata[4];
  int i = 0;
	// Populate stack with initial task
	points[0]=A;
  points[1]=B;
  push(points, bag);
  // States for whether workers are in use
  int worker[numprocs];
  for (i=1; i<numprocs; i++){
    worker[i]=0;
  }
  // Double for the total
  double total = 0;
  // MPI stuff...
  MPI_Status status;
  int tag;
  int source;

  // Loop goes here
  while(1){
    for (i=1; i<numprocs; i++){
      if (worker[i] == 0 && is_empty(bag) == 0){
        //if tere is an idle worker and a task to be done, pop task from stack and send to worker
        data = pop(bag);
        worker[i] = 1;
        tasks_per_process[i] += 1;
        MPI_Send(data, 2, MPI_DOUBLE, i, 1, MPI_COMM_WORLD);
      }
    }
    //receive message back from worker
    MPI_Recv(newdata, 3, MPI_DOUBLE, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
    source = status.MPI_SOURCE;
    tag = status.MPI_TAG;
    if (tag == 1){
      //if the tag is 1 then the values returned need to be pushed to the stack
      points[0]=newdata[0];
      points[1]=newdata[1];
      push(points, bag);
      points[0]=newdata[1];
      points[1]=newdata[2];
      push(points, bag);
    }
    else {
      //if the tag is not 1 then the value needs to be added to the total
      total = total + newdata[0];
    }

    //set the worker to idle now that all it's data has been dealt with (super safe)
    worker[source] = 0;

    //TERMMINATE condition -- if all workers are idle AND bag is empty
    //if flag is set by any worker then they are not all idle
    int flag = 0;
    for (i=1; i<numprocs; i++){
      if (worker[i] == 1){
        flag = 1;
        break;
      }
    }
    if (flag == 0 && is_empty(bag) == 1) {
      break;
    }

  }
  //Farmer is done. Send messages to tell workers to terminate
  for (i=1; i<numprocs; i++){
    MPI_Send(data, 2, MPI_DOUBLE, i, 9, MPI_COMM_WORLD);
  }
  //Return to the total
  return total;
}

void worker(int mypid) {
  //initiaise variables
  int tag;
  double left, right, fleft, fright, lrarea, mid, fmid, larea, rarea;   
  double data[2];
  double newdata[3];
  MPI_Status status;

  while(1){
    MPI_Recv(data, 2, MPI_DOUBLE, 0, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
    tag = status.MPI_TAG;
    if (tag == 9){
      //if message has been sent with tag 9, this is the signal to stop and we should break the loop and terminate
      break;
    }
    else{
      // performs quaod calculations
      left = data[0];
      right = data[1];
      fleft = F(left);
      fright = F(right);
      lrarea = (fleft + fright) / 2 * (right - left);
    
      mid = (left + right) / 2;
      fmid = F(mid);
      larea = (fleft + fmid) * (mid - left) / 2;
      rarea = (fmid + fright) * (right - mid) / 2;

      // epsilon comparison
      if( fabs((larea + rarea) - lrarea) > EPSILON ) {
        // if comparison succeeds send new tasks (in the form of start and end points)
        newdata[0] = left;
        newdata[1] = mid;
        newdata[2] = right;
        MPI_Send(newdata, 3, MPI_DOUBLE, 0, 1, MPI_COMM_WORLD);
      }else{
        // if the comparison fails then we have an area result
        newdata[0] = larea + rarea;
        MPI_Send(newdata, 1, MPI_DOUBLE, 0, 2, MPI_COMM_WORLD);
      usleep(SLEEPTIME);
      }

    }
  }
}

/*
###################################################################################################
# Report                                                                                          #
###################################################################################################
# I used the stack implemntation provided as my data structure in the farmer, and started by      #
# populating this stack with the initial values A and B. I then made an array of flags for each   #
# worker to show whether they were idle or busy. after initialising some more variables the code  #
# enters the main loop. The code looks at each worker in ascending order. If the worker is idle   #
# and stack is not empty then a task is popped from the stack and assigned to the worker via an   #
# MPI_Send. I chose to use the standard MPI_SEND as I felt a blocking implementation was simpler  #
# and safer to implement.                                                                         #
#                                                                                                 #
# The main loop the waits to receive data back from the worker. Again I chose to use a standard   #
# MPI_Recv as I felt it was safer and easier. I used wild cards such that the MPI_Recv could      #
# receive messages with any tag and from any source. The code then analyses the tag and the       #
# source to determine which worker it came from (so we know which worker can now be set to idle), #
# and what type of result we are receiving (tag of 1 indicates more tasks to push to the stack,   #
# tag of 2 indicates a result to be added to the total area). The relevant operation is then done #
# with the received data, the relevant worker set to idle, and the code checks to see whether it  #
# can terminate (break from the while loop). The condition for terminating is that all the        #
# workers are idle and that the stack is empty. If the farmer succeeds in terminating it sends    #
# messages to all the workers with a tag of 9, indicating that they should also termiante, and    #
# then returns the area (result).                                                                 #
#                                                                                                 #
# The worker implements a slightly modified version of the code provided in sequential quad. It   #
# begins by initialising variables (for use by the quad algorithm and MPI, then enters a main     #
# loop. It waits on an MPI_Recv (again, the standard one), strictly from process 0, the farmer.   #
# It will accept any tag, checks whether the tag is 9 (if so, it terminates). Else it uses the    #
# received data (in the form of a start and end point) to perform the first part of the quad      #
# algorithm (caculating total area, left area and right area). It then performs the comparison    #
# with epsilon and decides whether it is producing new tasks or an area result. Finally it sends  #
# whatever it produces back to the farmer using a standard MPI_Send, tagged with 1 for new tasks  #
# or tagged with 2 for an area result, and then sleeps.
###################################################################################################

*/

/*
# ----------------------------------------------------------------------------------------------- #

# ****************************   ___________   ___ _____  ____ ____   *************************** #
# ****************************  / ___/      | /  _]     |/    |    \  *************************** #
# **************************** (   \_|      |/  [_|   __|  o  |  _  | *************************** #
# ****************************  \__  |_|  |_|    _]  |_ |     |  |  | *************************** #
# ****************************  /  \ | |  | |   [_|   _]|  _  |  |  | *************************** #
# ****************************  \    | |  | |     |  |  |  |  |  |  | *************************** #
# ****************************   \___| |__| |_____|__|  |__|__|__|__| *************************** #

# ----------------------------------------------------------------------------------------------- #
*/