This tool is based on NUMA. And in this version just consider 2 nodes in NUMA.

Brief Description:

	This tool uses pmu-tools to periodically get the counts of read/write of every core.
  
	And than useing these data to calculate the delta of performance between DRAM and NVM.
  
	At last, we achieve NVM's simulation by making core busy wait.
  

Details of this tool:

    run.sh is used to start this tool

    core_NVM.c  is used to realize the driver core_NVM.ko which is used to receive the performance delta 
	of every core and send these information to every core
	
	delay_count.py is used to calculate the performance delta of every core

    NVM_emulate_bandwidth is used to control the nvm bandwith
