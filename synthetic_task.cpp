// The argument list of a synthetic task includes:
// program-name num-segments \
// [num-strands len-sec len-ns [num-requests [offset res-id cslen-ns], ...], ...], ...

#include <omp.h>
#include <sstream>
#include <string>
#include <stdio.h>
#include <stdlib.h>
#include <iostream>
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

// Pointer to shared memory region for FIFO locks
volatile ticketlock  *ticketlocks;
// Pointer to shared memory region for Priority locks
volatile plock *plocks;
// Local locks that used to resolve contention inside the task
volatile ticketlock *locallocks;
prio my_prio;

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

	// Init "locking" priority for this task: 0 (highest) to 63 (lowest).
	// UPDATED: the priority values in rtps have value from 1 to 97;
	// where 97 is the highest scheduling priority. Hence we must 
	// convert this priority to a priority used for spin locks.
	init_prio(97 - priority);
	//init_prio(priority - 1);

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
		unmap_spinlocks(ticketlocks, resources_num, type);
	} else if (type == "prio") {
		unmap_spinlocks(plocks, resources_num, type);
		if (locallocks != NULL) {
			free((void*)locallocks);
		}
	}
	
	fini_prio();

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
