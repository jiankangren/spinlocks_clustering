#define _GNU_SOURCE
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <time.h>
#include <limits.h>
#include <pthread.h>
#include <errno.h>
#include <assert.h>
#include "atomic.h"

const unsigned ITERS_PER_THR = 100000;

extern void ticket_lock(ticketlock *lock);
extern void ticket_unlock(ticketlock *lock);

unsigned long long to_ns(struct timespec *t) {
	return (t->tv_sec * 1000000000 + t->tv_nsec);
}

pthread_barrier_t barrier;

// Timestamps for each thread
struct TimeStamps {
	struct timespec stime, etime;
};

struct ThreadData {
    unsigned core_id; // start from 0
	ticketlock *lock;
	struct TimeStamps *timestamps;
};


// A global variable to emulate work inside critical sections
unsigned long long x = 0;

void *thr_run(void *input) {
	struct timespec stime, etime;
	struct ThreadData *data = (struct ThreadData*) input;
	unsigned core = data->core_id;
	ticketlock* lock = data->lock;
	struct TimeStamps *timestamps = data->timestamps;
	
	pthread_t thread_id = pthread_self();

	// Bind this thread to the core id tid
	cpu_set_t my_set;
	CPU_ZERO(&my_set);
	CPU_SET(core, &my_set);
	if (pthread_setaffinity_np(thread_id, sizeof(cpu_set_t), &my_set)) {
		perror("ERROR: Could not set affinity.");
		return (void*)-1;
	}

	struct sched_param sp;
	sp.sched_priority = 98;
    if (sched_setscheduler(0, SCHED_FIFO, &sp)) {
		perror("ERROR: Could not set process scheduler/priority");
		return (void*)-1;
	}

	// Wait for other threads
	int ret = pthread_barrier_wait(&barrier);
	if (ret != PTHREAD_BARRIER_SERIAL_THREAD && ret != 0) {
		printf("ERROR: Barrier wait failed");
		return (void*)-1;
	}

	// Every threads arrive, now all go
	clock_gettime(CLOCK_MONOTONIC, &stime);
	//	clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &stime);
	//	clock_gettime(CLOCK_THREAD_CPUTIME_ID, &stime);
	for (unsigned i=0; i<ITERS_PER_THR; i++) {
		ticket_lock(lock);
		x++;
		//barrier();
		ticket_unlock(lock);
	}
	//	clock_gettime(CLOCK_THREAD_CPUTIME_ID, &etime);
	//	clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &etime);
	clock_gettime(CLOCK_MONOTONIC, &etime);

	// Store time stamps of the thread
	timestamps->stime.tv_sec = stime.tv_sec;
	timestamps->stime.tv_nsec = stime.tv_nsec;
	timestamps->etime.tv_sec = etime.tv_sec;
	timestamps->etime.tv_nsec = etime.tv_nsec;

	return (void*)0;
}

