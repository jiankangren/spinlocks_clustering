#!/bin/bash

PROC_NUM=12
TOTAL_UTIL=0.75

for num_req in 128
do
	for num_res in 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16
	do
		path='ecrts_data_base/varying_num_requests/core='${PROC_NUM}'util='${TOTAL_UTIL}'resources='${num_res}'requests='${num_req}'type=mod'
		for i in {1..100}
		do
			file=${path}'/taskset'${i}'.rtpt'
			./scheduler ${file} fifo
			./scheduler ${file} prio
		done
	done
done

echo "Finished!"