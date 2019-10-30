#include <stddef.h>
#include <stdio.h>
#include "../atomic.h"


void lock_mcs(volatile mcs_lock *m, volatile mcs_lock_t *me)
{
	volatile mcs_lock_t *tail;

	me->next = 0;
	me->spin = 0;
	
	tail = (volatile mcs_lock_t*) xchg_64(m, me);
	
	/* No one there? */
	if (!tail) return;

	/* Someone there, need to link in */
	tail->next = me;

	/* Make sure we do the above setting of next. */
	barrier();
	
	/* Spin on my spin variable */
	while (!me->spin) cpu_relax();
	
	return;
}

void unlock_mcs(volatile mcs_lock *m, volatile mcs_lock_t *me)
{
	/* No successor yet? */
	if (!me->next)
		{
			/* Try to atomically unlock */
			if (cmpxchg(m, me, NULL) == me) return;
			
			/* Wait for successor to appear */
			while (!me->next) cpu_relax();
		}
	
	/* Unlock next one */
	me->next->spin = 1;
}
