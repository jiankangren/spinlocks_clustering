#include <time.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "atomic.h"


static __thread prio my_prio;

static void priority_set(int pr)
{
	// Priorities 0 (highest) to 63 (lowest)
	my_prio.priority = 1ULL << pr;
}

static void init_prio(int pr)
{
	priority_set(pr);
}

static void fini_prio(void)
{

}

static void plock_lock(plock *p)
{
	unsigned long long mask = my_prio.priority - 1;
	
	while (p->waiters & mask)
		{
			cpu_relax();
		}

	if (!cmpxchg(&p->owner, NULL, &my_prio)) return;
	
	atomic_or(&p->waiters, my_prio.priority);
	
	while (1)
		{
			while (p->waiters & mask)
				{
					if (!(my_prio.priority & p->waiters))
						{
							atomic_or(&p->waiters, my_prio.priority);
						}
					
					cpu_relax();
				}

			if (!cmpxchg(&p->owner, NULL, &my_prio))
				{
					atomic_and(&p->waiters, ~my_prio.priority);
					return;
				}
			
			cpu_relax();
		}
}

static void plock_unlock(plock *p)
{
	barrier();
	p->owner = NULL;
}


// Code to test priority spin locks
static int done = 0;
static plock lock1;

static unsigned long long ttaken;
static unsigned long long locks_taken;


/* Read the TSC for benchmarking cycle counts */
static __inline__ unsigned long long readtsc(void)
{
	unsigned hi, lo;
	__asm__ __volatile__ ("rdtsc" : "=a"(lo), "=d"(hi));
	return lo + ((unsigned long long)hi << 32);
}

/*
void ts_diff (struct timespec* ts1, struct timespec* ts2, struct timespec* result){
	if( *ts1 > *ts2 ){
		result->tv_nsec = ts1->tv_nsec - ts2->tv_nsec;
		result->tv_sec = ts1->tv_sec - ts2->tv_sec;
	} else {
		result->tv_nsec = ts2->tv_nsec - ts1->tv_nsec;
		result->tv_sec = ts2->tv_sec - ts1->tv_sec;
	}

	if( result->tv_nsec < 0 ){            //If we have a negative nanoseconds                                                                                    
		result->tv_nsec += 1000000000; //value then we carry over from the                                                                                     
		result->tv_sec -= 1;               //seconds part of the timespec                                                                                          
	}
}
*/

const unsigned nsec_in_sec = 1000000000;

#define HIGH_SPIN 1000
#define MED_SPIN 10000

static void *high_thread1(void *dummy)
{
	unsigned long long tstart;
	unsigned long long tend;
	struct timespec starttime, endtime;
	int i;
	
	init_prio(0);
	
	while (!done)
		{
			tstart = readtsc();
			clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &starttime);
			plock_lock(&lock1);
			clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &endtime);
			tend = readtsc();

			//ts_diff(&starttime, &endtime, &diff);
			unsigned long long stime_nsec = starttime.tv_sec * nsec_in_sec + starttime.tv_nsec;
			unsigned long long etime_nsec = endtime.tv_sec * nsec_in_sec + endtime.tv_nsec;
			unsigned long long diff = etime_nsec - stime_nsec;

			printf("Time for high thread to get the lock: %llu ns\n", diff);

			if (tend > tstart)
				{
					ttaken += tend - tstart;
					locks_taken++;
				}
			for (i = 0; i < HIGH_SPIN; i++) barrier();
			plock_unlock(&lock1);
			for (i = 0; i < HIGH_SPIN; i++) barrier();
		}
	
	fini_prio();

	return NULL;
}

static void *med_thread(void *dummy)
{
	int i;
	
	init_prio(1);
	
	while (!done)
		{
			plock_lock(&lock1);
			for (i = 0; i < MED_SPIN; i++) barrier();
			plock_unlock(&lock1);
		}
	
	fini_prio();
	
	return NULL;
}

int main(void)
{
	pthread_t high, med1, med2, med3;
	
	pthread_create(&high, NULL, high_thread1, NULL);
	pthread_create(&med1, NULL, med_thread, NULL);
	pthread_create(&med2, NULL, med_thread, NULL);
	pthread_create(&med3, NULL, med_thread, NULL);
	
	sleep(5);
	
	barrier();
	done = 1;
	
	pthread_join(high, NULL);
	pthread_join(med1, NULL);
	pthread_join(med2, NULL);
	pthread_join(med3, NULL);
	
	printf("Total cycles waiting per lock: %llu\n", ttaken / locks_taken);
	
	return 0;
}
