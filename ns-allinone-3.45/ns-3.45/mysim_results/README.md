# mysim_results

Default output folder of `storage_in_space` (`--outDir`, relative to the folder
the simulation runs in, normally `ns-3.45/`). Only this skeleton is under
version control; all result files are ignored (see `.gitignore`).
Missing folders are also created automatically at start-up.

| Folder | Files (`<run>` = `--runNumber`) |
|---|---|
| `queue_stats/experiment4/` | `queue_statisticsS<run>.csv` |
| `packet_stats/experiment4/` | `packet_statisticsS<run>.txt`, `packet_reassembleS<run>.csv` |
| `packet_stats/` | `packet_monitoring_dataFix.csv`, `spt_tree.csv` (walker-delta, `--broadcastAlgorithm=spt`) |
| `broadcast/experiment4/` | `broadcast_statsS<run>.csv` |
| `object_duplication/experiment4/` | `obj_injectS<run>.csv`, `obj_dupS<run>.csv` |
| `positions/` | `isl_connections.csv` |
| (root) | `std_out.txt` (console output, written by the run scripts) |

The file contents are described in the main `README.md` (section Output).
