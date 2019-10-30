#include <stddef.h>
#include "atomic.h"


void lock_mcs(mcs_lock *m, mcs_lock_t *me)
{
	mcs_lock_t *tail;
	
	me->next = NULL;
	me->spin = 0;
	
	tail = xchg_64(m, me);
	
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

void unlock_mcs(mcs_lock *m, mcs_lock_t *me)
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
