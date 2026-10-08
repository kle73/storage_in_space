# Storage in Space – ns-3 simulation

Simulates distributed storage in a LEO satellite constellation: data objects
are kept "in flight" by circulating them as packets over the inter-satellite
links (ISLs). Satellite positions come from TLE files; every satellite runs a
`SatelliteForwardingApp` with a pluggable **routing** and **content** strategy.
The simulation runs on ns-3.45 and is parallelised with MPI (ranks per orbit).

The repository is self-contained: it holds a complete ns-3.45 source tree with
the project code already in place. Clone, configure, build, run – apart from
the system packages listed under Build, nothing has to be downloaded or copied.

## Repository structure

```
.
├── README.md
├── LICENSE                                 GPL-2.0 (license of the project code)
├── .gitignore
└── ns-allinone-3.45/
    ├── LICENSE, MANIFEST.md                ns-allinone-3.45 release files (MANIFEST: origin of the bundled contrib modules)
    └── ns-3.45/                            ns-3 source tree – build and run from here
        ├── run_iridium.sh                  run scripts (start here)
        ├── run_iridium_experiment.sh
        ├── run_starlink.sh
        ├── scratch/
        │   ├── storage_in_space.cc         main program: arguments, TLEs, nodes, ISLs, apps
        │   └── satellite-simulation-data/  tle-iridium.txt, tle-starlink.txt
        ├── contrib/
        │   ├── satellite_network/          this project's ns-3 module
        │   │   ├── CMakeLists.txt          list of the module's source files
        │   │   └── model/
        │   │       ├── satellite-forwarding-app.*   per-satellite app: queues, packet records, statistics, output files
        │   │       ├── strategy-interfaces.h        base classes RoutingStrategy / ContentStrategy
        │   │       ├── routing/                     ring-switch-walker-star, ring-switch-walker-delta
        │   │       ├── content/                     fill-double
        │   │       ├── ring-switch-scheduler.*      computes and fires the ring switches
        │   │       ├── constellation-config.*       initial ring rows and seam satellites per constellation
        │   │       └── point-to-point-laser-*       laser ISL device / channel / helper (from Hypatia, device modified)
        │   └── nr, lorawan, oran, ...      modules bundled with ns-allinone-3.45 (not used by this project)
        ├── src/
        │   ├── satellite/                  SGP4 orbit module (satellite.h, SatellitePositionMobilityModel), from Hypatia
        │   ├── mpi/                        ns-3 MPI module with changes for moving satellites (see below)
        │   └── ...                         unmodified ns-3.45 modules
        └── mysim_results/                  output folder (default of --outDir); only the empty skeleton is versioned
```

Everything else in `ns-3.45/` is unmodified ns-3.45. Changes to ns-3 itself:

- `src/satellite/` is added (SGP4 satellite mobility by Pedro Silva, INESC TEC,
  as included in Hypatia; GPL-2.0).
- `src/mpi/`: the null-message synchronisation recomputes its lookahead every
  100 µs from the current satellite distances instead of using the fixed
  initial link delay (`null-message-simulator-impl.*`, `remote-channel-bundle.*`),
  reports packets that arrive in the past as `[MPI-CAUSAL-VIOLATION]`
  (`null-message-mpi-interface.cc`) and links `mobility` (`CMakeLists.txt`).
- `contrib/.gitignore` no longer ignores the contrib modules (git only).

## Build

Requirements (Linux): C++20 compiler (GCC ≥ 10 or Clang), CMake, Ninja or
Make, Python 3, MPI (e.g. OpenMPI). Optional, as on the original setup: GSL,
Eigen3, SQLite. On Ubuntu:

```
sudo apt install g++ cmake ninja-build python3 openmpi-bin libopenmpi-dev \
                 libgsl-dev libeigen3-dev libsqlite3-dev
```

Configure and build with the configuration the results were produced with
(build profile `default`, MPI on):

```
cd ns-allinone-3.45/ns-3.45
./ns3 configure --enable-mpi --enable-examples --enable-tests
./ns3 build
```

`--enable-examples --enable-tests` only add ns-3's own examples and tests and
can be left out. A full build compiles all bundled modules and takes a while;
`./ns3 configure --enable-mpi --enable-modules=satellite_network` builds only
what this project needs (the simulation itself is the same).

## Run

With a run script (MPI, from `ns-3.45/`):

```
./run_iridium.sh 2                # 2 ranks per orbit -> 6 x 2 = 12 processes
```

