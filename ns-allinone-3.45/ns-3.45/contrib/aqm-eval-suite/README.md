# AQM Evaluation Suite

> Automated framework for evaluating Active Queue Management (AQM) algorithms in ns-3

## Overview

The AQM Evaluation Suite provides a comprehensive, automated framework for evaluating and comparing Active Queue Management algorithms in ns-3. It implements standardized evaluation scenarios from RFC 7928 and delivers systematic performance analysis through automated simulation setup, topology creation, traffic generation, execution, results collection, and visualization.

## Featuress

- **RFC 7928 Compliance** - Implements standardized AQM evaluation scenarios
- **Automated Pipeline** - Complete workflow from setup to visualization
- **Multi-AQM Support** - Simultaneous evaluation of 11+ queue disciplines
- **Rich Metrics** - Queue delay, goodput, throughput, and drop statistics
- **Visual Analytics** - Automatic ellipse plots and time-series graphs
- **Extensible Design** - Easy integration of custom AQM algorithms

## Prerequisites

- **ns-3** - Network simulator framework
- **CMake** - Build system
- **Python 3** - For metrics processing and plotting
- **gnuplot-qt** - For graph generation
- **ImageMagick 7** - For image processing

### System Dependencies (Ubuntu/Debian)

```bash
sudo apt update
sudo apt install gnuplot-qt imagemagick
```

### Python Dependencies

```bash
pip3 install numpy
```

## Quick Start

### Build

```bash
# From ns-3 root directory
./ns3 configure --enable-examples
./ns3 build
```

### Run Single Scenario

```bash
# By RFC section number
./ns3 run "aqm-eval-suite-runner --number=5.2"

# By scenario name
./ns3 run "aqm-eval-suite-runner --name=AggressiveTransportSender"

# Run all scenarios
./ns3 run "aqm-eval-suite-runner --name=All"
```

### Run with Custom Parameters

```bash
./ns3 run "aqm-eval-suite-runner --name=MildCongestion --ecn=true --isBql=true"
```

## Available Scenarios

| RFC Section | Scenario Name | Description |
|-------------|---------------|-------------|
| 5.1.1 | TCPFriendlySameInitCwnd | TCP flows with identical initial congestion windows |
| 5.1.2 | TCPFriendlyDifferentInitCwnd | TCP flows with varying initial congestion windows |
| 5.2 | AggressiveTransportSender | Single aggressive TCP flow (CUBIC variant) |
| 5.3.1 | UnresponsiveTransport | Single UDP flow without congestion control |
| 5.3.2 | UnresponsiveWithFriendly | UDP flow competing with TCP traffic |
| 8.2.1 | MildCongestion | Light network congestion scenario |
| 8.2.2 | MediumCongestion | Moderate network congestion scenario |
| 8.2.3 | HeavyCongestion | Heavy network congestion scenario |

## Configuration Options

| Parameter | Description | Default |
|-----------|-------------|---------|
| `--number` | Run scenario by RFC section number | - |
| `--name` | Run scenario by name or "All" | - |
| `--QueueDiscMode` | Queue discipline mode (PACKETS/BYTES) | PACKETS |
| `--isBql` | Enable Byte Queue Limits | false |
| `--ecn` | Enable Explicit Congestion Notification | false |
| `--BaseOutputDir` | Output directory path | aqm-eval-output |

## Project Structure

```
.
├── CMakeLists.txt              # Build configuration
├── doc/                        # Documentation
├── examples/                   # Example scenarios and main runner
├── helper/                     # Helper classes for suite management
├── model/                      # Core evaluation models
└── utils/                      # Python utilities for data processing
```

## Output

Results are generated in the `aqm-eval-output/` directory:
- **Metrics**: Delay, goodput, throughput, drop statistics
- **Plots**: Time-series graphs and ellipse plots
- **Data**: Raw simulation data in various formats

## Development

### Adding New Scenarios

1. Create scenario file in `examples/`
2. Implement scenario logic using existing patterns
3. Register scenario in `aqm-eval-suite-runner.cc`
4. Update documentation

### Custom AQM Algorithms

1. Implement AQM in ns-3 traffic-control module
2. Add algorithm to suite's queue discipline list
3. Configure evaluation parameters
4. Run comparative analysis

## References

- [RFC 7928](https://tools.ietf.org/html/rfc7928) - Characterization Guidelines for Active Queue Management (AQM)
- [ns-3 Documentation](https://www.nsnam.org/documentation/)
