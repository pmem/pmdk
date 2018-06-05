set width 0
set height 0
set verbose off
set confirm off
set breakpoint pending on

b os_fsync
run
quit
