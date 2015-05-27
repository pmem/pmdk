set terminal png size 1000,500
date=system("date +%F_%H-%M-%S")
filename='benchmark_pmem_persist_and_msync'.date.'.png'
set output filename
set datafile separator ';'
unset log
unset label
set logscale xy 2
set grid
set xtic auto
set size ratio 0.5
set ytic auto
set format y "%3.2e"
set title "Execution times of pmem_persist and pmem_msync"
set xlabel "Size of flushed data [bytes]"
set ylabel "Execution time [seconds]"
set key inside left top
plot "pmem_persist_msync.out" using 1:2 title "pmem_persist" with linespoints, \
"pmem_persist_msync.out" using 3:4 title "pmem_msync" with linespoints
