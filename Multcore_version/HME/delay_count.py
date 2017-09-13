import time;  
import re; 
import sys;
import datetime
  
fpath_r_RW = "/dev/shm/ReadWrite.log"
fpath_r_EV =  "/dev/shm/unc_h.log"
fpath_w = "/sys/kernel/kobject_NVM/nvm"
fsyn = "/dev/shm/syncount.log"

#core start with 0
coreNUM = int(sys.argv[1])

fp_r_rw = open(fpath_r_RW, 'r')
fp_r_ev = open(fpath_r_EV, 'r')
fp_w = open(fpath_w, 'w',1)
fp_wsyn = open(fsyn, 'w',1)

writeCount = [0,0,0,0,0,0,0,0,0,0]
readCount = [0,0,0,0,0,0,0,0,0,0]
delay = [0,0,0,0,0,0,0,0,0,0]
eviction=0
through_count = 0

# Count declaration
WDELAY = 13
RDELAY = 2
PRECISION = 10000
syncount = 0

  
if not fp_r_rw: 
	print "rw_fail open."  
	exit(1);
if not fp_r_ev:
	print "ev_fail open."  
	exit(1);
if not fp_w:
	print "w_fail open."
	exit(1);

#get read and write count
def get_rw():
	global through_count
	through_count = 0
	coreOnLine = coreNUM
	while(coreOnLine > 0):
		count = 2
		while (count > 0):
			line_r = fp_r_rw.readline()
			if line_r.endswith("dram\n") or line_r.endswith(")\n")is True:
				#print line_r
				line_list = re.split('\s+', line_r)
				core_number = (coreNUM - coreOnLine)
				if count == 2:
					writeCount[core_number] = int(line_list[4].replace(',',''))
					through_count = through_count + writeCount[core_number]
				else:
					readCount[core_number] = int(line_list[4].replace(',',''))
				count = count -1
				continue
			elif line_r.endswith('\n') is not True:
				fp_r_rw.seek((fp_r_rw.tell() - len(line_r)),0)
		coreOnLine = coreOnLine -1
	#print writeCount
	#print readCount

#get eviction count 
def get_evc():
	global eviction
	eviction = 0
	count = 2
	while count > 0:
		line_r = fp_r_ev.readline()
		if line_r.endswith("remote_0\n") is True:
			line_list = re.split('\s+', line_r)
			eviction = int(line_list[2].replace(',',''))
			#print eviction
			count = count - 1
			continue
		elif line_r.endswith("remote_1\n") is True:
			count = count - 1
			continue
		elif line_r.endswith('\n') is not True:
			fp_r_ev.seek((fp_r_ev.tell() - len(line_r)),0)
	#print eviction


#count delay
def count_delay():
	dirty_eviction = eviction - through_count
	#print eviction
	#print through_count
	#print dirty_eviction
	#print readCount
	#print writeCount

	for i in range(coreNUM):
		if dirty_eviction >0 :
			delay[i] =  readCount[i] * RDELAY + ((writeCount[i] * WDELAY + dirty_eviction * WDELAY / coreNUM))
		else:
			delay[i] = readCount[i] * RDELAY
		#print delay[i]
		delay[i] = int(round( delay[i],-4)/PRECISION)
		#print delay[i]
	

#send operation
def sendop():
	for i in range(coreNUM):
		operate = str(i)+str(delay[i])+'\n'
		#print "operate:  "+operate
		fp_w.write(operate)



while True:
	#starttime = datetime.datetime.now()
	#get read and write count
	get_rw()
	#get eviction
	get_evc()
	#count delay
	count_delay()
	#send operation
	sendop()
	syncount = syncount + 0.1
	syn = str(syncount)+'\n'
	#print syn
	fp_wsyn.write(syn)
	#endtime = datetime.datetime.now()
	#print endtime - starttime
	#time.sleep(0.1)
