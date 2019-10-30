#!/bin/bash

PROC_NUM=36
NUM_RESOURCES=1
TOTAL_UTIL=0.5

for num_req in 128 256 384 512 640 768 896 1024
do
	path='ecrts_data_base/core='${PROC_NUM}'util='${TOTAL_UTIL}'resources='${NUM_RESOURCES}'requests='${num_req}'type=mod'
	
	for i in {1..100}
	do
		output_folder=${path}'/taskset'${i}'_output'

		fifo_stat=${output_folder}'/stat_fifo'
		prio_stat=${output_folder}'/stat_prio'

		gzip ${fifo_stat}
		gzip ${prio_stat}
	done
done

echo "Compression finished!"