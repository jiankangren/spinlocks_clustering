#!/bin/bash

PROC_NUM=36
NUM_RESOURCES=1
TOTAL_UTIL=0.75
NUM_TASKS=8

for num_req in 128 256 384 512 640 768 896 1024
do
	echo "======= Number of request: "${num_req}
	path='data_tpds/core='${PROC_NUM}'n='${NUM_TASKS}'util='${TOTAL_UTIL}'resources='${NUM_RESOURCES}'requests='${num_req}'type=mod'
	
	for i in {1..100}
	do
		fifo_rtps=${path}'/taskset'${i}'_fifo.rtps'
		dm_prio_rtps=${path}'/taskset'${i}'_prio_dm.rtps'
		opt_prio_rtps=${path}'/taskset'${i}'_prio_opt.rtps'
		../inspector ${fifo_rtps}
		../inspector ${dm_prio_rtps}
		../inspector ${opt_prio_rtps}
	done
done

echo "Checking finished!"