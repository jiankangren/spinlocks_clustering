// The argument list of a synthetic task includes:
// program-name num-segments 
// [num-strands len-sec len-ns [num-requests [offset res-id cslen-ns], ...], ...], ...

#include <omp.h>
#include <sstream>
#include <string>
#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include "../task.h"
#include "../timespec_functions.h"
#include "../atomic.h"
#include "spinlocks_util.h"
#include "../statistics.h"

using namespace std;

extern void lock_mcs(volatile mcs_lock *m, volatile mcs_lock_t *me);
extern void unlock_mcs(volatile mcs_lock *m, volatile mcs_lock_t *me);
extern void lock_prio(volatile prio_lock *lock, volatile mcs_lock_t *me);
extern void unlock_prio(volatile prio_lock *lock, volatile mcs_lock_t *me);
extern void init_prio_lock(prio_lock *lock);

// Resource information read from task_manager.cpp
extern const char *locks_name;
extern unsigned resources_num;
extern const char *locks_type;
extern int priority;
extern unsigned first_core, last_core;

// NOTE: Do not turn on compiler optimization! 
// We do not declare the lock variables as volatiles thus, 
// turning on optimization can cause trouble to the code.
// For now, let's just turn it off!

// Pointer to an array of MCS tail pointers in case of FIFO locks
volatile mcs_lock *tails;

// Pointer to an array of prio_lock objects in case of priority locks.
// Each task also needs to define its priority.
volatile prio_lock *locks;
int task_prio;

// In both FIFO and priority locks, each processor needs a 
// MCS object to call the locking interface
volatile mcs_lock_t *elems;

// Name of the shared memory region for statistics data
extern const char* stat_name;

// Pointer to shared memory region for statistics data
volatile struct waiting_requests_stat* stats;

const unsigned long kNanosecInSec = 1000000000;

static string type;

// A strand can be a list of Sections, each section can be
// a normal section (if res_id is 0) or a critical section (if res_id > 0).
// The other field is the length of this section, in nanoseconds.
typedef struct {
	unsigned res_id;
	unsigned long len_ns;
	timespec len;
} Section;

// Followings are components of a program built up from sections
typedef struct {
	unsigned num_sections;
	Section *sections;
} Strand;

typedef struct {
	unsigned num_strands;
	Strand *strands;
} Segment;

typedef struct {
	unsigned num_segments;
	Segment *segments;
} Program;


// Store structure of the task
// - A task consists of a list of segments (1st dimension)
// - Each segment contains a list of strands (2nd dimension)
// - Each strand is a list of sections (array of pointer, end with NULL)
Program program;

// Called when a new request to the corresponding resource arrive. 
// It will increase the number of requests in the queue by one.
// Input is the index of the structure for that resource.
static void record_in_request(unsigned i) {
	volatile struct waiting_requests_stat* stat = stats+i;

	pthread_spin_lock(&(stat->spinlock));
	if (stat->next_event >= MAX_EVENTS) {
		// Not recording if run out of space
		pthread_spin_unlock(&(stat->spinlock));
		return;
	}

	if (stat->next_event == 0) {
		stat->data[stat->next_event] += 1;
	} else {
		stat->data[stat->next_event] = stat->data[stat->next_event-1] + 1;
	}
	stat->next_event++;
	pthread_spin_unlock(&(stat->spinlock));
}

// Called when a request finishes (about to call unlock).
// It will decrease the number of requests in the queue by one.
static void record_out_request(unsigned i) {
	volatile struct waiting_requests_stat* stat = stats+i;

	pthread_spin_lock(&(stat->spinlock));
	if (stat->next_event >= MAX_EVENTS) {
		// Not recording if run out of space
		pthread_spin_unlock(&(stat->spinlock));
		return;
	}

	if (stat->next_event == 0) {
		fprintf(stderr, "WARNING: request out while there is no request in queue");
		pthread_spin_unlock(&(stat->spinlock));
		return;
	} else {
		stat->data[stat->next_event] = stat->data[stat->next_event-1] - 1;
	}
	stat->next_event++;
	pthread_spin_unlock(&(stat->spinlock));
}


// For debug only.
// Keep pointers to the beginning and end of the shared memory 
// region for spin locks.
//static void *first, *last;

