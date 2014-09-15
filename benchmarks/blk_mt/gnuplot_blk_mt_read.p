set terminal png size 1000,500
date=system("date +%F_%H-%M-%S")
filename='benchmark_pmemblk_read_'.date.'.png'
set output filename
set autoscale
set datafile separator ';'
unset log
unset label
set grid
set xtic auto
set size ratio 0.5
set ytic auto
set title "PMEMBLK mode read operations thread scaling"
set xlabel "Threads"
set ylabel "Operations per second"
set key inside right bottom
plot "benchmark_mt_pmemblk.out" using ($0+1):4 title "pmemblk" with linespoints, \
"benchmark_mt_fileio.out" using ($0+1):4 title "fileio" with linespoints
