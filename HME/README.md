# NVM-best_edition
A hybrid memory simulator that utilizes the Intel Core Performance Monitoring Unit (PMU) tool to simulate NVM using DRAM.With the numa architecture of the two nodes, each node can only have a CPU online, all other CPU must be offline (temporarily only support single-threaded, such as SPEC test set).CPU_0 on the A node, CPU_6 on the B node is online. CPU_0 is used to simulate NVM, and CPU_6 is only used to assist in the simulation. (All parameters can be set in emulate_nvm.c.)
## System Requirements
The simulator relies on the Xeon E5 / E7 (including the PMU hardware architecture).
Standard C++ compiler, like g++ or icc.
You must also have a base NUMA architecture.
## Compiling
Bind the module to CPU_6. If the settings in emulate_nvm.c do not match the actual binding CPU, the module will report an error and exit automatically. In this case, you need to configure it correctly.
    
    $numactl --physcpubind=6 --membind=1 insmod $modname
    
/proc file, used to control the delay, bandwidth configuration via bash script. Specifically see scripts /tesh.sh or scripts /hybrid-memory.sh.
## Support or Contact
This is developed in the [HUST SCTS&CGCL Lab](http://grid.hust.edu.cn/ "悬停显示"). If you have any questions, please contact Zhuohui Duan(122316931@qq.com), GLHF.