The script calls `./ns3 run scratch/storage_in_space` with
`mpiexec -np <orbits x ranks per orbit>`, writes the console output to
`std_out.txt` in the output folder and kills the run above `RAM_LIMIT_GB`
(200). The scripts locate ns-3 from their own location (keep them in
`ns-3.45/`); folders can be overridden with environment variables:

| Variable | Default |
|---|---|
| `NS3_DIR` | folder of the script |
| `TLE_DIR` | `$NS3_DIR/scratch/satellite-simulation-data` |
| `OUT_DIR` | `$NS3_DIR/mysim_results` (passed as `--outDir`) |

```
OUT_DIR=/data/iridium_run3 ./run_iridium.sh 2
```

| Script | Setup |
|---|---|
| `run_iridium.sh` | Iridium (6 orbits), walker-star |
| `run_iridium_experiment.sh` | same, loops over `--runNumber` (`for i in ...`) |
| `run_starlink.sh` | Starlink shell (36 orbits), walker-delta, loops over `--runNumber` |

With MPI, the number of processes must be exactly `orbits x numRanksPerOrbit`
(Iridium: 6 x n, Starlink: 36 x n); the run scripts take care of this. With
more processes than CPU cores, OpenMPI needs
`export OMPI_MCA_rmaps_base_oversubscribe=1`.

Single process (e.g. for debugging or short tests), from `ns-3.45/`:

```
./ns3 run "scratch/storage_in_space --constellation=iridium --simDur=1"
```

All arguments below can be added inside the quotes.

## Arguments (`storage_in_space`)

| Argument | Default | Meaning |
|---|---|---|
| `--constellation` | `iridium` | `iridium`, `starlink`; selects `tle-<name>.txt` and the entry in `constellation-config.cc` |
| `--tleDir` | `scratch/satellite-simulation-data` | folder containing the TLE files (relative to the run folder or absolute) |
| `--outDir` | `mysim_results` | output folder, relative to the run folder or absolute (printed at start) |
| `--routingAlgorithm` | `ring-switch-walker-star` | `ring-switch-walker-star` (Iridium) or `ring-switch-walker-delta` (Starlink) |
| `--contentGeneration` | `fill-double` | `fill-double` (only option) |
| `--simDur` | `40` | simulated time [s] |
| `--islQueue` | `4000` | ISL queue size [packets] |
| `--islRate` | `80000` | ISL data rate [Mbps] |
| `--inclination` | `86.4` | orbit inclination [deg] |
| `--maxQueueFillLevel` | `1` | queue fill limit in % of `--islQueue` |
| `--trafficShare` | `2` | TDMA slots per period for the data (storage) queue of each laser device. Slots of the other queue stay idle when it has nothing to send, so storage traffic gets `trafficShare/(trafficShare+trafficShareBroadcast)` of `--islRate` |
| `--trafficShareBroadcast` | `1` | TDMA slots per period for the broadcast queue; `0` gives the whole link to the data queue (only for runs without broadcast traffic) |
| `--numRanksPerOrbit` | `2` | MPI ranks per orbit |
| `--nullmsg` | `true` | MPI synchronisation: null-message (`true`) or distributed (`false`) |
| `--runNumber` | `0` | appended to the output file names (`...S<runNumber>.csv`) |
| `--forceStatic` | `false` | keep satellites at their initial positions |
| `--useBackpressure`, `--statistics` | | currently unused |

## Output (below `--outDir`)

| File | Content |
|---|---|
| `queue_stats/experiment4/queue_statisticsS<run>.csv` | queue lengths per satellite and direction |
| `packet_stats/experiment4/packet_statisticsS<run>.txt` | per satellite: own packets alive, packets created, mean RTT of the interval (time between two returns of the same copy; 10.0 = none yet), dropped, missing |
| `packet_stats/experiment4/packet_reassembleS<run>.csv` | object reassembly events |
| `packet_stats/packet_monitoring_dataFix.csv` | unroutable packets |
| `packet_stats/spt_tree.csv` | shortest-path tree (walker-delta only) |
| `flow_analysis/experiment3/flow_dataS<run>.csv` | per-link in/out/drop counters |
| `broadcast/experiment4/broadcast_statsS<run>.csv` | broadcast reception times |
| `object_duplication/experiment4/obj_injectS<run>.csv`, `obj_dupS<run>.csv` | object insertion / second copies |
| `content/content_stats.csv`, `debug/debug_out.csv` | content and debug log |
| `positions/position_data.csv`, `positions/isl_connections.csv` | positions, ISL topology |

