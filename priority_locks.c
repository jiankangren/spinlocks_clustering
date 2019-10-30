// Implementation of priority-ordered, FIFO tie-breaking spin locks
// It uses ticket lock to resolve contention between requests to the
// same resource that issued from the same task.

#include <stdlib.h>
#include "atomic.h"

// Local struct to store priority of a task
// Should store inside the task itself
extern prio my_prio;

extern void ticket_lock(volatile ticketlock *lock);
extern void ticket_unlock(volatile ticketlock *lock);

void priority_set(int pr)
{
	/* Priorities 0 (highest) to 63 (lowest) */
	my_prio.priority = 1ULL << pr;
}

void init_prio(int pr)
{
	priority_set(pr);
}

void fini_prio(void)
{

}

void plock_lock(volatile plock *p, volatile ticketlock *local_lock)
{
	ticket_lock(local_lock);
	
	unsigned long long mask = my_prio.priority - 1;
	
	while (p->waiters & mask) {
		cpu_relax();
	}
	
	if (!cmpxchg(&p->owner, NULL, &my_prio)) return;
	
	atomic_or(&p->waiters, my_prio.priority);
	
	while (1) {
		while (p->waiters & mask) {
			if (!(my_prio.priority & p->waiters)) {
				atomic_or(&p->waiters, my_prio.priority);
			}
			
			cpu_relax();
		}
		
		if (!cmpxchg(&p->owner, NULL, &my_prio)) {
			atomic_and(&p->waiters, ~my_prio.priority);
			return;
		}
		
		cpu_relax();
	}
}

void plock_unlock(volatile plock *p, volatile ticketlock *local_lock)
{
	// barrier() is called inside ticket_unlock()
	// so we don't need to call it here
	ticket_unlock(local_lock);
	p->owner = NULL;
}
