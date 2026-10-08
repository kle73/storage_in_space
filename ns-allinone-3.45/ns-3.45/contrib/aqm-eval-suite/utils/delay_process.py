#!/usr/bin/env python3
"""
AQM Evaluation Suite - Delay Processing Utility

This script processes queue delay data from AQM evaluation simulations and generates
delay plots using gnuplot. It reads delay data files and creates PNG graphs showing
delay over time for different queue disciplines.

Usage:
    python delay_process.py <scenario_name> <queuedisc_name> <base_output_dir>

Arguments:
    scenario_name   : Name of the simulation scenario
    queuedisc_name  : Name of the queue discipline
    base_output_dir : Base directory containing simulation output data

Input file format:
    - Reads from: {base_output_dir}/{scenario_name}/data/{queuedisc_name}-qdel.dat
    - Expected format: "time_seconds delay_milliseconds" per line

Output:
    - Creates gnuplot script: {base_output_dir}/{scenario_name}/data/{queuedisc_name}-gnu-delay
    - Generates PNG graph: {base_output_dir}/{scenario_name}/graph/{queuedisc_name}-delay.png

Example:
    python delay_process.py mild-congestion Red /path/to/output
    
    This will process delay data for the Red queue discipline in the 
    mild-congestion scenario and generate a delay vs time plot.
"""

import os
import sys

if len(sys.argv) < 4:
    print("Usage: python delay_process.py <scenario_name> <queuedisc_name> <base_output_dir>")
    sys.exit(1)

scenario_name = sys.argv[1]
queuedisc_name = sys.argv[2]
base_output_dir = sys.argv[3]
file_name = base_output_dir + "/" + scenario_name + "/data/" + queuedisc_name + "-qdel.dat"
File = open(file_name, "r")
lines_read = File.readlines()
lines_read.sort()
File.close()

gnufile = base_output_dir + "/" + scenario_name + "/data/" + queuedisc_name + "-gnu-delay"
gnu = open(gnufile, "w")

gnu.write("set terminal png\n")
gnu.write(
    'set output "'
    + base_output_dir
    + "/"
    + scenario_name
    + "/graph/"
    + queuedisc_name
    + '-delay.png"\n set xlabel "Time (Seconds)" font "Verdana,12"\nset ylabel "Delay (ms)" font "Verdana,12"\nset grid\nshow grid\nset key font "Verdana,12"\nset yrange [0:]\n'
)


if "RttFairness" in scenario_name:
    gnu.write("set xrange [:600]\n")
else:
    gnu.write("set xrange [:300]\n")

gnu.write('plot "' + file_name + '" using 1:2 with lines title "' + queuedisc_name + '"')

gnu.close()
os.system("gnuplot " + gnufile)
