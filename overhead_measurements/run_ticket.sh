#!/bin/bash

NUM_THREADS=$1

# Run 20 times
# Default number of threads is 8
for i in {1..20}
do 
	./ticket_contended $NUM_THREADS
	echo
done