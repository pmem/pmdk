# standard unit test setup

$Env:UNITTEST_NAME = "util_poolset_parse\TEST0"
$Env:UNITTEST_NUM = "0"

. ..\unittest\unittest.ps1



truncate -s 1M C:\test\sparse1

truncate -s 10M C:\test\sparse1

truncate -s 3M C:\test\sparse1