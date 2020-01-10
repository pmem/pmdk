#!/bin/bash

while [ "$?" -eq "0" ]; do
	./TESTS.py -b debug -f pmem -u 4
	# ./TESTS.py -b debug -f pmem -u 4 --force-enable helgrind -o 1d
done
