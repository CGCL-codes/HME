#!/bin/bash

_PREFIX=$(dirname $(pwd))
#_PREFIX="/home/ZHduan/NVM-best_edition/"

CORE_PMU_MODULE=${_PREFIX}/core.ko
UNCORE_PMU_MODULE=${_PREFIX}/uncore.ko

CORE_IOCTL=/proc/core_pmu
UNCORE_IOCTL=/proc/uncore_pmu
EMULATER_IOCTL=/proc/emulate_nvm

INSTALL_MOD="sudo numactl --physcpubind=12 --membind=1 insmod"
REMOVE_MOD="sudo rmmod uncore"
USE_MOD_NVM="numactl --physcpubind=0 --membind=1"
USE_MOD_INTERLEAVE="numactl --physcpubind=0 --interleave=all"
USE_MOD_LIB="numactl --physcpubind=0 --membind=0"

INPUT_COMMAND=$@
#echo $INPUT_COMMAND

#echo $bw > /proc/uncore_pmu

Start()
{
    #${INSTALL_MOD} ${CORE_PMU_MODULE}
	${INSTALL_MOD} ${UNCORE_PMU_MODULE}
    echo "Emulation starts"
}

INIT()
{
FILENAME=${_PREFIX}/scripts/nvmini.in
declare -A dict
while read LINE
do
    if [[ ${LINE} =~ "=" ]]
    then
        var1=`echo ${LINE// /}|awk -F '=' '{print $1}'`
        var2=`echo ${LINE// /}|awk -F '=' '{print $2}'`
        dict+=([$var1]="$var2")
    fi
done < $FILENAME


NVM_READ_LAT=${dict["NVM_Read_latency_ns"]}
NVM_WRITE_LAT=${dict["NVM_Write_latency_ns"]}
DRAM_READ_LAT=${dict["DRAM_Read_latency_ns"]}
DRAM_WRITE_LAT=${dict["DRAM_Write_latency_ns"]}
NVM_BW=${dict["NVM_bw_ratio"]}
EPOCH_DURATION_US=${dict["epoch_duration_us"]}
TYPE=${dict["type"]}
NVM_READCOST=${dict["NVM_read_w"]}
NVM_WRITECOST=${dict["NVM_write_w"]}
DRAM_READCOST=${dict["DRAM_read_w"]}
DRAM_WRITECOST=${dict["DRAM_write_w"]}

TAG=","
echo $NVM_BW > $UNCORE_IOCTL

echo $DRAM_READ_LAT$TAG$DRAM_WRITE_LAT$TAG$NVM_READ_LAT$TAG$NVM_WRITE_LAT$TAG$EPOCH_DURATION_US$TAG$NVM_READCOST$TAG$NVM_WRITECOST$TAG$DRAM_READCOST$TAG$DRAM_WRITECOST$TAG > $EMULATER_IOCTL

}

USE()
{
    if [ "$TYPE" -eq "1" ]
    then
        ${USE_MOD_NVM} ${INPUT_COMMAND}
    else
        if [ "$TYPE" -eq "2" ]
        then
            ${USE_MOD_INTERLEAVE} ${INPUT_COMMAND}
        else
            ${USE_MOD_LIB} ${INPUT_COMMAND}
        fi
    fi
}

End()
{
    #${REMOVE_MOD} ${CORE_PMU_MODULE}
    #${REMOVE_MOD} ${UNCORE_PMU_MODULE}
    ${REMOVE_MOD}
    echo "Emulation ends"
}

Reset()
{
    ${REMOVE_MOD}
    echo "Reset online modules"
}

CHECK=`lsmod | grep -c uncore`
TEMP="0"
if [ "$CHECK" -ne "$TEMP" ]
then
    Reset
fi

Start
INIT
USE
