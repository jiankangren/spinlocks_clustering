#!/bin/bash

PROC_NUM=12
TOTAL_UTIL=0.625

for num_res in 1
do
	for num_req in 128 256 384 512 640 768 896 1024
	do
		path='ecrts_data/core='${PROC_NUM}'util='${TOTAL_UTIL}'resources='${num_res}'requests='${num_req}'type=mod'
		for i in {1..100}
		do
			file=${path}'/taskset'${i}'.rtpt'
			./scheduler ${file} fifo
			./scheduler ${file} prio
		done
	done
done

echo "Finished!"