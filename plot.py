import numpy as np
import matplotlib.pyplot as plt
import sys
from argparse import ArgumentParser


def scale(a):
    return a / 1000000.0


def parse_throughput(filename):
    times = []
    pktsize = []

    throughput_file = open(filename, "r")
    for line in throughput_file:
        tokens = line.split(",")
        pktsize.append(1450)
        times.append((float(tokens[0])))

    throughput_file.close()
    return times, pktsize


def parse_delay(filename):
    delays = []
    times1 = []
    cnt = 0

    delay_file = open(filename, "r")
    first_delay = float(delay_file.readline().split(",")[0])
    delay_file.seek(0)
    for line in delay_file:
        tokens = line.split(",")
        if len(tokens) < 3:
            break
        if float(tokens[1]) > 0.0:
            delays.append((float(tokens[2])))
            times1.append((float(tokens[0]) - first_delay))
        if cnt == 2000:
            break
        cnt += 1
    delay_file.close()

    return delays, times1


def calc_throughput(data, pktsize):
    values = []
    last_index = 0
    ctr = 0
    w = float(data[0] + float(1))
    for i, v in enumerate(data):
        if w < v:
            values.append(ctr * 8.0 / 1000000.0)
            w = float(w + float(1))
            ctr = 0

        ctr = ctr + pktsize[i]
    return values


parser = ArgumentParser(description="plot")

parser.add_argument('--dir',
                    '-d',
                    help="Directory to store outputs",
                    required=True)

parser.add_argument('--name',
                    '-n',
                    help="name of the experiment",
                    required=True)

parser.add_argument('--trace', '-tr', help="name of the trace", required=True)

parser.add_argument('--window',
                    '-cwnd',
                    help="CSV file record for cwnd",
                    required=True)

args = parser.parse_args()

fig = plt.figure(figsize=(21, 3), facecolor='w')
ax = plt.gca()

# plotting the trace file
f1 = open(args.trace, "r")
BW = []
nextTime = 1000
cnt = 0
for line in f1:
    if int(line.strip()) > nextTime:
        BW.append(cnt * 1492 * 8)
        cnt = 0
        nextTime += 1000
    else:
        cnt += 1
f1.close()

ax.fill_between(range(len(BW)), 0, list(map(scale, BW)), color='#D3D3D3')

# plotting throughput
throughputDL = [float(0)]
timeDL = [float(0)]

traceDL = open(args.dir + "/" + str(args.name) + '_receiver.csv', 'r')
traceDL.readline()

bytes = float(0)
start_time = float(traceDL.readline().strip().split(",")[0])
prev_time = start_time
stime = float(start_time)

for line in traceDL:
    entry = line.strip().split(",")
    time_now = float(entry[0])
    time_elapsed = time_now - start_time
    if time_elapsed <= 1.0:
        bytes += float(entry[1])
    else:
        timeDL.append(prev_time - stime)
        throughputDL.append(bytes * 8 / 1000000.0)
        bytes = 0
        start_time = time_now
    prev_time = time_now

plt.plot(timeDL, throughputDL, lw=2, color='r')
plt.ylabel("Throughput (Mbps)")
plt.xlabel("Time (s)")
plt.xlim([timeDL[0], timeDL[-1]])
plt.grid(True, which="both")
plt.savefig(args.dir + '/throughput.pdf', dpi=1000, bbox_inches='tight')

fig = plt.figure(figsize=(21, 3), facecolor='w')
ax = plt.gca()

# plotting CWND
CWNDDL = []
timeDL = []

traceDL = open(args.window, 'r')
traceDL.readline()

start_time = float(traceDL.readline().strip().split(",")[0])

for line in traceDL:
    entry = line.strip().split(",")
    time_elapsed = float(entry[0]) - start_time
    if (time_elapsed not in timeDL):
        timeDL.append(float(time_elapsed))
        CWNDDL.append(int(entry[-1]))

plt.plot(timeDL, CWNDDL, lw=2, color='r')
plt.ylabel("CWND (packets)")
plt.xlabel("Time (s)")
plt.xlim([timeDL[0], timeDL[-1]])
plt.grid(True, which="both")
plt.savefig(args.dir + '/CWND.pdf', dpi=1000, bbox_inches='tight')
