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
	double points[2];
  double *data;
  double newdata[4];
	// Populate stack with initial task
	points[0]=A;
  points[1]=B;
  push(points, bag);
  // States for whether workers are in use
  int worker1, worker2, worker3, worker4;
  worker4 = 0;
  worker3 = 0;
  worker2 = 0;
  worker1 = 0;
  // Double for the total
  double total = 0;
  // Other stuff...
  MPI_Status status;
  int tag;
  int source;

  // Loop goes here
  int i = 0;
  while(1){
    i++;
    //printf("%d\n", i);
    if (worker1 == 0 && is_empty(bag) == 0){
      // if worker1 is idle then assign task to worker 1
      data = pop(bag);
      worker1 = 1;
      tasks_per_process[1] += 1;
      MPI_Send(data, 2, MPI_DOUBLE, 1, 1, MPI_COMM_WORLD);
    }
    if (worker2 == 0 && is_empty(bag) == 0){
      // if worker1 is idle then assign task to worker 1
      data = pop(bag);
      worker2 = 1;
      tasks_per_process[2] += 1;
      MPI_Send(data, 2, MPI_DOUBLE, 2, 1, MPI_COMM_WORLD);
    }
    if (worker3 == 0 && is_empty(bag) == 0){
      // if worker1 is idle then assign task to worker 1
      data = pop(bag);
      worker3 = 1;
      tasks_per_process[3] += 1;
      MPI_Send(data, 2, MPI_DOUBLE, 3, 1, MPI_COMM_WORLD);
    }
    if (worker4 == 0 && is_empty(bag) == 0){
      // if worker1 is idle then assign task to worker 1
      data = pop(bag);
      worker4 = 1;
      tasks_per_process[4] += 1;
      MPI_Send(data, 2, MPI_DOUBLE, 4, 1, MPI_COMM_WORLD);
    }

    MPI_Recv(newdata, 3, MPI_DOUBLE, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
    source = status.MPI_SOURCE;
    tag = status.MPI_TAG;
    if (tag == 1){
      //if the tag is 1 then the values returned need to be pushed to the stack
      points[0]=newdata[0];
      points[1]=newdata[1];
      printf("%lf %lf\n", points[0], points[1] );
      push(points, bag);
      points[0]=newdata[1];
      points[1]=newdata[2];
      printf("%lf %lf\n", points[0], points[1] );
      push(points, bag);
      int c = is_empty(bag);
    }
    else {
      //if the tag is not 1 then the value needs to be added to the total
      total = total + newdata[0];
    }

    switch(source){
      case 1: worker1 = 0;
        break;
      case 2: worker2 = 0;
        break;
      case 3: worker3 = 0;
        break;
      case 4: worker4 = 0;
        break;
    }
    if (worker1 == 0 && worker2 == 0 && worker3 == 0 && worker4 == 0 && is_empty(bag) == 1) {
      break;
    }
  }
  MPI_Send(data, 2, MPI_DOUBLE, 1, 9, MPI_COMM_WORLD);
  MPI_Send(data, 2, MPI_DOUBLE, 2, 9, MPI_COMM_WORLD);
  MPI_Send(data, 2, MPI_DOUBLE, 3, 9, MPI_COMM_WORLD);
  MPI_Send(data, 2, MPI_DOUBLE, 4, 9, MPI_COMM_WORLD);
  return total;
}

void worker(int mypid) {
  //initiaise variables
  int i = 0;
  int tag;
  double left, right, fleft, fright, lrarea, mid, fmid, larea, rarea;   
  double data[2];
  double newdata[3];
  MPI_Status status;

  while(1){
    i++;
    MPI_Recv(data, 2, MPI_DOUBLE, 0, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
    tag = status.MPI_TAG;
    if (tag == 9){
      //if message has been sent with tag 9, this is the signal to stop and we should break the loop
      break;
    }
    else{
      left = data[0];
      right = data[1];
      fleft = F(left);
      fright = F(right);
      lrarea = (fleft + fright) / 2 * (left - right);
    
      mid = (left + right) / 2;
      fmid = F(mid);
      larea = (fleft + fmid) * (mid - left) / 2;
      rarea = (fmid + fright) * (right - mid) / 2;


      if( fabs((larea + rarea) - lrarea) > EPSILON ) {
        newdata[0] = left;
        newdata[1] = mid;
        newdata[2] = right;
        MPI_Send(newdata, 3, MPI_DOUBLE, 0, 1, MPI_COMM_WORLD);
      }else{
        newdata[0] = larea + rarea;
        newdata[1] = 0;
        newdata[2] = 0;
        MPI_Send(newdata, 3, MPI_DOUBLE, 0, 2, MPI_COMM_WORLD);
      }

    }
  }
}

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