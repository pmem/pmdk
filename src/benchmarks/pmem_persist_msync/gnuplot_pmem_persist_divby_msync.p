set terminal png size 1000,500
date=system("date +%F_%H-%M-%S")
filename='benchmark_pmem_persist_divby_msync'.date.'.png'
set output filename
set datafile separator ';'
unset log
unset label
set logscale xy 2
set grid
set xtic auto
set size ratio 0.5
set ytic auto
set title "Ratio of execution times (pmem_persist / pmem_msync)"
set xlabel "Size of flushed data [bytes]"
set ylabel "Ratio of execution times"
set key inside left top
plot "pmem_persist_msync.out" using 5:6 title "(pmem_persist / pmem_msync)" with linespoints
