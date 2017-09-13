export PATH=/home/liurens/pmu-tools:$PATH

numactl --physcpubind=12 insmod /home/liurens/Mult_coreNVM/NVM_emulate_bandwidth/uncore.ko

numactl --physcpubind=9 --membind=0 insmod core_NVM.ko

numactl --physcpubind=8 --membind=0 ocperf.py stat -e offcore_response.demand_rfo.llc_miss.remote_dram,offcore_response.all_reads.llc_miss.remote_dram -I 100 -a --cpu=0,1 --per-core > /dev/shm/ReadWrite.log 2>&1 &

numactl --physcpubind=7 --membind=0 ocperf.py stat -e unc_h_requests.writes_remote -I 100 -a --cpu=12 > /dev/shm/unc_h.log 2>&1 &


numactl --physcpubind=6 --membind=0 python delay_count.py 2

