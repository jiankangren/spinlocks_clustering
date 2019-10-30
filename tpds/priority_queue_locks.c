// A priority-ordered lock contains an array of pointers. Each slot
// of the array corresponds to a priority level, e.g., position 0
// associates with task with the highest priority. Each pointer 
// points to the tail of a MCS-liked list of requests issued by 
// the same task (requests from processors of the same task are 
// appended to this list). In each list, only the head processor
// can attempt to obtain the lock. The other processors of each list
// must spin locally on their flags in the same manner as in MCS 
// locks. The head of a list only attempts to acquire the lock 
// when all higher priority slots are empty (i.e., no requests 
// with higher priorities presents). 
// At this time, we use array of 64 elements, which allows up to 
// 64 tasks (priorities). Since each task is high-utilization, 
// this should be enough for machines with up to 64-cores.
// NOTE: this implementation assumes 64-bit pointer.

#include <string.h>
#include <stdint.h>
#include "../atomic.h"


// A variable stores priority of the calling task
// Highest is 0, lowest is 63
extern int task_prio;

extern void lock_mcs(volatile mcs_lock *lock, volatile mcs_lock_t *me);
extern void unlock_mcs(volatile mcs_lock *lock, volatile mcs_lock_t *me);


// Set @owner and all elements of @priority array to be NULL
void init_prio_lock(prio_lock *lock) {
	memset(lock, 0, sizeof(prio_lock));
}

#ifndef CACHE_ALIGNED

// Lock function
// @lock: pointer to the priority-ordered lock.
// @me  : pointer to a MCS structure associated with the 
//        calling processor.
void lock_prio(prio_lock *lock, mcs_lock_t *me) {

	mcs_lock_t **mcs_tail = &lock->priority[task_prio];

	// Spin until the calling processor is the head of the list
	lock_mcs(mcs_tail, me);

	// Check if there is no higher priority requests present.
	// If no, attempt to acquire the lock. Otherwise, spin.
	while(1) {
		int ready_to_go = 1;
		for (unsigned i=0; i<task_prio; i++) {
			if (lock->priority[i] != 0) {
				ready_to_go = 0;
				break;
			}
		}

		if (ready_to_go == 1) {
			if (!cmpxchg(&lock->owner, 0, me)) {
				return;
			}
		}

		cpu_relax();
	}
}

// Unlock function. Same inputs as the lock function.
void unlock_prio(prio_lock *lock, mcs_lock_t *me) {
	
	// Allow the next processor in the list to be the head
	mcs_lock_t **mcs_tail = &lock->priority[task_prio];
	unlock_mcs(mcs_tail, me);

	// The next processor in the list must check for 
	// any processors from higher priority slot before 
	// it can attempt to acquire the lock.
	lock->owner = 0;
}

#else // A separate cache line for each item in the array

void lock_prio(volatile prio_lock *lock, volatile mcs_lock_t *me) {

	mcs_lock_t* volatile *mcs_tail = &lock->priority[task_prio].ptr;

	// Spin until the calling processor is the head of the list
	lock_mcs(mcs_tail, me);

	// Check if there is no higher priority requests present.
	// If no, attempt to acquire the lock. Otherwise, spin.
	while(1) {
		int ready_to_go = 1;
		for (int i=0; i<task_prio; i++) {
			if (lock->priority[i].ptr != 0) {
				ready_to_go = 0;
				break;
			}
		}

		if (ready_to_go == 1) {
			if (!cmpxchg(&lock->owner, 0, me)) {
				return;
			}
		}

		cpu_relax();
	}	
}

void unlock_prio(volatile prio_lock *lock, volatile mcs_lock_t *me) {

	// Allow the next processor in the list to be the head
	mcs_lock_t* volatile *mcs_tail = &lock->priority[task_prio].ptr;
	unlock_mcs(mcs_tail, me);

	// The next processor in the list must check for 
	// any processors from higher priority slot before 
	// it can attempt to acquire the lock.
	lock->owner = 0;
}

#endif // CACHE_ALIGNED
