#!/bin/bash

FIRST_CORE=0
LAST_CORE=11
TOTAL_UTIL=0.5

for num_res in 1
do
	for i in {1..35}
	do
		python taskgen_wlocks_base.py ${FIRST_CORE} ${LAST_CORE} ${num_res} mod ${TOTAL_UTIL}
	done
done

echo "Finished!"