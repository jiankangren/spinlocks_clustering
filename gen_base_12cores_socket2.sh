#!/bin/bash

FIRST_CORE=24
LAST_CORE=35
TOTAL_UTIL=0.5

for num_res in 1
do
	for i in {71..100}
	do
		python taskgen_wlocks_base.py ${FIRST_CORE} ${LAST_CORE} ${num_res} mod ${TOTAL_UTIL}
	done
done

echo "Finished!"