int main(int argc, char *argv[]) {
	// Default number of threads
	unsigned NUM_THREADS = 8;

	if (argc == 2) {
		NUM_THREADS = atoi(argv[1]);
	} else if (argc == 1) {
		printf("Default number of threads: %d\n", NUM_THREADS);
	} else {
		printf("Usage: %s number_of_threads\n", argv[0]);
		return 0;
	}

	// Bind process to a set of processors
	cpu_set_t my_set;
	CPU_ZERO(&my_set);
	for (unsigned i=0; i<NUM_THREADS; i++) {
		CPU_SET(i, &my_set);
	}
	if (sched_setaffinity(getpid(), sizeof(cpu_set_t), &my_set)) {
		perror("ERROR: Could not set affinity.");
		return -1;
	}

	struct sched_param sp;
	sp.sched_priority = 98;
    if (sched_setscheduler(getpid(), SCHED_FIFO, &sp)) {
		perror("ERROR: Could not set process scheduler/priority");
		return -1;
	}
	
	//	struct TimeStamps stamps[NUM_THREADS];
	struct TimeStamps *stamps = (struct TimeStamps*) malloc(NUM_THREADS*sizeof(struct TimeStamps));
	
	//	ticketlock lock = {.u = 0};
	ticketlock *lock = (ticketlock*) malloc(sizeof(ticketlock));
	lock->u = 0;

	struct timespec stime, etime;
	unsigned long long single_total = 0;
	
	/*
	// Time it takes to run on a single processor
	//	clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &stime);
	for (unsigned i=0; i<NUM_THREADS; i++) {
		//clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &stime);
		clock_gettime(CLOCK_MONOTONIC, &stime);
		for (unsigned j=0; j<ITERS_PER_THR; j++) {
			x++;
			//barrier();
		}
		clock_gettime(CLOCK_MONOTONIC, &etime);
		//clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &etime);

		unsigned long long diff = to_ns(&etime) - to_ns(&stime);
		single_total += diff;
	}
	//	clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &etime);

	printf("Time to run on a single CPU: %llu ns\n", single_total);
	//	unsigned long long diff = to_ns(&etime) - to_ns(&stime);
	//	printf("Time to run on a single CPU: %llu ns\n", diff);
	*/

	///////// First way to measure ////////////////
	clock_gettime(CLOCK_MONOTONIC, &stime);
	for (unsigned i=0; i<NUM_THREADS*ITERS_PER_THR; i++) {
		x++;
	}
	clock_gettime(CLOCK_MONOTONIC, &etime);

	single_total += to_ns(&etime) - to_ns(&stime);
	/////////////////////////////////////////////////

	/*
	/////////// Second way to measure ////////////////
	unsigned long long diff;

	for (unsigned i=0; i<NUM_THREADS*ITERS_PER_THR; i++) {
		clock_gettime(CLOCK_MONOTONIC, &stime);
		x++;
		clock_gettime(CLOCK_MONOTONIC, &etime);

		diff = to_ns(&etime) - to_ns(&stime);
		single_total += diff;
	}
	/////////////////////////////////////////////////
	*/

	printf("Time to run on a single CPU: %llu ns\n", single_total);

	// Reset global variable x
	x = 0;

	// Run on multiple threads
	pthread_t threads[NUM_THREADS];

	// Thread data
	//	struct ThreadData data[NUM_THREADS];
	struct ThreadData *data = (struct ThreadData*) malloc(NUM_THREADS*sizeof(struct ThreadData));
	
	if (pthread_barrier_init(&barrier, NULL, NUM_THREADS)) {
		perror("ERROR: Create barrier failed");
		return -1;
	}

	for (unsigned i=0; i<NUM_THREADS; i++) {
		data[i].core_id = i;
		//data[i].lock = &lock;
		//data[i].timestamps = &stamps[i];
		data[i].lock = lock;
		data[i].timestamps = stamps+i;

		if (pthread_create(&threads[i], NULL, thr_run, &data[i])) {
			perror("ERROR: Create a new pthread failed.");
			return -1;
		}
	}

	for (unsigned int i=0; i<NUM_THREADS; i++) {
		if (pthread_join(threads[i], NULL)) {
			perror("WARNING: Join pthread failed");
		}
	}

	pthread_barrier_destroy(&barrier);

	// Check the value of x 
	assert(x == NUM_THREADS * ITERS_PER_THR);
	
	// Find the first starting timestamp, and the last finishing timestamp
	unsigned long long min_start, max_end;
	min_start = to_ns(&(stamps[0].stime));
	max_end = to_ns(&(stamps[0].etime));
	
	for (unsigned i=1; i<NUM_THREADS; i++) {
		if (to_ns(&(stamps[i].stime)) < min_start)
			min_start = to_ns(&(stamps[i].stime));
		if (to_ns(&(stamps[i].etime)) > max_end)
			max_end = to_ns(&(stamps[i].etime));
	}
	unsigned long long mean = ((max_end - min_start) - single_total)/(NUM_THREADS * ITERS_PER_THR);

	// Process elapsed times of all threads
	unsigned long long total = max_end - min_start;
	printf("Time to run on multiple threads: %llu ns\n", total);

	printf("Average overhead per critical section: %llu ns\n", mean);

	return 0;
}
