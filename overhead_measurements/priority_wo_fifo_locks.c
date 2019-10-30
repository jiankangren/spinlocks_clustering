#include <stdlib.h>
#include "atomic.h"

__thread prio my_prio;


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

void plock_lock(plock *p)
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

void plock_unlock(plock *p)
{
	barrier();
	p->owner = NULL;
}
