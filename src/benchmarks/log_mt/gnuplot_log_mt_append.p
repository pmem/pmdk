set terminal png size 1000,500
date=system("date +%F_%H-%M-%S")
filename='benchmark_log_append_'.date.'.png'
set output filename
set autoscale
set datafile separator ';'
unset log
unset label
set grid
set xtic auto
set size ratio 0.5
set ytic auto
set title "pmemlog thread scaling"
set xlabel "Threads count"
set ylabel "Vector appends per second"
set key inside right bottom
plot "pmemlog_mt.out" using ($0+1):2 title "pmemlog" with linespoints, \
"fileiolog_mt.out" using ($0+1):2 title "fileiolog" with linespoints
