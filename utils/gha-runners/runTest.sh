echo "run test pmdk"

FULL_PATH=$(readlink -f .)
PMDK_0_PATH=$(dirname $FULL_PATH)
PMDK_PATH=${PMDK_0_PATH}/pmdk
echo "Fullpath"
echo ${PMDK_PATH}

source ${PMDK_PATH}/utils/gha-runners/common.sh

local OPTIND optchar opt_args
while getopts ':hdmpe-:' optchar; do
	case "$optchar" in
		-)
		case "$OPTARG" in
            test-type*) TEST_TYPE="${OPTARG#*=}" ;;
            test-build*) TEST_BUILD="${OPTARG#*=}" ;;
            drd*) DRD="${OPTARG#*=}" ;;
            memcheck*) MEM_CHECK="${OPTARG#*=}" ;;
            pmemcheck*) PMEM_CHECK="${OPTARG#*=}" ;;
            helgrind*) HEL_GRIND="${OPTARG#*=}" ;;
            fs-type*) FS_TYPE="${OPTARG#*=}" ;;
            test-folders=*) TEST_FOLDERS="${OPTARG#*=}" ;;
		esac
		;;
	esac
done

run_unittests --test-type=$TEST_TYPE --test-build=$TEST_BUILD --drd=$DRD --memcheck=$MEM_CHECK --pmemcheck=$PMEM_CHECK --helgrind=$HEL_GRIND --fs-type=$FS_TYPE --test-folders=$TEST_FOLDERS --pmdk-path=${PMDK_PATH} --test-libs=''
