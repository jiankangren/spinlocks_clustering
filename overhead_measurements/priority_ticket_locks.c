#include <stdlib.h>
#include "atomic.h"

__thread prio my_prio;


extern void ticket_lock(ticketlock *lock);
extern void ticket_unlock(ticketlock *lock);

//extern void lock_mcs(mcs_lock *lock, mcs_lock_t *me);
//extern void unlock_mcs(mcs_lock *lock, mcs_lock_t *me);

void priority_set(int pr) {
	/* Priorities 0 (highest) to 63 (lowest) */
	my_prio.priority = 1ULL << pr;
}

void init_prio(int pr) {
	priority_set(pr);
}

void fini_prio(void) {

}

void plock_lock(plock *p, ticketlock *tlock) {
//void plock_lock(plock *p, mcs_lock *fifo_lock, mcs_lock_t *me) {

	ticket_lock(tlock);
	//	lock_mcs(fifo_lock, me);

	// Get the mask of requests with higher priority than me
	unsigned long long mask = my_prio.priority - 1;
	
	// Spin until there are no higher priority requests in the waiter list.
	while (p->waiters & mask) {
			cpu_relax();
	}

	// Attempt to acquire the lock. 
	// If success, return.
	if (!cmpxchg(&p->owner, NULL, &my_prio)) return;

	// If fail, add myself to the list of waiters
	atomic_or(&p->waiters, my_prio.priority);
	
	// Then spin until my turn
	while (1) {
		// Keep checking the waiter list until there are no higher priority requests
		while (p->waiters & mask) {
			if (!(my_prio.priority & p->waiters)) {
				atomic_or(&p->waiters, my_prio.priority);
			}
					
			cpu_relax();
		}

		// Attempt to acquire the lock when there are no higher priority requests.
		// If success, remove itself from the waiter list and return.
		if (!cmpxchg(&p->owner, NULL, &my_prio)) {
			atomic_and(&p->waiters, ~my_prio.priority);
			return;
		}
		
		// If fail, retry.
		cpu_relax();
	}
}

void plock_unlock(plock *p, ticketlock *tlock) {
//void plock_unlock(plock *p, mcs_lock *fifo_lock, mcs_lock_t *me) {

	//	barrier();
	ticket_unlock(tlock);
	//	unlock_mcs(fifo_lock, me);
	p->owner = NULL;
}
