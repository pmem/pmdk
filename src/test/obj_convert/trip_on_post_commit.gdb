set width 0
set height 0
set verbose off
set confirm off
set breakpoint pending on

b tx_post_commit if trap == 1
run
quit
