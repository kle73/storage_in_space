#!/usr/bin/env python3
"""
AQM Evaluation Suite - Ellipse Input Generator

This script processes queue delay and throughput data from AQM evaluation
simulations to generate input data for ellipse plotting. It correlates delay
and throughput measurements over time intervals to create scatter plot data
for delay-throughput analysis.

Usage:
    python generate-ellipseinput.py <scenario_name> <queuedisc_name> <base_output_dir>

Arguments:
    scenario_name   : Name of the simulation scenario
    queuedisc_name  : Name of the queue discipline
    base_output_dir : Base directory containing simulation output data

Input files:
    - Queue delay: {base_output_dir}/{scenario_name}/data/{queuedisc_name}-qdel.dat
    - Throughput:  {base_output_dir}/{scenario_name}/data/{queuedisc_name}-throughput.dat

Output:
    - Result file: {base_output_dir}/{scenario_name}/data/{queuedisc_name}-result.dat
    - Format: "average_delay average_throughput" per 0.1s time interval

Processing method:
    1. Groups delay measurements into 0.1 second time intervals
    2. Calculates average delay for each interval
    3. Calculates average throughput for corresponding intervals
    4. Outputs delay-throughput pairs for scatter plotting

This data is typically used with the ellipsemaker tool to generate
delay vs throughput scatter plots with confidence ellipses.

Example:
    python generate-ellipseinput.py mild-congestion Red /path/to/output

    This will correlate delay and throughput data to create input suitable
    for ellipse-based delay-throughput analysis plots.
"""

import sys

if len(sys.argv) < 4:
    print(
        "Usage: python generate-ellipseinput.py <scenario_name> <queuedisc_name> <base_output_dir>"
    )
    sys.exit(1)

scenario_name = sys.argv[1]
queuedisc_name = sys.argv[2]
base_output_dir = sys.argv[3]

read_qdelay_file = open(
    base_output_dir + "/" + scenario_name + "/data/" + queuedisc_name + "-qdel.dat", "r"
)
read_throughput_file = open(
    base_output_dir + "/" + scenario_name + "/data/" + queuedisc_name + "-throughput.dat", "r"
)
write_file = open(
    base_output_dir + "/" + scenario_name + "/data/" + queuedisc_name + "-result.dat", "w"
)
qdelay_lines = read_qdelay_file.readlines()
throughput_lines = read_throughput_file.readlines()
last_time_interval = 0.0
qdelay_result = 0.0
qdelay_count = 0
throughput_index = 0
throughput_val = 0
throughput_count = 0
for l in qdelay_lines:
    s = l.split(" ")
    qdelay_time = float(s[0])
    qdelay_val = float(s[1])
    curr_time_interval = ((int)(qdelay_time * 10)) / 10.0
    if last_time_interval == curr_time_interval:
        qdelay_result += qdelay_val
        qdelay_count += 1
    else:
        if not qdelay_count == 0:
            qdelay_result /= qdelay_count
            while throughput_index < len(throughput_lines):
                throughput_time = float((throughput_lines[throughput_index].split(" "))[0])
                if throughput_time < curr_time_interval:
                    throughput_val += float((throughput_lines[throughput_index].split(" "))[1])
                    throughput_count += 1
                    throughput_index += 1
                else:
                    break
            if not throughput_count == 0:
                throughput_result = throughput_val / throughput_count
            else:
                throughput_result = 0
            write_file.write(str(qdelay_result) + " " + str(throughput_result) + "\n")
            throughput_val = 0
            throughput_count = 0
            qdelay_result = 0
            qdelay_count = 0
        last_time_interval = curr_time_interval

if not qdelay_count == 0:
    qdelay_result /= qdelay_count
    while throughput_index < len(throughput_lines):
        throughput_val += float((throughput_lines[throughput_index].split(" "))[1])
        throughput_count += 1
        throughput_index += 1
    if not throughput_count == 0:
        throughput_result = throughput_val / throughput_count
    else:
        throughput_result = 0
    write_file.write(str(qdelay_result) + " " + str(throughput_result) + "\n")
    throughput_val = 0
    throughput_count = 0
    qdelay_result = 0
    qdelay_count = 0
    last_time_interval = curr_time_interval

read_qdelay_file.close()
read_throughput_file.close()
write_file.close()
