#!/usr/bin/env python3
"""
AQM Evaluation Suite - Goodput Processing Utility

This script processes goodput (effective throughput) data from AQM evaluation 
simulations and generates goodput over time plots. It calculates instantaneous
goodput rates for different flows and creates comparative visualizations.

Usage:
    python goodput_process.py <scenario_name> <queuedisc_name> <base_output_dir>

Arguments:
    scenario_name   : Name of the simulation scenario
    queuedisc_name  : Name of the queue discipline
    base_output_dir : Base directory containing simulation output data

Input file format:
    - Reads from: {base_output_dir}/{scenario_name}/data/{queuedisc_name}-goodput.dat
    - Expected format: "flow_id timestamp bytes_received" per line

Output:
    - Creates processed data: {base_output_dir}/{scenario_name}/data/new-{queuedisc_name}-goodput.dat
    - Creates gnuplot script: {base_output_dir}/{scenario_name}/data/{queuedisc_name}-gnu-goodput
    - Generates PNG graph: {base_output_dir}/{scenario_name}/graph/{queuedisc_name}-goodput.png

The script calculates:
    1. Instantaneous goodput rates by aggregating data in 0.1s windows
    2. Conversion from bytes/sec to Mbps (divides by 1024*128)
    3. Flow-specific goodput traces over time
    4. Special handling for RTT Fairness scenarios with appropriate labeling

Example:
    python goodput_process.py mild-congestion Red /path/to/output
    
    This will process goodput data for all flows and generate a time-series
    plot showing goodput evolution for each flow.
"""

import os
import sys

if len(sys.argv) < 4:
    print("Usage: python goodput_process.py <scenario_name> <queuedisc_name> <base_output_dir>")
    sys.exit(1)

scenario_name = sys.argv[1]
queuedisc_name = sys.argv[2]
base_output_dir = sys.argv[3]
file_name = base_output_dir + "/" + scenario_name + "/data/" + queuedisc_name + "-goodput.dat"
new_file_name = (
    base_output_dir + "/" + scenario_name + "/data/new-" + queuedisc_name + "-goodput.dat"
)
File = open(file_name, "r")
lines_read = File.readlines()
lines_read.sort()
File.close()
i = 0
data = []
File = []
inst_data = []
for file_iterator in lines_read:
    if i == len(lines_read) - 1:
        break
    i += 1
    if file_iterator.split(" ")[0] == lines_read[i].split(" ")[0]:
        string_uno = float(file_iterator.split(" ")[1])
        string_duo = int((file_iterator.split(" ")[2]).replace("\n", ""))
        data.append([string_uno, string_duo])
    else:
        data.sort()
        goodput_val = 0
        goodput_count = 0
        last_recorded = 0
        for a in data:
            if a[0] - last_recorded < 0.1:
                goodput_val += a[1]
                goodput_count += 1
            else:
                last_recorded = a[0]
                if goodput_count > 0:
                    inst_data.append([a[0], (goodput_val * 1.0 / a[0])])
        last_recorded = a[0]
        if goodput_count > 0:
            inst_data.append([a[0], (goodput_val * 1.0 / a[0])])
        data = []
        File.append(inst_data)
        inst_data = []

data.sort()
goodput_val = 0
goodput_count = 0
last_recorded = 0
for a in data:
    if a[0] - last_recorded < 0.1:
        goodput_val += a[1]
        goodput_count += 1
    else:
        last_recorded = a[0]
        if goodput_count > 0:
            inst_data.append([float("{0:.2f}".format(a[0])), (goodput_val * 1.0 / a[0])])
last_recorded = a[0]
if goodput_count > 0:
    inst_data.append([float("{0:.2f}".format(a[0])), (goodput_val * 1.0 / a[0])])
File.append(inst_data)

st = ""

gnufile = base_output_dir + "/" + scenario_name + "/data/" + queuedisc_name + "-gnu-goodput"
gnu = open(gnufile, "w")

gnu.write("set terminal png\n")
gnu.write(
    'set output "'
    + base_output_dir
    + "/"
    + scenario_name
    + "/graph/"
    + queuedisc_name
    + '-goodput.png"\n set xlabel "Time (Seconds)" font "Verdana,12"\nset ylabel "Goodput (Mbps)" font "Verdana,12"\nset grid\nshow grid\nset key font "Verdana,12"\n'
)


wfile = open(new_file_name, "w")

flow = 1
for x in File:
    wfile.write('\n\n#"flow' + str(flow) + '"\n')
    flow += 1
    for y in x:
        wfile.write(str(y[0]) + " " + str(y[1] / (1024 * 128)) + "\n")
wfile.close()

if "RttFairness" in scenario_name:
    for j in range(len(File)):
        if j == 0:
            gnu.write(
                'plot "'
                + new_file_name
                + '" i '
                + str(j)
                + ' using 1:2 with lines smooth csplines title "Fixed Rtt Flow"'
            )
        else:
            gnu.write(
                ', "'
                + new_file_name
                + '" i '
                + str(j)
                + ' using 1:2 with lines smooth csplines title "Variable Rtt Flow"'
            )
else:
    for j in range(len(File)):
        if j == 0:
            gnu.write(
                'plot "'
                + new_file_name
                + '" i '
                + str(j)
                + ' using 1:2 with lines smooth csplines title "Flow '
                + str(j + 1)
                + '"'
            )
        else:
            gnu.write(
                ', "'
                + new_file_name
                + '" i '
                + str(j)
                + ' using 1:2 with lines smooth csplines title "Flow '
                + str(j + 1)
                + '"'
            )

gnu.close()
os.system("gnuplot " + gnufile)
