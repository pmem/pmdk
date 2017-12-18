$LOG = "out${Env:UNITTEST_NUM}.log"
$LOG_TEMP = "out${Env:UNITTEST_NUM}_part.log"
rm $LOG -Force -ea si
touch $LOG
rm $LOG_TEMP -Force -ea si
touch $LOG_TEMP

$LAYOUT = "OBJ_LAYOUT${Env:SUFFIX}"
$POOLSET = "$DIR\pool.set"

# Known compat flags:

# Known incompat flags:
$POOL_FEAT_NOHDRS = 0x0001

# Unknown compat flags:
$UNKNOWN_COMPAT = 1, 2, 3, 1111

# Unknown incompat flags:
$UNKNOWN_INCOMPAT = 2, 3, 1111

# set compat flags in header
function set_compat {
    Param (
        [parameter(Position=0)]
        [String]$part,
        [parameter(Position=1)]
        [String]$flag

    )
	expect_normal_exit $PMEMSPOIL $part "pool_hdr.compat_features=$flag" `
		"pool_hdr.checksum_gen()"
}

# set incompat flags in header
function set_incompat {
    Param (
        [parameter(Position=0)]
        [String]$part,
        [parameter(Position=1)]
        [String]$flag

    )
	expect_normal_exit $PMEMSPOIL $part "pool_hdr.incompat_features=$flag" `
		"pool_hdr.checksum_gen()"
}
