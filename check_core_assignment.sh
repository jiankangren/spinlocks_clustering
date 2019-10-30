#!/bin/bash

PROC_NUM=36
NUM_RESOURCES=1
TOTAL_UTIL=0.625

for num_req in 128 256 384 512 640 768 896 1024
do
	echo "======= Number of request: "${num_req}
	path='ecrts_data/core='${PROC_NUM}'util='${TOTAL_UTIL}'resources='${NUM_RESOURCES}'requests='${num_req}'type=mod'
	
	for i in {1..200}
	do
		fifo_rtps=${path}'/taskset'${i}'_fifo.rtps'
		prio_rtps=${path}'/taskset'${i}'_prio.rtps'
		./inspector ${fifo_rtps}
		./inspector ${prio_rtps}

	done
done

echo "Checking finished!"