Missing folders are created automatically. `std_out.txt` (console output) is
written there by the run scripts, not by the program. Result files are not
versioned (`.gitignore`).

## Compile-time switches

Paths relative to `contrib/satellite_network/model/`. Change, then
`./ns3 build`.

| Where | Switches |
|---|---|
| `satellite-forwarding-app.h` | `MONITORE_GENERAL` (queue/packet stats), `MONITORE_POSITIONS`, `MONITORE_PACKET` (per-hop log, slow), `MONITORE_FLOW_BALANCE`, `QUEUE_BUFFER`, `QUEUE_BUFFER_RING` |
| `content/content-fill-double.h` | `STORAGE_FILL_PHASE_END_S`, `FREEZE_AFTER_FIRST_DOWN_SWITCH`, `TIME_TO_LIVE`, `GENERATION_START_TIME`, `SWITCH_SAFETY_TIMEOUT` |
| `routing/routing-ring-switch-walker-star.cc` | `DOWN_QUEUE_REGULATION`, `STORAGE_FILL_PHASE_END_S` (keep equal to the content one) |

## Extending

- **New routing or content strategy:** derive from `RoutingStrategy` /
  `ContentStrategy` (`model/strategy-interfaces.h`; `Init`, `OnReceive` /
  `Generate`), map its name in `SatelliteForwardingApp::SetupWithDevices`
  (`model/satellite-forwarding-app.cc`) and in the name check in
  `scratch/storage_in_space.cc`, include its header in
  `model/satellite-forwarding-app.h` and add the files to
  `contrib/satellite_network/CMakeLists.txt`. Write output files via
  `SatelliteForwardingApp::OutputPath("folder/file.csv")`.
- **New constellation:** add `tle-<name>.txt` to
  `scratch/satellite-simulation-data/` (first line
  `<orbits> <satellites per orbit>`, then the TLEs grouped by orbit and sorted
  by mean anomaly; node id = orbit x satellites per orbit + position) and an
  entry in `MakeConstellationConfig` (`model/constellation-config.cc`) with the
  initial ring rows and seam satellites.

More on the ring-switch routing: the overview comment at the top of
`model/routing/routing-ring-switch-walker-star.cc`.

## Acknowledgements

The satellite mobility (`src/satellite`, SGP4) and the laser inter-satellite
links (`point-to-point-laser-*` in `contrib/satellite_network/model/`) are
taken from the ns-3 satellite network simulator of Hypatia
([snkas/hypatia](https://github.com/snkas/hypatia), `ns3-sat-sim`, GPL-2.0).
The laser net device is a modified version: it has two transmit queues (data
and broadcast) served by a fixed TDMA slot grid (`TrafficShare`,
`TrafficShareBroadcast`, see `point-to-point-laser-net-device.h`). 
If you use this code, please also cite the Hypatia paper:

> S. Kassing, D. Bhattacherjee, A. B. Águas, J. E. Saethre, A. Singla.
> Exploring the "Internet from space" with Hypatia. ACM Internet Measurement
> Conference (IMC), 2020.

```bibtex
@inproceedings {hypatia,
    author = {Kassing, Simon and Bhattacherjee, Debopam and Águas, André
    Baptista and Saethre, Jens Eirik and Singla, Ankit},
    title = {{Exploring the "Internet from space" with Hypatia}},
    booktitle = {{ACM IMC}},
    year = {2020}
}
```

## License

The project code – `contrib/satellite_network/`, `scratch/storage_in_space.cc`,
the run scripts and the changes in `src/mpi/` – is licensed under the GNU
General Public License version 2 only (GPL-2.0-only), the same license as
ns-3; see [`LICENSE`](LICENSE).

Third-party code included in this repository:

- ns-3.45 (`ns-allinone-3.45/ns-3.45`): GPL-2.0-only, see `ns-3.45/LICENSE`.
- `src/satellite`: INESC TEC (Pedro Silva), GPL-2.0, SGP4 implementation by
  D. Vallado; taken from Hypatia, see the file headers.
- `contrib/satellite_network/model/point-to-point-laser-*`: from Hypatia
  (André Baptista Águas, 2020), based on ns-3's point-to-point module;
  GPL-2.0, see the file headers. The net device was modified for this
  project (two transmit queues, TDMA traffic shaping).
- Contrib modules bundled with ns-allinone-3.45 (`nr`, `lorawan`, `oran`, ...):
  their own licenses in their folders; origin in `ns-allinone-3.45/MANIFEST.md`.