// Input is the index of the corresponding lock object
static void lock_wrapper(unsigned i) {
	// Record an incoming request for statistics data
	record_in_request(i);

	// Use the calling thread's id as an index for its MCS object
	int tid = omp_get_thread_num();

	if (type == "fifo") {
		//		if (!(first <= (void*)(tails+i) && (void*)(tails+i) <= last && 
		//			  first <= (void*)(elems+first_core+tid) && (void*)(elems+first_core+tid) <= last)) {
		//			printf("ERROR: Access out-of-bound memory: first addr: %p, second addr: %p\n", 
		//				   (void*)(tails+i), (void*) (elems+first_core+tid));
		//		}

		lock_mcs(tails+i, elems + first_core + tid);
	} else if (type == "prio") {
		lock_prio(locks+i, elems + first_core + tid);
	}
}

// Input is the index of the corresponding lock object
static void unlock_wrapper(unsigned i) {
	// Record an outgoing request for statistics data
	record_out_request(i);

	// Use the calling thread's id as an index for its MCS object
	int tid = omp_get_thread_num();

	if (type == "fifo") {
		unlock_mcs(tails+i, elems + first_core + tid);
	} else if (type == "prio") {
		unlock_prio(locks+i, elems + first_core + tid);
	}
}

// Convert length in nanosecond to timespec
timespec ns_to_timespec(unsigned long len) {
	unsigned len_sec;
	unsigned len_ns;
	if (len >= kNanosecInSec) {
		len_sec = len/kNanosecInSec;
		len_ns = len - len_sec*kNanosecInSec;
	} else {
		len_sec = 0;
		len_ns = len;
	}

	timespec ret = {len_sec, len_ns};
	return ret;
}

