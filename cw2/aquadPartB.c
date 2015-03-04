#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <mpi.h>

#define EPSILON 1e-3
#define F(arg)  cosh(arg)*cosh(arg)*cosh(arg)*cosh(arg)
#define A 0.0
#define B 5.0

#define SLEEPTIME 1

int *tasks_per_process;


//make new tuple type, to return 2 values from quad function (area nad count)
typedef struct{
  double area;
  double count;
} tuple;

tuple quad (double, double, double, double, double, double);

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


//modified quad function to count how many times quad is called as well as calculating the area
tuple quad(double left, double right, double fleft, double fright, double lrarea, double count) {
  double mid, fmid, larea, rarea;
  count ++;
  tuple l;
  tuple r;
  mid = (left + right) / 2;
  fmid = F(mid);
  larea = (fleft + fmid) * (mid - left) / 2;
  rarea = (fmid + fright) * (right - mid) / 2;
  if( fabs((larea + rarea) - lrarea) > EPSILON ) {
    l = (quad(left, mid, fleft, fmid, larea, count));
    r = (quad(mid, right, fmid, fright, rarea, count));
    larea = l.area;
    rarea = r.area;
    count = l.count + r.count - count;
  }
  double a = (larea + rarea);
  double b = count;
  tuple result = {a,b};
  return result;
}

//farmer
double farmer(int numprocs) {
  MPI_Status status;
  double data[2];
  double area = 0;
  double step = (B - A) / (numprocs-1);
  double i = 0;
  double range[2];
  int proc = 1;
  range[0] = 0;
  range[1] = A;

  //calculates the ranges to be used based on the start and end points, and the number of processors
  //then sends a range to be worked on to each processor in ascending order
  for (i = A+step; i <= B; i+=step){
    range[0] = range[1];
    range[1] = i;
    MPI_Send(range, 2, MPI_DOUBLE, proc, 1, MPI_COMM_WORLD);
    proc += 1;
  }

  //waits to receive a message from each processor. Can receive a message from any processor (so whichever finished first)
  //then extracts area and count from the message and calculates totals
  for (proc = 1; proc < numprocs; proc++){
    MPI_Recv(data, 2, MPI_DOUBLE, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
    int source = status.MPI_SOURCE;
    area += data[0];
    tasks_per_process[source] = (int)data[1];
  }
  return area;
}

//worker. Receives message, calls quad function, and sends results on.
void worker(int mypid) {
  MPI_Status status;
  double quadcount = 0;
  double data[2];
  tuple result;
  MPI_Recv(data, 2, MPI_DOUBLE, 0, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
  double left, right, fleft, fright, lrarea;
  left = data[0];
  right = data[1];
  fleft = F(left);
  fright = F(right);
  lrarea = (fleft + fright) * (left - right) / 2;
  result = quad(left, right, fleft, fright, lrarea, quadcount);
  data[0] = result.area;
  data[1] = result.count;
  MPI_Send(data, 2, MPI_DOUBLE, 0, 1, MPI_COMM_WORLD);
}

/*
###################################################################################################
# Report                                                                                          #
###################################################################################################
# The code for this task is relatively simple, and I approached the problem as it was outlined in #
# the handout; namely, I had a farmer distribute a set range of tasks to workers, which called the#
# quad function provided. I modified this quad function to enable task counting, and created my   #
# own tuple struct to enable it to return two arguments, the area and the count.                  #

# I made sure to allow the code to scale to any given number of processors, and trust that the    #
# user will enter sensible values or else the code maybe be very slow.                            #
#                                                                                                 #
# I make use of send and receive in standard mode.                                                #
# I wanted the sends to be blocking so that safety was ensured and no data was lost. Similarly I  #
# also used blocking receives to ensure no data was lost from buffers.                            #
#                                                                                                 #
# I didn't need to use any tags in this code, so I gave my receives MPI_ANY_TAG wildcards. I could#
# have specified the tags, but opted not to. In my sends I arbitrarilary gave the tags a value of #
# 1. I let me receiver in the farmer accept from any source so that it would receive messages as  #
# soon as they were available, but the for the receiver in the worker I specified that the source #
# must be the farmer, for safety.                                                                 #
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