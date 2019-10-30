#!/bin/bash

FIRST_CORE=12
LAST_CORE=23
TOTAL_UTIL=0.5

for num_res in 1
do
	for i in {36..70}
	do
		python taskgen_wlocks_base.py ${FIRST_CORE} ${LAST_CORE} ${num_res} mod ${TOTAL_UTIL}
	done
done

echo "Finished!"