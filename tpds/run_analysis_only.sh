#!/bin/bash

PROC_NUM=12
TOTAL_UTIL=0.75

for num_res in 1
do
	for num_tasks in 5
	do
		for num_req in 128 256 384 512 640 768 896 1024
		do
			path='data_analysis_tpds/core='${PROC_NUM}'n='${num_tasks}'util='${TOTAL_UTIL}'resources='${num_res}'requests='${num_req}'type=short'
			for i in {0..4}
			do
				for k in {0..4}
				do
					for j in {1..40}
					do
						let "tset_num = $i * 200 + $k * 40 + $j"
						file=${path}'/taskset'${tset_num}'.rtpt'
						taskset -c ${j} ../analysis_only_sched ${file} fifo &
						taskset -c ${j} ../analysis_only_sched ${file} prio dm &
						taskset -c ${j} ../analysis_only_sched ${file} prio opt &
						taskset -c ${j} ../analysis_only_sched ${file} prio sim &
					done
				done
				wait
			done
		done
	done
done

echo "Finished!"