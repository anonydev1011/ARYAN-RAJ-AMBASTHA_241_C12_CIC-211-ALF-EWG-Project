# CIC-211 Option 1 — ALF-EWG

Autonomous Landslide & Flash-Flood Early Warning Grid implemented in standard C++17.

## Build and run

```sh
make
./alf_ewg
```

Or compile directly:

```sh
g++ -std=c++17 -Wall -Wextra -pedantic -Iinclude main.cpp src/Sensors.cpp src/Alerts.cpp -o alf_ewg
```

The simulation writes `alf_ewg_incidents.bin` in its working directory. It persists a committed incident, appends a deliberately corrupt pending incident, reads it with `seekg`, detects the checksum mismatch, resets the write cursor with `seekp`, and truncates the uncommitted tail to restore the previous file boundary.

## Layout and requirement map

- `include/DisasterCore.h`: `DisasterCore::Sensors` and `DisasterCore::Alerts`; inline hazard-index overloads; coordinate transformation by reference; `SensorStation`, auditor friendship, `TelemetryBuffer`, the virtual-inheritance device diamond, exceptions, siren, route functor, manipulators, and incident logger declarations.
- `include/TelemetryBuffer.h`: focused include for telemetry buffer users.
- `src/Sensors.cpp`: station lifetime accounting, stream operators, cluster aggregation, deep-copy buffer ownership, and probe telemetry/polymorphism implementation.
- `src/Alerts.cpp`: network alert state, formatting manipulators, and binary transactional rollback.
- `main.cpp`: executable demonstration of Units I–IV, including the sweep through `std::vector<BaseSensorNode*>`.

The station `<<` and `>>` operators use a whitespace-delimited round-trip format: `id region latitude longitude rainfall slope soilSaturation vibration`. The simulation demonstrates both string-stream and file-stream extraction.
