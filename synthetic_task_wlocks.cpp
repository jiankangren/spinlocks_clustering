#include <omp.h>
#include <sstream>
#include <string>
#include <stdio.h>
#include "task.h"
#include "timespec_functions.h"
#include "atomic.h"
#include "spinlocks_util.h"
#include "statistics.h"

using namespace std;

extern void ticket_lock(volatile ticketlock* lock);
extern void ticket_unlock(volatile ticketlock* lock);
extern void plock_lock(volatile plock *lock, volatile ticketlock *local);
extern void plock_unlock(volatile plock *lock, volatile ticketlock *local);
extern void init_prio(int priority);
extern void fini_prio();

// Resource information read from task_manager.cpp
extern const char *locks_name;
extern unsigned resources_num;
extern const char *locks_type;
extern int priority;

// Name of the shared memory region for statistics data
extern const char* stat_name;

// Pointer to shared memory region for FIFO locks
volatile ticketlock  *ticketlocks;

// Pointer to shared memory region for Priority locks
volatile plock *plocks;
// Local locks that used to resolve contention inside the task
volatile ticketlock *locallocks;

prio my_prio;

// Pointer to shared memory region for statistics data
volatile struct waiting_requests_stat* stats;

// The argument list of a synthetic task includes:
// <program-name> <segments-num> \
// <strands-num> <len-sec> <len-nsec> <requests-num> <resource-id> <request-len-nsec> \
// .....
// Each segment requires a set of parameters as in the second line.

const long kNanosecInSec = 1000000000;

static string type;


// Called when a new request to the corresponding resource arrive. 
// It will increase the number of requests in the queue by one.
// Input is the index of the structure for that resource.
static void record_in_request(unsigned i) {
	struct waiting_requests_stat* stat = stats+i;
	if (stat->next_event >= NUM_EVENTS) {
		return;
	}

	pthread_spin_lock(&(stat->spinlock));
	if (stat->next_event == 0) {
		stat->data[stat->next_event] += 1;
	} else {
		stat->data[stat->next_event] = stat->data[stat->next_event-1] + 1;
	}
	stat->next_event++;
	printf("[in]: %d\n", stat->next_event);
	pthread_spin_unlock(&(stat->spinlock));
}

// Called when a request finishes (about to call unlock).
// It will decrease the number of requests in the queue by one.
static void record_out_request(unsigned i) {
	struct waiting_requests_stat* stat = stats+i;

	pthread_spin_lock(&(stat->spinlock));
	if (stat->next_event == 0) {
		fprintf(stderr, "WARNING: request out while there is no request in queue");
		return;
	} else {
		stat->data[stat->next_event] = stat->data[stat->next_event-1] - 1;
	}
	stat->next_event++;
	printf("[out]: %d\n", stat->next_event);
	pthread_spin_unlock(&(stat->spinlock));
}

// Input is the index of the corresponding lock object
static void lock_wrapper(unsigned i) {
	// Record an incoming request for statistics data
	record_in_request(i);

	if (type == "fifo") {
		ticket_lock(&ticketlocks[i]);
	} else if (type == "prio") {
		plock_lock(&plocks[i], &locallocks[i]);
	}
}

// Input is the index of the corresponding lock object
static void unlock_wrapper(unsigned i) {
	// Record an outgoing request for statistics data
	record_out_request(i);

	if (type == "fifo") {
		ticket_unlock(&ticketlocks[i]);
	} else if (type == "prio") {
		plock_unlock(&plocks[i], &locallocks[i]);
	}
}

int init(int argc, char* argv[]) {
	type = string(locks_type);

	if (type == "fifo") {
		ticketlocks = (volatile ticketlock*) get_spinlocks(locks_name, resources_num, locks_type);
		if (ticketlocks == NULL) {
			return -1;
		}
	} else if (type == "prio") {
		plocks = (volatile plock*) get_spinlocks(locks_name, resources_num, locks_type);
		if (plocks == NULL) {
			return -1;
		}

		locallocks = (ticketlock*) malloc(sizeof(ticketlock) * resources_num);
		if (locallocks == NULL) {
			return -1;
		}

		// Initialize local locks (ticketlocks)
		for (unsigned i=0; i<resources_num; i++) {
			locallocks[i].u = 0;
		}
	}

	// Init priority for this task (0 (highest) to 63 (lowest))
	// priority read from .rtps file starts from 1,
	// so we must subtract it by 1 before passing
	init_prio(priority - 1);


	// Get the pointer to shared memory region of statistics data
	size_t size = sizeof(struct waiting_requests_stat) * resources_num;
	int error_flag;
	stats = get_stat_mem(stat_name, size, &error_flag);
	if (stats == NULL) {
		return -1;
	}

	return 0;
}

