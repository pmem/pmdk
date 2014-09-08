set terminal png size 1000,500
set output "benchmark_malloc.png"
set autoscale
set datafile separator ';'
unset log
unset label
set grid
set xtic auto
set size ratio 0.5
set ytic auto
set title "jemalloc library thread scaling"
set xlabel "Threads"
set ylabel "Operations per second"
set key inside right bottom
plot "benchmark_mt_vmem.out" using ($0+1):2 title "vmem" with linespoints, \
"benchmark_mt.out" using ($0+1):2 title "malloc" with linespoints
