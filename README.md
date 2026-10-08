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
        │   │       ├── satellite-forwarding-app.*   per-satellite app: queues, storage levels, packet records, statistics, output files
        │   │       ├── strategy-interfaces.h        base classes RoutingStrategy / ContentStrategy, packet types
        │   │       ├── routing/                     ring-switch-walker-star, ring-switch-walker-delta (incl. broadcast algorithms)
        │   │       ├── content/                     fill-double (object generation for storage and broadcast)
        │   │       ├── ring-switch-scheduler.*      computes and starts the ring switches
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
| `--contentGeneration` | `fill-double` | `fill-double` (storage: two copies per object) or `broadcast` (every object is sent to all satellites) |
| `--broadcastAlgorithm` | `prune` | broadcast algorithm of `ring-switch-walker-delta`: `prune` (flooding with reverse-path pruning), `flood`, `dim-order` (dimension-ordered spanning tree), `spt` (delay-optimal shortest-path trees); walker-star always floods |
| `--simDur` | `40` | simulated time [s] |
| `--islQueue` | `4000` | ISL queue size [packets] |
| `--islRate` | `80000` | ISL data rate [Mbps] |
| `--maxQueueFillLevel` | `1` | fill level of the storage queues in % of `--islQueue` |
| `--objectSize` | `10` | packets per object |
| `--ttl` | `10000` | time to live of stored objects [s]; expired copies are replaced or deleted by their origin |
| `--trafficShare` | `2` | TDMA slots per period for the data (storage) queue of each laser device. Slots of the other queue stay idle when it has nothing to send, so storage traffic gets `trafficShare/(trafficShare+trafficShareBroadcast)` of `--islRate` |
| `--trafficShareBroadcast` | `1` | TDMA slots per period for the broadcast queue; `0` gives the whole link to the data queue (only for runs without broadcast traffic) |
| `--numRanksPerOrbit` | `2` | MPI ranks per orbit |
| `--nullmsg` | `true` | MPI synchronisation: null-message (`true`) or distributed (`false`) |
| `--runNumber` | `0` | appended to the output file names (`...S<runNumber>.csv`) |
| `--forceStatic` | `false` | keep satellites at their initial positions |
| `--useBackpressure`, `--inclination`, `--statistics` | | accepted for older run scripts, not used |

## Output (below `--outDir`)

| File | Content |
|---|---|
| `queue_stats/experiment4/queue_statisticsS<run>.csv` | queue lengths per satellite and direction |
| `packet_stats/experiment4/packet_statisticsS<run>.txt` | per satellite (no header): node, time, own packets in the system, packet ids created, mean RTT of the interval (time between two passes of the same copy; 10.0 = none yet), dropped (cumulative), missing (no copy seen for 6.2 s and for twice the longest RTT), copies that arrived after their record was evicted |
| `packet_stats/experiment4/packet_reassembleS<run>.csv` | a flagged single copy returned to its origin |
| `packet_stats/packet_monitoring_dataFix.csv` | unroutable packets (node, last hop, time, code) |
| `packet_stats/spt_tree.csv` | tree of satellite 0 after every rebuild (walker-delta, `--broadcastAlgorithm=spt`) |
| `broadcast/experiment4/broadcast_statsS<run>.csv` | send and reception times of a 1 % sample of the broadcasts |
| `object_duplication/experiment4/obj_injectS<run>.csv`, `obj_dupS<run>.csv` | object insertions (mode 0: both copies, 1: UP copy only, 2: DOWN copy only) / second copies |
| `positions/isl_connections.csv` | ISL topology |

Missing folders are created automatically. `std_out.txt` (console output) is
written there by the run scripts, not by the program. Result files are not
versioned (`.gitignore`).

## Storage mechanism (overview)

Every object is stored as an UP and a DOWN copy that circulate in the storage
rings. Each satellite has a level for its up queue and its down queue: the
level becomes active the first time the queue is full, and from then on the
routing tops the queue up to its level with dummy packets (they only add
queueing delay and are dropped by the next satellite). New objects are stored
whenever the levels admit them and no ring switch is near; a copy whose time to
live (`--ttl`) has expired is replaced by a copy of a waiting object, or
deleted, by its origin. The stored amount therefore stays constant once all
levels are active. Details: class comment of `SatelliteForwardingApp` and the
overview comments at the top of the two routing files.

There are no compile-time switches. Tuning constants (queue headroom, switch
guard, statistics interval, ...) are named constants at the top of the source
files in `contrib/satellite_network/model/`.

## Extending

- **New routing or content strategy:** derive from `RoutingStrategy` /
  `ContentStrategy` (`model/strategy-interfaces.h`), map its name in
  `SatelliteForwardingApp::Setup` and include its header
  (`model/satellite-forwarding-app.cc`), extend the name check in
  `scratch/storage_in_space.cc` and add the files to
  `contrib/satellite_network/CMakeLists.txt`. Write output files via
  `SatelliteForwardingApp::OutputPath("folder/file.csv")`.
- **New broadcast algorithm (walker-delta):** derive from
  `DeltaBroadcastAlgorithm` (`model/routing/routing-ring-switch-walker-delta.h`)
  in `routing-ring-switch-walker-delta.cc` and add one line with its name to
  `kBroadcastAlgorithms` there; `--broadcastAlgorithm=<name>` then selects it.
- **New constellation:** add `tle-<name>.txt` to
  `scratch/satellite-simulation-data/` (first line
  `<orbits> <satellites per orbit>`, then the TLEs grouped by orbit and sorted
  by mean anomaly; node id = orbit x satellites per orbit + position) and an
  entry in `MakeConstellationConfig` (`model/constellation-config.cc`) with the
  initial ring rows and seam satellites.

More on the ring-switch routing: the overview comments at the top of
`model/routing/routing-ring-switch-walker-star.cc` and
`model/routing/routing-ring-switch-walker-delta.cc`.

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