int run(int argc, char *argv[])
{
    if (argc < 1)
    {
        fprintf(stderr, "ERROR: Two few arguments");
	    return -1;
    }
	
    int num_segments;
    if (!(std::istringstream(argv[1]) >> num_segments))
    {
        fprintf(stderr, "ERROR: Cannot parse input argument");
        return -1;
    }
    
    // For each segment
	for (int i = 0; i < num_segments; ++i)
	{
	    if (argc < 8 + 6*i)
	    {
	        fprintf(stderr, "ERROR: Two few arguments");
		    return -1;
	    }
	    

	    unsigned num_strands;
	    unsigned long len_sec, len_ns;

		// For now, assume that strands of a segment only access the same resource.
		// Will generalize later so that strands can access different resources.
		unsigned resource_id;
		unsigned num_requests;
		unsigned long cs_len; // in nanoseconds
	    if (!(
			  std::istringstream(argv[2 + 6*i]) >> num_strands &&
			  std::istringstream(argv[3 + 6*i]) >> len_sec &&
			  std::istringstream(argv[4 + 6*i]) >> len_ns &&
			  std::istringstream(argv[5 + 6*i]) >> num_requests &&
			  std::istringstream(argv[6 + 6*i]) >> resource_id &&
			  std::istringstream(argv[7 + 6*i]) >> cs_len
			  ))
			{
				fprintf(stderr, "ERROR: Cannot parse input argument");
				return -1;
			}


		// For now, assume at most 1 request per strand
		if (num_requests > num_strands) {
			num_requests = num_strands;
		}

		// Length of critical section is at most length of strand
		if ( cs_len > (len_sec*kNanosecInSec + len_ns) ) {
			cs_len = len_sec*kNanosecInSec + len_ns;
		}

		// Length of each critical section & 
		// the remaining length of strand excluding critical section
		timespec cs_length, remain_length;
		
		// Total length of each strand
		timespec segment_length = { len_sec, len_ns };

		if (num_requests > 0) {
			// Assume that critical section length is always less than 1 sec
			// which is reasonable. Thus, the first field is always zero.
			cs_length = { 0, cs_len };
			
			if (cs_len > len_ns) {
				remain_length = { (len_sec-1), (kNanosecInSec + len_ns - cs_len) };
			} else {
				remain_length = { len_sec, (len_ns-cs_len) };
			}
		}
		

		// For each strand in parallel
		#pragma omp parallel for schedule(runtime)
		for (unsigned j = 0; j < num_strands; ++j)
		{
			// Assign critical sections to the first strands
			if (num_requests > 0 && j < num_requests) {
				// Execute critical section at the beginning of strand
				// Resource is indexed from 1
				//ticket_lock(&locks[resource_id - 1]);
				lock_wrapper(resource_id - 1);
				
				busy_work(cs_length);

				unlock_wrapper(resource_id - 1);
				//ticket_unlock(&locks[resource_id - 1]);

				// Then execute the remaining of the strand
				busy_work(remain_length);
			} else {
				// Perform work
				busy_work(segment_length);
			}
		}
		
	}
	
	return 0;
}

int finalize(int argc, char* argv[]) {
	if (type == "fifo") {
		unmap_spinlocks(ticketlocks, resources_num, type);
	} else if (type == "prio") {
		unmap_spinlocks(plocks, resources_num, type);
		if (locallocks != NULL) {
			free((void*)locallocks);
		}
	}
	
	fini_prio();

	// Unmap shared memory region of statistics data
	unmap_stat_mem(stats, resources_num);

	return 0;
}

task_t task = {init, run, finalize};
