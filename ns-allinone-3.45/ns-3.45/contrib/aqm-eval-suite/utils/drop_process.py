#!/usr/bin/env python3
"""
AQM Evaluation Suite - Drop Processing Utility

This script processes packet drop data from AQM evaluation simulations and generates
cumulative distribution function (CDF) plots showing the time intervals between
packet drops for different flows. It analyzes drop patterns to understand the
fairness and behavior of queue disciplines.

Usage:
    python drop_process.py <scenario_name> <queuedisc_name> <base_output_dir>

Arguments:
    scenario_name   : Name of the simulation scenario
    queuedisc_name  : Name of the queue discipline
    base_output_dir : Base directory containing simulation output data

Input file format:
    - Reads from: {base_output_dir}/{scenario_name}/data/{queuedisc_name}-drop.dat
    - Expected format: "flow_id drop_time" per line

Output:
    - Creates processed data: {base_output_dir}/{scenario_name}/data/new-{queuedisc_name}-drop.dat
    - Creates gnuplot script: {base_output_dir}/{scenario_name}/data/{queuedisc_name}-gnu-drop
    - Generates PNG graph: {base_output_dir}/{scenario_name}/graph/{queuedisc_name}-drop.png

The script calculates:
    1. Time differences between consecutive drops for each flow
    2. CDF of inter-drop times using histogram analysis
    3. Multi-flow comparison plots with smooth curves

Example:
    python drop_process.py mild-congestion Red /path/to/output
    
    This will analyze drop patterns for all flows in the Red simulation
    and generate a CDF plot comparing drop intervals across flows.
"""

import os
import sys

import numpy as np

if len(sys.argv) < 4:
    print("Usage: python drop_process.py <scenario_name> <queuedisc_name> <base_output_dir>")
    sys.exit(1)

scenario_name = sys.argv[1]
queuedisc_name = sys.argv[2]
base_output_dir = sys.argv[3]
file_name = base_output_dir + "/" + scenario_name + "/data/" + queuedisc_name + "-drop.dat"
new_file_name = base_output_dir + "/" + scenario_name + "/data/new-" + queuedisc_name + "-drop.dat"
File = open(file_name, "r")
lines_read = File.readlines()
lines_read.sort()
File.close()
i = 0
data = []
inst_data = []
bucket = []
for file_iterator in lines_read:
    if i == len(lines_read) - 1:
        break
    i += 1
    if file_iterator.split(" ")[0] == lines_read[i].split(" ")[0]:
        temp_string = float(file_iterator.split(" ")[1])
        data.append(temp_string)
    else:
        data.sort()
        data_iterator = 1

        while data_iterator < len(data):
            inst_data.append(data[data_iterator] - data[data_iterator - 1])
            data_iterator += 1
        hist, bin_edges = np.histogram(inst_data, bins=10000, density=True)
        dx = bin_edges[1] - bin_edges[0]
        Function_uno = np.cumsum(hist) * dx
        Function_duo = [[0, 0], [bin_edges[0], 0]]
        for k in range(len(Function_uno)):
            Function_duo.append([bin_edges[k + 1], Function_uno[k]])
        bucket.append(Function_duo)
        data = []

data.sort()
data_iterator = 1
while data_iterator < len(data):
    inst_data.append(data[data_iterator] - data[data_iterator - 1])
    data_iterator += 1
hist, bin_edges = np.histogram(inst_data, bins=10000, density=True)
dx = bin_edges[1] - bin_edges[0]
Function_uno = np.cumsum(hist) * dx
Function_duo = [[0, 0], [bin_edges[0], 0]]
for k in range(len(Function_uno)):
    Function_duo.append([bin_edges[k + 1], Function_uno[k]])
bucket.append(Function_duo)
data = []

gnufile = base_output_dir + "/" + scenario_name + "/data/" + queuedisc_name + "-gnu-drop"
gnu = open(gnufile, "w")

gnu.write("set terminal png size 1260, 800\n")
gnu.write(
    'set output "'
    + base_output_dir
    + "/"
    + scenario_name
    + "/graph/"
    + queuedisc_name
    + '-drop.png"\n set xlabel "Time difference between two drops"\nset ylabel "CDF"\nset grid\nshow grid\n'
)


wfile = open(new_file_name, "w")

flow = 1
for x in bucket:
    wfile.write('\n\n#"flow' + str(flow) + '"\n')
    flow += 1
    for y in x:
        wfile.write(str(y[0]) + " " + str(y[1]) + "\n")
wfile.close()

for data_iterator in range(len(bucket)):
    if data_iterator == 0:
        gnu.write(
            'plot "'
            + new_file_name
            + '" i '
            + str(data_iterator)
            + ' using 1:2 with lines smooth csplines  title "Flow '
            + str(data_iterator + 1)
            + '"'
        )
    else:
        gnu.write(
            ', "'
            + new_file_name
            + '" i '
            + str(data_iterator)
            + ' using 1:2 with lines smooth csplines title "Flow '
            + str(data_iterator + 1)
            + '"'
        )

gnu.close()
os.system("gnuplot " + gnufile)
