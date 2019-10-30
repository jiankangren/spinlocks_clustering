#!/bin/bash

FIRST_CORE=12
LAST_CORE=23

for num_res in 1
do
	for num_req in 256
	do
		for i in {1..900}
		do
			python taskgen_wlocks.py ${FIRST_CORE} ${LAST_CORE} ${num_res} ${num_req} mod
		done
	done
done

echo "Finished!"