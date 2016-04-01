import collections

header = "total-avg;ops-per-second;total-max;total-min;total-median;total-std-dev;latency-avg;latency-min;latency-max;latency-std-dev;threads;ops-per-thread;data-size;seed;repeats;type;seed;max-key;external-tx\n"
dictionary = collections.defaultdict(lambda: str(header))
for i in range(1, 501):
	with open("bench_map" + str(i) + ".log", "r") as inputFile:
		lines = inputFile.readlines()
		for line in lines[2:]:
			split = line.split(";")
			name = split[15]
			dictionary[name] += line
			print (name)
			print (line)


for key, value in dictionary.items():
	with open("all_" + key + ".csv", "w+") as outFile:
		outFile.write(key + "\n" + value)
