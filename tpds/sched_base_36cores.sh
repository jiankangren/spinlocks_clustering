#!/bin/bash

PROC_NUM=36
TOTAL_UTIL=0.75

for num_res in 1
do
	for num_req in 128 256 384 512 640 768 896 1024
	do
		path='data_tpds/core='${PROC_NUM}'n=8util='${TOTAL_UTIL}'resources='${num_res}'requests='${num_req}'type=mod'
		for i in {0..3}
		do
			for j in {1..25}
			do
				let "tset_num = $i * 25 + $j"
				file=${path}'/taskset'${tset_num}'.rtpt'
				taskset -c ${j} ../scheduler ${file} fifo &
				taskset -c ${j} ../scheduler ${file} prio dm &
		   		taskset -c ${j} ../scheduler ${file} prio opt &
			done
		done
		wait
		#for i in {101..101}
		#do
		#	file=${path}'/taskset'${i}'.rtpt'
		#	../scheduler ${file} fifo 
		#	../scheduler ${file} prio dm 
		#	../scheduler ${file} prio opt 
		#done
	done
done

echo "Finished!"