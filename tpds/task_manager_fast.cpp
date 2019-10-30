// Each real time task should be compiled as a separate program and include task_manager.cpp and task.h
// in compilation. The task struct declared in task.h must be defined by the real time task.
#define __STDC_FORMAT_MACROS
#include <stdint.h> //For uint64_t
#include <inttypes.h> //For PRIu64
#include <stdlib.h> //For malloc
#include <sched.h>
#include <unistd.h> 
#include <stdio.h>
#include <math.h>
#include <sstream>
#include <signal.h>
#include <omp.h>
#include <iostream>
#include <fstream>
#include <string>
#include "../task.h"
#include "../single_use_barrier.h"
#include "../timespec_functions.h"
#include "common.h"


// Name of the shared memory region for statistics data
// It is taken from clustering_launcher but should be 
// passed as an argument instead.
const char* stat_name;

const char *locks_name;
unsigned resources_num;
const char *locks_type;
int priority;
unsigned first_core, last_core;

int main(int argc, char *argv[])
{
	// Process command line arguments
	
	const char *task_name = argv[0];
	const int num_req_args = 17;
	if (argc < num_req_args)
	{
		fprintf(stderr, "ERROR: Too few arguments for task %s\n", task_name);
		kill(0, SIGTERM);
		return RT_GOMP_TASK_MANAGER_ARG_COUNT_ERROR;
	}
	
	//	int priority;
	unsigned num_iters;
	long period_sec, period_ns, deadline_sec, deadline_ns, relative_release_sec, relative_release_ns;
	if (!(
		std::istringstream(argv[1]) >> first_core &&
		std::istringstream(argv[2]) >> last_core &&
		std::istringstream(argv[3]) >> priority &&
		std::istringstream(argv[4]) >> period_sec &&
		std::istringstream(argv[5]) >> period_ns &&
		std::istringstream(argv[6]) >> deadline_sec &&
		std::istringstream(argv[7]) >> deadline_ns &&
		std::istringstream(argv[8]) >> relative_release_sec &&
		std::istringstream(argv[9]) >> relative_release_ns &&
		std::istringstream(argv[10]) >> num_iters
	))
	{
		fprintf(stderr, "ERROR: Cannot parse input argument for task %s", task_name);
		kill(0, SIGTERM);
		return RT_GOMP_TASK_MANAGER_ARG_PARSE_ERROR;
	}
	
	char *barrier_name = argv[11];

	// Son (21Sep2015): Read locks name, resources number
	// Son (27Sep2015): Read lock type as the next argument
	locks_name = argv[12];
	if(! (std::istringstream(argv[13]) >> resources_num) ) {
		fprintf(stderr, "ERROR: Read number of shared resources failed for task %s", task_name);
		kill(0, SIGTERM);
		return RT_GOMP_TASK_MANAGER_ARG_PARSE_ERROR;
	}
	locks_type = argv[14];

	std::string locks_type_str(locks_type);
	if (locks_type_str != "fifo" && locks_type_str != "prio") {
		fprintf(stderr, "ERROR: Wrong lock type: %s", locks_type);
		kill(0, SIGTERM);
		return RT_GOMP_TASK_MANAGER_ARG_PARSE_ERROR;
	}

	// Son (08Feb2016): read name of shared memory of stat data
	stat_name = argv[15];

	int task_argc = argc - (num_req_args-1);
	char **task_argv = &argv[num_req_args-1];

	timespec period = { period_sec, period_ns };
	timespec deadline = { deadline_sec, deadline_ns };
	timespec relative_release = { relative_release_sec, relative_release_ns };
	
	// Check if the task has a run function
	if (task.run == NULL)
	{
		fprintf(stderr, "ERROR: Task does not have a run function %s", task_name);
		kill(0, SIGTERM);
		return RT_GOMP_TASK_MANAGER_RUN_TASK_ERROR;
	}
	
	// Bind the task to the assigned cores
	cpu_set_t mask;
	CPU_ZERO(&mask);
	for (unsigned i = first_core; i <= last_core; ++i)
	{
		CPU_SET(i, &mask);
	}
	
	int ret_val = sched_setaffinity(getpid(), sizeof(mask), &mask);
	if (ret_val != 0)
	{
		perror("ERROR: Could not set CPU affinity");

		// SonDN (18 Jan, 2016): comment this out since it causes the group of processes terminate.
		// Instead just terminate this child process (must call await_single_use_barrier first since
		// other children tasks are waiting).
		//	kill(0, SIGTERM);

		// Wait at barrier for the other tasks
		if (await_single_use_barrier(barrier_name) != 0)
			{
				fprintf(stderr, "ERROR: Barrier error for task %s", task_name);
				kill(0, SIGTERM);
				return RT_GOMP_TASK_MANAGER_BARRIER_ERROR;
			}
		
		// Write to its output file saying that it failed
		fprintf(stdout, "Binding failed !");
		fflush(stdout);

		return RT_GOMP_TASK_MANAGER_CORE_BIND_ERROR;
	}
	
	// Set priority to the assigned real time priority
	sched_param sp;
	// Since each task runs on a set of dedicated cores, we
	// assign the highest priority possible (97) to all tasks
	// (Note that the priority in this work is only used to 
	// prioritize requests to shared resources).
	sp.sched_priority = 97;
	//	sp.sched_priority = priority;
	ret_val = sched_setscheduler(getpid(), SCHED_FIFO, &sp);
	if (ret_val != 0)
	{
		perror("ERROR: Could not set process scheduler/priority");
		kill(0, SIGTERM);
		return RT_GOMP_TASK_MANAGER_SET_PRIORITY_ERROR;
	}
	
	// Set OpenMP settings
	omp_set_dynamic(0);
	omp_set_nested(0);
	//LJ
	omp_set_schedule(omp_sched_dynamic, 1);
	//omp_set_schedule(omp_sched_guided, 1);
	omp_set_num_threads(omp_get_num_procs());

	//	fprintf(stderr, "==== INFO ====: OpenMP get_num_procs: %d. Number of cores: %d\n", omp_get_num_procs(), last_core-first_core+1);
	
	omp_sched_t omp_sched;
	int omp_mod;
	omp_get_schedule(&omp_sched, &omp_mod);
	fprintf(stderr, "OMP sched: %u %u\n", omp_sched, omp_mod);
	
	fprintf(stderr, "Initializing task %s\n", task_name);

	//Create storage for per-job timings
	//	uint64_t *period_timings = (uint64_t*) malloc(num_iters * sizeof(uint64_t));

	// Initialize the task
	if (task.init != NULL)
	{
		ret_val = task.init(task_argc, task_argv);
		if (ret_val != 0)
		{
			fprintf(stderr, "ERROR: Task initialization failed for task %s", task_name);
			kill(0, SIGTERM);
			return RT_GOMP_TASK_MANAGER_INIT_TASK_ERROR;
		}
	}
	
	fprintf(stderr, "Task %s reached barrier\n", task_name);
	
	// Wait at barrier for the other tasks
	ret_val = await_single_use_barrier(barrier_name);
	if (ret_val != 0)
	{
		fprintf(stderr, "ERROR: Barrier error for task %s", task_name);
		kill(0, SIGTERM);
		return RT_GOMP_TASK_MANAGER_BARRIER_ERROR;
	}
	
	// Initialize timing controls
	unsigned deadlines_missed = 0;
	timespec correct_period_start, actual_period_start, period_finish, period_runtime;
	get_time(&correct_period_start);
	correct_period_start = correct_period_start + relative_release;
	timespec max_period_runtime = { 0, 0 };
	uint64_t total_nsec = 0;


	for (unsigned i = 0; i < num_iters; ++i)
	{
		// Sleep until the start of the period
		sleep_until_ts(correct_period_start);
		get_time(&actual_period_start);
	
		// Run the task
		ret_val = task.run(task_argc, task_argv);
		get_time(&period_finish);
		if (ret_val != 0)
		{
			fprintf(stderr, "ERROR: Task run failed for task %s", task_name);
			return RT_GOMP_TASK_MANAGER_RUN_TASK_ERROR;
		}
		
		// Check if the task finished before its deadline and record the maximum running time
		ts_diff(actual_period_start, period_finish, period_runtime);
		if ( i != 0 ) { //current program has first iteration setup
                                //that throws off max
			if (period_runtime > deadline) {
				deadlines_missed += 1;
				// If any job misses deadline, we stop and let clustering_launcher knows
				// immediately the task failed to schedule.
				break;
			}

			if (period_runtime > max_period_runtime) max_period_runtime = period_runtime;
			total_nsec += period_runtime.tv_nsec + nsec_in_sec * period_runtime.tv_sec;
		}

		// Son (Jan 31, 2016): record the response time for each job. May need to abort the first job.
		//		period_timings[i] = period_runtime.tv_nsec + nsec_in_sec * period_runtime.tv_sec;

		// Update the period_start time
		correct_period_start = correct_period_start + period;
	}
	
	// Finalize the task
	if (task.finalize != NULL) 
	{
		ret_val = task.finalize(task_argc, task_argv);
		if (ret_val != 0)
		{
			fprintf(stderr, "WARNING: Task finalization failed for task %s\n", task_name);
		}
	}

	if (deadlines_missed > 0) {
		// Return a value to clustering_launcher to let it know the task failed to schedule
		return RT_GOMP_TASK_MANAGER_MISSED_DEADLINE;
	}

	// Return no job missed dealine
	return RT_GOMP_TASK_MANAGER_SUCCESS;

	/*
	fprintf(stdout,"Deadlines missed for task %s: %d/%d\n", task_name, deadlines_missed, num_iters);
	fprintf(stdout,"Max running time for task %s: %i sec  %lu nsec\n", task_name, (int)max_period_runtime.tv_sec, max_period_runtime.tv_nsec);
	fprintf(stdout,"Avg running time for task %s: %" PRIu64  " nsec\n", task_name, total_nsec/(num_iters-1));

	// SonDN (Jan 31, 2016): write the recorded response times to the file
	for (unsigned i=0; i<num_iters; i++) {
		fprintf(stdout, "%" PRIu64 "\n", period_timings[i]);
	}
	
	// Remember to free allocated memory
	free(period_timings);

	fflush(stdout);
	
	return RT_GOMP_TASK_MANAGER_SUCCESS;
	*/
}
