#!/bin/bash

NVM_READ_SLAT=200
NVM_WRITE_SLAT=400
CPATH=$(pwd)/nvmini.in
READ_DURATION=100
WRITE_DURATION=200
N=3

GET(){
declare -A dict
while read LINE
do
    if [[ ${LINE} =~ "=" ]]
    then
        var1=`echo ${LINE// /}|awk -F '=' '{print $1}'`
        var2=`echo ${LINE// /}|awk -F '=' '{print $2}'`
        dict+=([$var1]="$var2")
    fi
done < $CPATH
NVM_READ_CLAT=${dict["NVM_Read_latency_ns"]}
NVM_WRITE_CLAT=${dict["NVM_Write_latency_ns"]}
#echo $NVM_READ_CLAT
#echo $NVM_WRITE_CLAT
}

SET(){
sed -i "s/NVM_Read_latency_ns = $NVM_READ_CLAT/NVM_Read_latency_ns = $NVM_READ_SLAT/g" $CPATH
sed -i "s/NVM_Write_latency_ns = $NVM_WRITE_CLAT/NVM_Write_latency_ns = $NVM_WRITE_SLAT/g" $CPATH
}

CHANGE_READ(){
    NVM_READ_SLAT=$(($NVM_READ_SLAT+$READ_DURATION))
}

CHANGE_WRITE(){
    NVM_WRITE_SLAT=$(($NVM_WRITE_SLAT+$WRITE_DURATION))
}

TEST(){
    dir_name=TEST_RESULT
    if [ ! -d $dir_name   ]; then
        mkdir -p $dir_name
    else
	rm -rf $dir_name
	mkdir -p $dir_name
    fi

    for ((i = 0; i <= $N; i += 1)); do
        echo "Current execution rounds: $i"
        GET
	SET
	GET
        echo -e >> $(pwd)/$dir_name/memtest.log
        echo -e >> $(pwd)/$dir_name/time.txt
        echo "NVM read latency : $NVM_READ_CLAT ns" >> $(pwd)/$dir_name/memtest.log
        echo "NVM read latency = $NVM_READ_CLAT ns" >> $(pwd)/$dir_name/time.txt
        echo "NVM write latency : $NVM_WRITE_CLAT ns" >> $(pwd)/$dir_name/memtest.log
        echo "NVM write latency = $NVM_WRITE_CLAT ns" >> $(pwd)/$dir_name/time.txt
        CHANGE_READ
	echo "Running Memtester to test 1M NVM, config: NVM read latency: $NVM_READ_CLAT ns; NVM write latency:$NVM_WRITE_CLAT ns"
        time (sh runenv.sh memtester -p 0x880000000 500K 1 >> $(pwd)/$dir_name/memtest.log)>>$(pwd)/$dir_name/time.txt 2>&1
    done
}

TEST