// Initialize data structure for lock objects & program structure
int init(int argc, char* argv[]) {
	type = string(locks_type);

	if (type == "fifo") {
		get_fifo_locks(locks_name, resources_num, &tails, &elems);

		//		printf("Tail addr: %p. Elems addr: %p\n", tails, elems);
		if (tails == NULL || elems == NULL) {
			return -1;
		}

		// Start debugging
		//		first = (void*)tails;
		//		last = (void*)((char*)first + sizeof(mcs_lock)*resources_num + sizeof(mcs_lock_t)*48 - 1);
		// End debugging
	} else if (type == "prio") {
		get_priority_locks(locks_name, resources_num, &locks, &elems);
		
		if (locks == NULL || elems == NULL) {
			return -1;
		}
	}

	// Init "locking" priority for this task: 0 (highest) to 63 (lowest).
	// UPDATED: the priority values in rtps have value from 1 to 97;
	// where 97 is the highest scheduling priority. Hence we must 
	// convert this priority to a priority used for spin locks.
	//	init_prio(97 - priority);

	// Set locking priority for task: 0 (highest) to 63 (lowest).
	// Convert from the priority value read from .rtps file.
	task_prio = 97 - priority;

    if (argc < 1)
    {
        fprintf(stderr, "ERROR: Two few arguments");
	    return -1;
    }
	
    unsigned num_segments;
    if (!(std::istringstream(argv[1]) >> num_segments))
    {
        fprintf(stderr, "ERROR: Cannot parse input argument");
        return -1;
    }

	// Allocate memory for storing pointers of segments
	program.num_segments = num_segments;
	program.segments = (Segment*) malloc(num_segments * sizeof(Segment));
	if (program.segments == NULL) {
		fprintf(stderr, "ERROR: Cannot allocate memory for segments");
		return -1;
	}

	// Keep track of current argument index
	unsigned arg_idx = 2;
	for (unsigned i=0; i<num_segments; i++) {
		unsigned num_strands;
		if (!(std::istringstream(argv[arg_idx]) >> num_strands)) {
			fprintf(stderr, "ERROR: Cannot read number of strands");
			return -1;
		}
		arg_idx++;
		
		// Allocate memory for storing strands of this segment, NULL at the end
		Segment *current_segment = &(program.segments[i]);
		current_segment->num_strands = num_strands;
		current_segment->strands = (Strand*) malloc(num_strands * sizeof(Strand));
		if (current_segment->strands == NULL) {
			fprintf(stderr, "ERROR: Cannot allocate memory for strands");
			return -1;
		}

		unsigned len_sec;
		unsigned long len_ns;
		if (!(std::istringstream(argv[arg_idx]) >> len_sec &&
			  std::istringstream(argv[arg_idx+1]) >> len_ns)) {
			fprintf(stderr, "ERROR: Cannot parse input argument");
			return -1;
		}
		arg_idx += 2;
		unsigned long segment_len_ns = len_sec*kNanosecInSec + len_ns;

		for (unsigned j=0; j<num_strands; j++) {
			unsigned num_requests;
			if (!(std::istringstream(argv[arg_idx]) >> num_requests)) {
				fprintf(stderr, "ERROR: Cannot read number of requests");
				return -1;
			}
			arg_idx++;

			// A strand starts and ends with a normal section
			unsigned num_sections = 2*num_requests + 1;
			
			// Allocate memory for storing sections of this strand, NULL at the end
			Strand* current_strand = &(current_segment->strands[j]);
			current_strand->num_sections = num_sections;
			current_strand->sections = (Section*) malloc(num_sections * sizeof(Section));
			if (current_strand->sections == NULL) {
				fprintf(stderr, "ERROR: Cannot allocate memory for sections");
				return -1;
			}

			// Read information for each request
			unsigned long last_section_end_time = 0;
			for (unsigned k=0; k<num_requests; k++) {
				unsigned long offset, cs_len;
				unsigned res_id;
				if ( !(std::istringstream(argv[arg_idx]) >> offset &&
					   std::istringstream(argv[arg_idx+1]) >> res_id &&
					   std::istringstream(argv[arg_idx+2]) >> cs_len) ) {
					fprintf(stderr, "ERROR: Cannot read request information");
					return -1;
				}
				arg_idx += 3; // Go read the next request

				// Normal sections have resource id = 0
				current_strand->sections[2*k].res_id = 0;
				current_strand->sections[2*k].len_ns = offset - last_section_end_time;
				current_strand->sections[2*k].len = ns_to_timespec(offset-last_section_end_time);

				// Then critical section
				current_strand->sections[2*k+1].res_id = res_id;
				current_strand->sections[2*k+1].len_ns = cs_len;
				current_strand->sections[2*k+1].len = ns_to_timespec(cs_len);

				// Update end time of this section
				last_section_end_time = offset + cs_len;
			}
			// The last section is always a normal section
			current_strand->sections[num_sections-1].res_id = 0;
			current_strand->sections[num_sections-1].len_ns = segment_len_ns - last_section_end_time;
			current_strand->sections[num_sections-1].len = ns_to_timespec(segment_len_ns-last_section_end_time);
		}
			
	}

	/*
	// DEBUG
	cout << "Number of segments: " << num_segments << endl;

	for (unsigned i=0; i<num_segments; i++) {
		Segment *segment = &(program.segments[i]);
		unsigned num_strands = segment->num_strands;
		cout << "#strands: " << num_strands << endl;

		for (unsigned j=0; j<num_strands; j++) {
			Strand *strand = &(segment->strands[j]);
			unsigned num_sections = strand->num_sections;
			
			for (unsigned k=0; k<num_sections; k++) {
				unsigned res_id = strand->sections[k].res_id;
				cout << "<" << res_id << ", " << strand->sections[k].len_ns << "> ";
			}
			cout << endl;
		}
	}
	// END DEBUG
	*/

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
	unsigned num_segments = program.num_segments;
	for (unsigned i=0; i<num_segments; i++) {
		Segment *segment = &(program.segments[i]);
		unsigned num_strands = segment->num_strands;

		#pragma omp parallel for schedule(runtime)
		for (unsigned j=0; j<num_strands; j++) {
			Strand *strand = &(segment->strands[j]);
			unsigned num_sections = strand->num_sections;
			
			for (unsigned k=0; k<num_sections; k++) {
				unsigned res_id = strand->sections[k].res_id;
				if (res_id == 0 && strand->sections[k].len_ns > 0) {
					// Not critical section
					busy_work(strand->sections[k].len);
				} else if (res_id > 0) {
					// Critical section
					lock_wrapper(res_id-1);
					busy_work(strand->sections[k].len);
					unlock_wrapper(res_id-1);
				}
			}
		}

	}
	
	return 0;
}

int finalize(int argc, char* argv[]) {
	if (type == "fifo") {
		unmap_spinlocks((void*)tails, resources_num, type);
	} else if (type == "prio") {
		unmap_spinlocks((void*)locks, resources_num, type);
	}
	
	// Free memory used to store the program structure
	unsigned num_segments = program.num_segments;
	Segment *segments = program.segments;
	for (unsigned i=0; i<num_segments; i++) {
		Segment *segment = &(segments[i]);
		unsigned num_strands = segment->num_strands;
		Strand *strands = segment->strands;
		
		for (unsigned j=0; j<num_strands; j++) {
			Strand *strand = &(strands[j]);
			if (strand->sections != NULL) {
				free(strand->sections);
				strand->sections = NULL;
			}
		}

		free(strands);
		segment->strands = NULL;
	}

	free(segments);
	program.segments = NULL;

	// Unmap shared memory region of statistics data
	int ret_val = unmap_stat_mem(stats, resources_num);

	return ret_val;
}

task_t task = {init, run, finalize};
