#ifndef ATOMIC_H
#define ATOMIC_H

#define E_BUSY 1
#define CACHE_LINE_SIZE 64 // Cache line size of 64 bytes

// Currently priority locks support 64 priority levels (i.e., 64 tasks)
#define PRIO_SIZE 64

// Align cache to avoid false sharing
#define CACHE_ALIGNED

typedef union ticketlock ticketlock;
union ticketlock
{
    unsigned u;
    struct
    {
        unsigned short ticket;
        unsigned short users;
    } s;
};

// Definition of priority. Currently support 64 priorities:
// Highest priority is 0, lowest is 63
typedef struct prio prio;
struct prio
{
	unsigned long long priority;
};

// Definition of priority-based lock structure
typedef struct plock plock;
struct plock
{
	prio *owner;
	unsigned long long waiters;
};


// Definition of MCS lock structure
typedef struct mcs_lock_t mcs_lock_t;
struct mcs_lock_t
{
	volatile mcs_lock_t *volatile next;
	volatile int spin;
} __attribute__ ((aligned(CACHE_LINE_SIZE)));
typedef struct mcs_lock_t *mcs_lock;


#ifndef CACHE_ALIGNED
// Definition of array-based priority locks
// @owner: pointer to the current owner of the lock, 
//         NULL means the lock is free.
// @priority: array of lists of requests, each for a task
//            at a certain priority. Slot with value NULL
//            means there is no requests from that task.
//            Slot 0 has the highest priority, down to slot 63.
typedef struct prio_lock {
	mcs_lock_t *owner;
	mcs_lock_t *priority[PRIO_SIZE];
} prio_lock;

#else  // Use separate cache lines for each item in the array
// The same array-based priority locks but with aligned cache
typedef struct task_head {
	mcs_lock_t *volatile ptr;
} __attribute__ ((aligned(CACHE_LINE_SIZE))) task_head;

typedef struct prio_lock {
	mcs_lock_t *volatile owner;
	task_head priority[PRIO_SIZE];
} prio_lock;
#endif // CACHE_ALIGNED


#define atomic_xadd(P, V) __sync_fetch_and_add((P), (V))
#define cmpxchg(P, O, N) __sync_val_compare_and_swap((P), (O), (N))
#define atomic_inc(P) __sync_add_and_fetch((P), 1)
#define atomic_dec(P) __sync_add_and_fetch((P), -1) 
#define atomic_add(P, V) __sync_add_and_fetch((P), (V))
#define atomic_set_bit(P, V) __sync_or_and_fetch((P), 1<<(V))
#define atomic_clear_bit(P, V) __sync_and_and_fetch((P), ~(1<<(V)))
#define atomic_or(P, V) __sync_or_and_fetch((P), (V))
#define atomic_and(P, V) __sync_and_and_fetch((P), (V))

/* Compile read-write barrier */
#define barrier() asm volatile("": : :"memory")

/* Pause instruction to prevent excess processor bus usage */ 
#define cpu_relax() asm volatile("pause\n": : :"memory")

/* Atomic exchange (of various sizes) */
static inline volatile void *xchg_64(volatile void *ptr, volatile void *x)
{
	/* From Intel docs: XCHG instructions that reference memory will implement 
	   the processor's locking protocol, thus will lock the bus and are atomic.
	   Other instructions need to be prefixed by the LOCK prefix to lock the bus.
	 */
	__asm__ __volatile__("xchgq %0,%1"
						 :"=r" ((unsigned long long) x)
						 :"m" (*(volatile long long *)ptr), "0" ((unsigned long long) x)
						 :"memory");

	return x;
}

static inline unsigned xchg_32(void *ptr, unsigned x)
{
	__asm__ __volatile__("xchgl %0,%1"
						 :"=r" ((unsigned) x)
						 :"m" (*(volatile unsigned *)ptr), "0" (x)
						 :"memory");

	return x;
}

static inline unsigned short xchg_16(void *ptr, unsigned short x)
{
	__asm__ __volatile__("xchgw %0,%1"
						 :"=r" ((unsigned short) x)
						 :"m" (*(volatile unsigned short *)ptr), "0" (x)
						 :"memory");

	return x;
}

/* Test and set a bit */
static inline char atomic_bitsetandtest(void *ptr, int x)
{
	char out;
	__asm__ __volatile__("lock; bts %2,%1\n"
						 "sbb %0,%0\n"
						 :"=r" (out), "=m" (*(volatile long long *)ptr)
						 :"Ir" (x)
						 :"memory");

	return out;
}

#endif
