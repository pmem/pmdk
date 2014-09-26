#! /bin/bash

LIB=""
DIR_OPT=""
MALLOCS_COUNT=10000000
MAX_THREADS=32
SMALL=512
VMEM_OUT=benchmark_mt_vmem_
MALLOC_OUT=benchmark_mt_
PROCESSORS=`nproc`
LD_PRELOAD_BAK=$LD_PRELOAD
RUN_NUMA=0
RUN_CUT=0

# usage info
show_help() {
cat << EOF
Usage: ${0##*/} [-hnp] [-l LIBRARY] [-t THREADS] [-d DIR_PATH]
Run the vmem_mt benchmark in different modes. Do not mix the n and p
flags together. If no flags are used a single run on all CPUs is
performed.

	-h          display this help and exit
	-n          Run with a decreasing number of CPUs using numactl
	-p	    Run with a decreasing number of CPUs for the whole
		    system
	-l LIBRARY  Use this malloc library
	-t THREADS  The max numbers of threads to be used
	-d DIR_PATH Directory where the pools will be created
EOF
}

check_recover_error() {
out=$?
if [ $out != 0 ]
then
	echo Benchmark error threads: $i
	exit 1
	if [ $RUN_CUT -gt 0 ]
	then
		revert_cpus
	fi
fi
}

revert_cpus() {
PROC_OFF=`seq $((PROCESSORS - 1)) -1 1`

# turn the processors back on
for j in $PROC_OFF ; do
	PROC_FILE='/sys/devices/system/cpu/cpu'$j'/online'

	if [ -f "$PROC_FILE" ]
	then
		echo 1 > $PROC_FILE;
	fi
done
}

# run a single benchmark on all CPUs
run_single() {
RUNS=`seq $MAX_THREADS`
rm -rf $VMEM_OUT$PROCESSORS.out
for i in $RUNS ; do
	echo ./vmem_mt -e vmem -s $SMALL $DIR_OPT $i $MALLOCS_COUNT;
	./vmem_mt -e vmem -s $SMALL $i $DIR_OPT $MALLOCS_COUNT >> $VMEM_OUT$PROCESSORS.out;
	check_recover_error
done

export LD_PRELOAD=$LD_PRELOAD:$LIB
rm -rf $MALLOC_OUT$PROCESSORS.out
for i in $RUNS ; do
	echo ./vmem_mt -e malloc -s $SMALL $DIR_OPT $i $MALLOCS_COUNT;
	./vmem_mt -e malloc -s $SMALL $DIR_OPT $i $MALLOCS_COUNT >> $MALLOC_OUT$PROCESSORS.out;
	check_recover_error
done
export LD_PRELOAD=$LD_PRELOAD_BAK

# prepare plot scripts, plot and cleanup
sed -e "s/\USEDPROCESSORS/$PROCESSORS/g" gnuplot_mt_free.template > gnuplot_mt_free_$PROCESSORS.p

sed -e "s/\USEDPROCESSORS/$PROCESSORS/g" gnuplot_mt_malloc.template > gnuplot_mt_malloc_$PROCESSORS.p

gnuplot *.p
rm *.p
}

# run a set of tests with a decreasing number of CPUs using numactl
run_numa() {
PROC_OFF=`seq $((PROCESSORS - 1)) -1 0`
RUNS=`seq $MAX_THREADS`

for j in $PROC_OFF ; do
	rm -rf $VMEM_OUT$((j + 1)).out
	for i in $RUNS ; do
		echo numactl --physcpubind=+0-$j ./vmem_mt -e vmem -s $SMALL $DIR_OPT $i $MALLOCS_COUNT;
		numactl --physcpubind=+0-$j ./vmem_mt -e vmem -s $SMALL $DIR_OPT $i $MALLOCS_COUNT >> $VMEM_OUT$((j + 1)).out;
		check_recover_error
	done

	export LD_PRELOAD=$LD_PRELOAD:$LIB
	rm -rf $MALLOC_OUT$((j + 1)).out
	for i in $RUNS ; do
		echo numactl --physcpubind=+0-$j ./vmem_mt -e malloc -s $SMALL $DIR_OPT $i $MALLOCS_COUNT;
		numactl --physcpubind=+0-$j ./vmem_mt -e malloc -s $SMALL $DIR_OPT $i $MALLOCS_COUNT >> $MALLOC_OUT$((j + 1)).out;
		check_recover_error
	done
	export LD_PRELOAD=$LD_PRELOAD_BAK

	# prepare plot scripts
	sed -e "s/\USEDPROCESSORS/$((j + 1))/g" gnuplot_mt_free.template > gnuplot_mt_free_$((j + 1)).p

	sed -e "s/\USEDPROCESSORS/$((j + 1))/g" gnuplot_mt_malloc.template > gnuplot_mt_malloc_$((j + 1)).p
done

# plot and cleanup
gnuplot *.p
rm *.p
}

# run a set of tests with a decreasing number of CPUs for the whole system
run_cut() {
PROC_OFF=`seq $((PROCESSORS - 1)) -1 1`
RUNS=`seq $MAX_THREADS`

# start cutting off the processors
for j in $PROC_OFF ; do

	PROC_FILE='/sys/devices/system/cpu/cpu'$j'/online'

	if [ -f "$PROC_FILE" ]
	then
		echo "disabling cpu"$j
		echo 0 > $PROC_FILE;
		rm -rf $VMEM_OUT$j.out
		for i in $RUNS ; do
			echo ./vmem_mt -e vmem -s $SMALL $DIR_OPT $i $MALLOCS_COUNT;
			./vmem_mt -e vmem -s $SMALL $DIR_OPT $i $MALLOCS_COUNT >> $VMEM_OUT$j.out;
			check_recover_error
		done

		export LD_PRELOAD=$LD_PRELOAD:$LIB
		rm -rf $MALLOC_OUT$j.out
		for i in $RUNS ; do
			echo ./vmem_mt -e malloc -s $SMALL $DIR_OPT $i $MALLOCS_COUNT;
			./vmem_mt -e malloc -s $SMALL $DIR_OPT $i $MALLOCS_COUNT >> $MALLOC_OUT$j.out;
			check_recover_error
		done
		export LD_PRELOAD=$LD_PRELOAD_BAK

	# prepare plot scripts
	sed -e "s/\USEDPROCESSORS/$j/g" gnuplot_mt_free.template > gnuplot_mt_free_$j.p

	sed -e "s/\USEDPROCESSORS/$j/g" gnuplot_mt_malloc.template > gnuplot_mt_malloc_$j.p
	else
		echo "Cannot dynamically turn a CPU off."
	fi

done

revert_cpus

# plot and cleanup
gnuplot *.p
rm *.p
}

# parse options
OPTIND=1
while getopts "hnpl:t:d:" opt; do
	case "$opt" in
	h)
		show_help
		exit 0
		;;
	n)
		RUN_NUMA=$((RUN_NUMA + 1))
		;;
	p)
		RUN_CUT=$((RUN_CUT + 1))
		;;
	l)
		LIB=$OPTARG
		;;
	t)
		MAX_THREADS=$OPTARG
		;;
	d)
		DIR_OPT='-d '$OPTARG
		;;
	'?')
		show_help >&2
		exit 1
		;;
	esac
done
shift "$((OPTIND - 1))"

if [ $RUN_NUMA -gt 0 ]
then
	run_numa;
elif [ $RUN_CUT -gt 0 ]
then
	run_single;
	run_cut;
else
	run_single;
fi
