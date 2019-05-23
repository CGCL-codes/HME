export PATH=/home/chendi/pmu-tools:$PATH

#numactl --physcpubind=12 insmod /home/luhaodi/NVMSimulate/Mult_coreNVM/NVM_emulate_bandwidth/uncore.ko
kill -s 9 `ps -ef | grep perf | awk '{print $2}'`


rmmod core_NVM.ko
numactl --physcpubind=38 --membind=0 insmod core_NVM.ko

numactl --physcpubind=16 --membind=0 ocperf.py stat -e offcore_response.demand_rfo.llc_miss.remote_dram,offcore_response.all_reads.llc_miss.remote_dram -I 100 -a --cpu=0,2,4,6,8,10,12,14 --per-core > /dev/shm/ReadWrite.log 2>&1 &

numactl --physcpubind=18 --membind=0 ocperf.py stat -e unc_h_requests.writes_remote -I 100 -a --cpu=0,2,4,6,8,10,12,14 --per-core > /dev/shm/unc_h.log 2>&1 &


numactl --physcpubind=36 --membind=0 python delay_count.py 8

