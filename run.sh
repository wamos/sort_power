sudo -v
WORK_DURATION=$1
DURATION=$((WORK_DURATION+10))
RAPL_DUR=$((WORK_DURATION+11))
THREAD=$2
METHOD=$3 
PERIOD=1
HOST=$(hostname | cut -d'.' -f1) 
unixstamp=$(date +%s)
echo $(date +%s)
if [ ${HOST} = "b09-15" ]; then
	nohup python3 meter_reading.py ${METHOD}_${THREAD}th_${unixstamp} ${DURATION} ${PERIOD} 2>&1 &
else
	nohup sudo ./build/rapl_measure ${RAPL_DUR} > log/rapl_output.txt 2>&1 &
	sleep 1
	if [ ${THREAD} = "4" ] && [ ${METHOD} = "cpu" ]; then
		nohup sar -P 0,1,2,3 1 ${DURATION}  > log/${unixstamp}_cpu_4th.sar 2>&1 &
	elif [ ${THREAD} = "8" ] && [ ${METHOD} = "cpu" ]; then
		nohup sar -P 0,1,2,3,4,5,6,7 1 ${DURATION}  > log/${unixstamp}_cpu_8th.sar 2>&1 &
	elif [ ${THREAD} = "16" ] && [ ${METHOD} = "cpu" ]; then
		nohup sar -P 0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15 1 ${DURATION}  > log/${unixstamp}_cpu_16th.sar 2>&1 &
	elif [ ${THREAD} = "4" ] && [ ${METHOD} = "mem" ]; then
		nohup sar -r 1 ${DURATION}  > log/${unixstamp}_mem_4th.sar 2>&1 &
		nohup sar -P 0,1,2,3 1 ${DURATION}  > log/${unixstamp}_memcpu_4th.sar 2>&1 &
	elif [ ${THREAD} = "8" ] && [ ${METHOD} = "mem" ]; then
		nohup sar -r 1 ${DURATION}  > ${unixstamp}_mem_8th.sar 2>&1 &
		nohup sar -P 0,1,2,3,4,5,6,7 1 ${DURATION}  > log/${unixstamp}_memcpu_8th.sar 2>&1 &
	elif [ ${THREAD} = "16" ] && [ ${METHOD} = "mem" ]; then
		nohup sar -r 1 ${DURATION}  > log/${unixstamp}_mem_16th.sar 2>&1 &
		nohup sar -P 0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15 1 ${DURATION}  > log/${unixstamp}_memcpu_16th.sar 2>&1 &
	else
		echo "illegal arguments, check it!"
	fi
fi
sleep 5
echo $(date +%s)
if [ ${HOST} = "b09-15" ]; then
	echo "other servers start tasks"
else
	#sleep 1
	echo "workload started"
	nohup ./build/rapl_baseline ${METHOD} ${WORK_DURATION} ${THREAD} > log/${unixstamp}_${METHOD}_${THREAD}th.work
fi
sleep 5
echo $(date +%s)

