#!/bin/bash

modname=/home/syz/Github/NVM/uncore.ko

numactl --physcpubind=6 --membind=1 insmod $modname

for ((bw = 0; bw <= 4; bw += 2)); do
	echo $bw > /proc/uncore_pmu

	dir_name=SPEC_$bw
	if [ ! -d $dir_name ]; then
		mkdir -p $dir_name
	fi

	# GCC BZIP MCF BWAVES MILC
	SPEC_BENCH="403 401 429 410 433"
	spec_flags="--config=mytest.cfg --noreportable --iteration=1"
	for i in ${SPEC_BENCH}; do
		file_name=${PWD}/${dir_name}/${i}_nvm
		numactl --physcpubind=0 --membind=1 runspec ${spec_flags} ${i} > ${file_name}
		cat /proc/uncore_pmu >> ${file_name}
		
		file_name=${PWD}/${dir_name}/${i}_interleave
		numactl --physcpubind=0 --interleave=all runspec ${spec_flags} ${i} > ${file_name}
		cat /proc/uncore_pmu >> ${file_name}
	done
done
