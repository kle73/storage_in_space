#!/bin/bash

# Check if argument is provided
if [ -z "$1" ]; then
    echo "Usage: $0 <numRanksPerOrbit>"
    exit 1
fi

# Validate that the argument is a positive integer
if ! [[ "$1" =~ ^[0-9]+$ ]] || [ "$1" -le 0 ]; then
    echo "Error: Argument must be a positive integer."
    exit 1
fi

NUM_RANKS_PER_ORBIT=$1
NP=$(( NUM_RANKS_PER_ORBIT * 36 )) 

# Paths (no absolute paths needed; the script lives in ns-3.45/):
#   NS3_DIR  ns-3 folder        default: folder of this script
#   OUT_DIR  output folder      default: $NS3_DIR/mysim_results (passed as --outDir)
#   TLE_DIR  TLE files          default: $NS3_DIR/scratch/satellite-simulation-data
# Override e.g.: OUT_DIR=/data/results ./run_starlink.sh 2   (paths without spaces)
NS3_DIR="${NS3_DIR:-$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)}"
OUT_DIR="${OUT_DIR:-$NS3_DIR/mysim_results}"
TLE_DIR="${TLE_DIR:-$NS3_DIR/scratch/satellite-simulation-data}"
RAM_LIMIT_GB=200
RAM_LIMIT_KB=$(( RAM_LIMIT_GB * 1024 * 1024 ))  # Convert GB to KB

# Trap Ctrl+C and termination signals
trap 'echo "Interrupted! Killing all simulation processes...";
      pkill -TERM -P "$SIM_PID" 2>/dev/null;
      kill -TERM "$SIM_PID" 2>/dev/null;
      pkill -TERM -f "mpiexec.*storage_in_space" 2>/dev/null;
      pkill -TERM -f "storage_in_space" 2>/dev/null;
      sleep 3;
      pkill -KILL -f "mpiexec.*storage_in_space" 2>/dev/null;
      pkill -KILL -f "storage_in_space" 2>/dev/null;
      exit 1' SIGINT SIGTERM

echo "Starting simulation with numRanksPerOrbit=$NUM_RANKS_PER_ORBIT and -np=$NP"
echo "RAM limit: ${RAM_LIMIT_GB}GB"

# Check the folders and make them absolute (the simulation runs in $NS3_DIR)
NS3_DIR="$(cd "$NS3_DIR" 2>/dev/null && pwd)"
[ -x "$NS3_DIR/ns3" ] || { echo "Error: no ns3 script in '$NS3_DIR' - put this script into ns-3.45/ or set NS3_DIR"; exit 1; }
TLE_DIR="$(cd "$TLE_DIR" 2>/dev/null && pwd)" || { echo "Error: TLE folder not found - set TLE_DIR"; exit 1; }
mkdir -p "$OUT_DIR" || { echo "Error: cannot create output folder $OUT_DIR"; exit 1; }
OUT_DIR="$(cd "$OUT_DIR" && pwd)"
OUTPUT_FILE="$OUT_DIR/std_out.txt"
echo "ns-3: $NS3_DIR   TLEs: $TLE_DIR   output: $OUT_DIR"

# Run the simulation in the background
cd "$NS3_DIR" || { echo "Error: Could not cd to $NS3_DIR"; exit 1; }

for i in 20
do
    ./ns3 run scratch/storage_in_space \
        --command-template="mpiexec -np $NP %s \
        --tleDir=$TLE_DIR \
        --outDir=$OUT_DIR \
        --constellation=starlink\
        --islQueue=4000 \
        --routingAlgorithm=ring-switch-walker-delta\
        --contentGeneration=fill-double\
        --maxQueueFillLevel=100\
        --inclination=70 \
        --simDur=10000\
        --useBackpressure=false \
        --trafficShare=1\
        --runNumber=$i\
        --numRanksPerOrbit=$NUM_RANKS_PER_ORBIT" \
        > "$OUTPUT_FILE" 2>&1 &

    SIM_PID=$!
    echo "Simulation started with PID: $SIM_PID"

    # Monitor RAM usage
    while true; do
        # Check if the simulation process is still running
        if ! kill -0 "$SIM_PID" 2>/dev/null; then
            echo "Simulation process has finished."
            break
        fi

        # Sum up RSS (resident set size in KB) for all mpiexec/ns3 related processes
        TOTAL_RAM_KB=$(ps aux --no-headers | awk '
            /mpiexec|ns3|storage_in_space/ && !/awk/ {sum += $6}
            END {print sum}
        ')

        TOTAL_RAM_GB=$(awk "BEGIN {printf \"%.2f\", $TOTAL_RAM_KB / 1024 / 1024}")
        echo "Current RAM usage: ${TOTAL_RAM_GB}GB / ${RAM_LIMIT_GB}GB"

        if [ "$TOTAL_RAM_KB" -ge "$RAM_LIMIT_KB" ]; then
            echo "WARNING: RAM usage exceeded ${RAM_LIMIT_GB}GB (currently ${TOTAL_RAM_GB}GB). Killing all simulation processes..."

            # Kill the main process and all its children
            pkill -TERM -P "$SIM_PID" 2>/dev/null
            kill -TERM "$SIM_PID" 2>/dev/null

            # Also kill any remaining mpiexec processes from this run
            pkill -TERM -f "mpiexec.*storage_in_space" 2>/dev/null
            pkill -TERM -f "storage_in_space" 2>/dev/null

            sleep 3

            # Force kill if still running
            pkill -KILL -P "$SIM_PID" 2>/dev/null
            kill -KILL "$SIM_PID" 2>/dev/null
            pkill -KILL -f "mpiexec.*storage_in_space" 2>/dev/null
            pkill -KILL -f "storage_in_space" 2>/dev/null

            echo "All simulation processes killed due to RAM limit exceeded."
            exit 1
        fi

        sleep 5  # Check every 5 seconds
    done


    echo "Simulation completed successfully."
done
exit 0