#!/bin/bash

FIRST_CORE=0
LAST_CORE=11
TOTAL_UTIL=0.75
REQ_PER_RES=128

#for num_res in 1
#do
	for i in {1..35}
	do
		python taskgen_wlocks_base2.py ${FIRST_CORE} ${LAST_CORE} ${REQ_PER_RES} mod ${TOTAL_UTIL}
	done
#done

echo "Finished!"