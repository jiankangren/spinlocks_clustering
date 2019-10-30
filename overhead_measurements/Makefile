all: contended

contended: ticket_cont mcs_cont prio_mcs_cont prio_ticket_cont prio_queue_cont
uncontended: ticket_unc mcs_unc prio_mcs_unc prio_ticket_unc


# Rules for contended benchmarks
ticket_cont: bench_ticket_contended.c ticket_locks.c
	gcc -O0 bench_ticket_contended.c ticket_locks.c -o ticket_cont -lrt -lpthread

mcs_cont: bench_mcs_contended.c mcs_locks.c
	gcc -O0 bench_mcs_contended.c mcs_locks.c -o mcs_cont -lrt -lpthread

prio_ticket_cont: bench_prio_ticket_contended.c priority_ticket_locks.c ticket_locks.c
	gcc -O0 bench_prio_ticket_contended.c priority_ticket_locks.c ticket_locks.c -o prio_ticket_cont -lrt -lpthread

prio_mcs_cont: bench_prio_mcs_contended.c priority_mcs_locks.c mcs_locks.c
	gcc -O0 bench_prio_mcs_contended.c priority_mcs_locks.c mcs_locks.c -o prio_mcs_cont -lrt -lpthread

prio_queue_cont: bench_prioqueuelock_contended.c priority_queue_locks.c mcs_locks.c
	gcc -O0 bench_prioqueuelock_contended.c priority_queue_locks.c mcs_locks.c -o prio_queue_cont -lrt -lpthread

#Rules for uncontended benchmarks
ticket_unc: bench_ticket_uncontended.c ticket_locks.c
	gcc -O0 bench_ticket_uncontended.c ticket_locks.c -o ticket_unc -lrt

mcs_unc: bench_mcs_uncontended.c mcs_locks.c
	gcc -O0 bench_mcs_uncontended.c mcs_locks.c -o mcs_unc -lrt

prio_ticket_unc: bench_prio_ticket_uncontended.c priority_ticket_locks.c ticket_locks.c
	gcc -O0 bench_prio_ticket_uncontended.c priority_ticket_locks.c ticket_locks.c -o prio_ticket_unc -lrt

prio_mcs_unc: bench_prio_mcs_uncontended.c priority_mcs_locks.c mcs_locks.c
	gcc -O0 bench_prio_mcs_uncontended.c priority_mcs_locks.c mcs_locks.c -o prio_mcs_unc -lrt

clean: 
	rm ticket_cont mcs_cont prio_ticket_cont prio_mcs_cont prio_queue_cont \
		ticket_unc mcs_unc prio_ticket_unc prio_mcs_unc