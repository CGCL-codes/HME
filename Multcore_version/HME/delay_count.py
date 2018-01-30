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
testw = 0
testr = 0
# Count declaration
WDELAY = 1300
RDELAY = 200
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
	linenum=0
	coreOnLine = coreNUM
	while(coreOnLine > 0):
		count = 2
		while count > 0:
			line_r = fp_r_rw.readline()
			if (not line_r) or (re.search("\S+",line_r) is None):
				break  
			line_r = fp_r_rw.readline()
			if line_r.find("#")>-1:
				continue
			linenum=linenum+1
			#print 'the '+str(linenum)+'line:'+line_r
			if re.findall("\d+.*?\d+.*?\d+.*?offcore_r.*?_dram.*?",line_r).__len__()>0:
				#print line_r
				#line_list = re.split('\s+', line_r)
				#if line_list[4].isdigit() is not True:
				#if line_r.find('#') > -1:
				#		continue
				core_number = (coreNUM - coreOnLine)
				dr = re.findall("(\d+)\s+off.*?remote",line_r)
				if dr.__len__() < 1:
					continue	
				if count == 2:
					testw=writeCount[core_number]=int(re.findall("(\d+)\s+off.*?remote",line_r)[0])
					print "writeCount= %d" %testw
					through_count = through_count + writeCount[core_number]
					count = count-1
				else:
					testr=readCount[core_number] = int(re.findall("(\d+)\s+off.*?remote",line_r)[0])
					print "readCount= %d" %testr
					count = count -1
				continue
			else:
				fp_r_rw.seek((fp_r_rw.tell() - len(line_r)),0)
		coreOnLine = coreOnLine -1
	#print writeCount
	#print readCount

#get eviction count 
def get_evc():
	global eviction
	eviction = 0
	count = 2
	lineNum=0
	while count > 0:
		line_r = fp_r_ev.readline()
		if (not line_r) or (re.search("\S+",line_r) is None):
			#print "the uncfile is empty"
			break
		lineNum = lineNum+1
		#print "the "+str(lineNum)+" line: "+line_r
		if line_r.find('#') > -1:
			continue
		if re.findall("\d+.*?\d+.*?\d+.*?unc_h.*?remote_0.*?",line_r).__len__() > 0:
			#line_list = re.split('\s+', line_r)
			#print "pass:the "+str(lineNum)+" line: "+line_r
			#print line_list
			#break
			#if line_list[2].isdigit() is not True:
			#	continue
			#eviction = int(line_list[2].replace(',',''))
			eviction=int(re.findall("(\d+)\s+un.*?remote_0.*?",line_r)[0])
			#print eviction
		#	count = count - 1
			continue
		elif re.findall("\d+.*?\d+.*?\d+.*?unc_h.*?remote_1.*?",line_r).__len__()>0:
			count = count - 1
			continue
		else:
			fp_r_ev.seek((fp_r_ev.tell() - len(line_r)),0)
			#break
	#print eviction


#count delay
def count_delay():
	#print "eviction=%d,through_cout=%d"%(eviction,through_count)
	dirty_eviction = eviction - through_count
	#print eviction
	#print through_count
	#print dirty_eviction
	#print readCount
	#print writeCount
	print "dirty_eviction=%d"%dirty_eviction
	for i in range(coreNUM):
		if dirty_eviction >0 :
			delay[i] =  readCount[i] * RDELAY + ((writeCount[i] * WDELAY + dirty_eviction * WDELAY / coreNUM))
		else:
			delay[i] = readCount[i] * RDELAY
		print "delay[%d]=%f"%(i,delay[i])
		delay[i] = int(round( delay[i],-4)/PRECISION)
		#print delay[i]
	

#send operation
def sendop():
	for i in range(coreNUM):
		operate = str(i)+str(delay[i])+'\n'
		print "operate:  "+operate
		fp_w.write(operate)



while True:
	#starttime = datetime.datetime.now()
	#get read and write count
	print("********begin get_rw************")
	get_rw()
	#print readCount
	#print writeCount
	print("********begin get_evc*********")
	#get eviction
	get_evc()
	#print("*************************begin count_delay****************************")
	#count delay
	count_delay()
	#send operation
	print delay
	print("***********begin sendop*********************************")
	sendop()
	syncount = syncount + 0.1
	syn = str(syncount)+'\n'
	#print syn
	fp_wsyn.write(syn)
	#endtime = datetime.datetime.now()
	#print endtime - starttime
	time.sleep(1)
