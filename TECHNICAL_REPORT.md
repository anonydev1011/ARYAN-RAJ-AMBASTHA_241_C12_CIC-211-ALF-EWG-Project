# Autonomous Landslide & Flash-Flood Early Warning Grid — ALF-EWG

**Course Code:** CIC-211: Object-Oriented Programming Using C++  
**Institution:** Maharaja Agrasen Institute of Technology (MAIT)  
**Student Name:** Aryan Raj Ambastha  
**Roll Number:** 24114802725  
**Section:** C12  
**Language/Compiler:** Standard C++17, g++

## 1. Executive Summary & Problem Statement

Landslides and flash floods can develop quickly when intense rainfall combines with steep terrain, saturated soil, and ground vibration. ALF-EWG is a small educational simulation of a distributed warning grid. It models stations and probes that derive a bounded hazard index from telemetry, report threshold conditions, and route evacuation notices according to urgency and distance.

The implementation separates public declarations from definitions: `include/DisasterCore.h` declares the domain API in two namespaces; `src/Sensors.cpp` implements station, buffer, and probe behavior; `src/Alerts.cpp` implements alert dispatch and binary incident logging; and `main.cpp` runs a reproducible demonstration. The Makefile compiles these translation units as C++17. This modular structure makes the core types reusable while keeping the executable demonstration separate from their implementation.

The system is a teaching model, not a deployed hazard prediction service. Its index weights and coordinate conversion are illustrative, and its telemetry is generated from fixed sample values rather than acquired from field hardware.

## 2. System Architecture & File Organization

| Path | Responsibility |
|---|---|
| `include/DisasterCore.h` | Central public declarations for `DisasterCore::Sensors` and `DisasterCore::Alerts`: station and buffer types, the virtual device hierarchy, exceptions, routing types, manipulators, and incident logger. It also defines small inline calculations and accessors. |
| `include/TelemetryBuffer.h` | Focused compatibility include for clients that use `TelemetryBuffer`; it includes `DisasterCore.h`, where the class is declared. |
| `src/Sensors.cpp` | Defines `SensorStation` lifetime accounting, auditing, comparison and stream operators, `RegionalZone` aggregation, dynamic `TelemetryBuffer` ownership, and probe telemetry behavior. |
| `src/Alerts.cpp` | Defines siren alert-level state, stream formatting functions, and `IncidentLogger::demonstrateRollback()` for binary incident persistence and rollback. |
| `main.cpp` | Constructs the sample network, exercises the Units I–IV mechanisms, and prints their results. |
| `Makefile` | Builds `main.cpp`, `src/Sensors.cpp`, and `src/Alerts.cpp` using g++ and provides `all`, `run`, and `clean` targets. |

The inheritance and containment relationships are:

```text
                         DisasterTelemetryDevice
                           (abstract interface)
                           /                 \
                 virtual /                   \ virtual
                         /                     \
             SolarPoweredNode          WirelessLoRaTransceiver
                         \                     /
                          \                   /
                            BaseSensorNode (abstract)
                              | contains
                              +------ PowerSubsystem
                              |
                              v
                    AutonomousHybridProbe
```

Both intermediate classes virtually inherit the same `DisasterTelemetryDevice` base. `BaseSensorNode` inherits both branches, so the derived probe has one shared root subobject. It also owns a `PowerSubsystem` member by value, which is containership/composition rather than a second inheritance relationship.

## 3. Detailed Syllabus Implementation (Units I–IV)

### 3.1 Unit I — Core Foundations & Call Semantics

#### Namespaces

Sensor and hazard types live in `DisasterCore::Sensors`; alert and routing types live in `DisasterCore::Alerts`. This qualifies domain names and reduces accidental name collisions. In `main.cpp`, `using namespace DisasterCore;` is followed by local namespace directives inside `main()` for concise demonstration code; library declarations remain explicitly organized in the two nested namespaces.

#### Inline risk algorithms and overload resolution

`include/DisasterCore.h` defines the calculations inline so they can be used at call sites without a separate function definition. Each input is normalized and clamped before applying a weighted sum, keeping the resulting index within `[0, 100]` for finite input values:

```cpp
inline double computeHazardIndex(double rainfallMmPerHour, double slopeDegrees) {
    const double rain = std::clamp(rainfallMmPerHour / 100.0, 0.0, 1.0);
    const double slope = std::clamp(slopeDegrees / 45.0, 0.0, 1.0);
    return 100.0 * (0.65 * rain + 0.35 * slope);
}
```

A second overload accepts strong telemetry types, allowing soil saturation and vibration frequency to be distinguished at compile time despite both measurements being represented internally as `double`:

```cpp
struct SoilSaturation { double percent; };
struct VibrationFrequency { double hertz; };

inline double computeHazardIndex(SoilSaturation soilTelemetry,
                                 VibrationFrequency vibrationTelemetry) {
    const double soil = std::clamp(soilTelemetry.percent / 100.0, 0.0, 1.0);
    const double vibration = std::clamp(vibrationTelemetry.hertz / 20.0, 0.0, 1.0);
    return 100.0 * (0.7 * soil + 0.3 * vibration);
}
```

The actual implementation uses weights `0.7` for saturation and `0.3` for vibration. `main.cpp` invokes both overloads, and `SensorStation` and `AutonomousHybridProbe` use the rainfall/slope form when updating their hazard values.

#### Call and return by reference

`offsetCoordinate(GeoCoordinate& point, double north, double east)` mutates the supplied coordinate instead of copying it, then returns `GeoCoordinate&` to the same object. This supports in-place coordinate adjustment and permits the caller in `main.cpp` to bind `transformed` directly to the updated value. The conversion approximates degrees of latitude and longitude from meter offsets; longitude scaling accounts for the cosine of latitude with a lower bound to avoid division by a value near zero.

`TelemetryBuffer::accumulate(double value)` also returns `double&`. With allocated capacity, it shifts the stored samples and returns a reference to the newest value in `data_[0]`; with zero capacity, it accumulates into the object's `accumulator_` member and returns a reference to that member. The reference is valid while its owning buffer remains alive and, for allocated data, until the buffer is changed or destroyed.

#### Friend inspection

`SensorStation` declares `friend class TelemetryAuditor;`. `TelemetryAuditor::inspect(const SensorStation&, std::ostream&)` in `src/Sensors.cpp` reads the private rainfall, slope, saturation, and vibration fields directly. This gives the audit operation controlled access without exposing raw readings through public getters, while ordinary clients receive only selected public observations such as `hazard()` and `location()`.

### 3.2 Unit II — Encapsulation, Dynamic Memory & Operator Overloading

#### Encapsulation and const correctness

`SensorStation` stores its identity, region, location, four raw measurements, and derived hazard index as private members. Its public accessors (`id()`, `region()`, `location()`, and `hazard()`) are `const`; string and coordinate accessors return const references to avoid unnecessary copies while preventing mutation through the returned reference. The static `activeCount()` accessor is `noexcept` and exposes the count without providing a way to change it.

`EarlyWarningSiren` keeps its station identifier and local alert level private. Its `stationId()` and `alertLevel()` accessors are const, and `globalAlertLevel()` reads a class-wide static value. `dispatch(int, const std::string&)` bounds a requested level to the supported range 0–3 and updates both local and global state.

#### Static network state

`SensorStation::activeCount_` is initialized once in `src/Sensors.cpp`. The normal constructor and copy constructor increment it; the destructor decrements it. Assignment does not change the number of live objects. `EarlyWarningSiren::globalAlertLevel_` similarly provides shared network alert state, and `dispatch()` retains the greatest level reached across siren instances. The simulation prints both static values after constructing stations and dispatching an alert.

#### Dynamic telemetry memory and copy semantics

`TelemetryBuffer` owns a dynamically allocated `double[]`, represented by `data_` and `size_`. Its capacity constructor zero-initializes an array; its pointer/count constructor copies supplied samples; the copy constructor allocates independent storage; copy assignment allocates the replacement before releasing current storage and handles self-assignment; and the destructor calls `delete[]`.

This is the **Rule of Three**: destructor, copy constructor, and copy-assignment operator are explicitly supplied because the class owns raw memory. Move construction and move assignment are not implemented, so this class does not implement the Rule of Five. In `main.cpp`, copying a buffer and then accumulating a new sample demonstrates that the original and copy hold separate arrays. The design avoids shared ownership and double deletion in the demonstrated copy paths.

#### Operator overloading

- `operator+(const SensorStation&, const SensorStation&)` returns a `RegionalZone`. It stores the two station identifiers and their hazard values in vectors and combines region labels when the stations belong to different regions.
- `operator>(const SensorStation&, const SensorStation&)` compares hazard values, enabling direct hazard-priority checks.
- `operator<<(std::ostream&, const SensorStation&)` writes the station fields in a whitespace-delimited format. Because it accepts `std::ostream&`, it supports console and file output.
- `operator>>(std::istream&, SensorStation&)` parses the matching fields into local variables before changing the station, then updates its private state and recomputes the hazard index. It works with string streams and file streams.

The demo writes a station to `alf_ewg_station.txt`, seeks to the beginning, reads it into another station object, and prints the round-tripped state. The `<<` operator sets fixed precision to three decimal places on the supplied stream.

### 3.3 Unit III — Inheritance, Containership & Polymorphism

#### Abstract base interface

`DisasterTelemetryDevice` declares the pure virtual `deviceName() const` and has a virtual destructor. `BaseSensorNode` provides the common device name and power reporting, while remaining abstract through its pure virtual operations:

```cpp
virtual double readTelemetry() = 0;
virtual bool evaluateThreshold(double threshold) const = 0;
```

`AutonomousHybridProbe` implements both. Its `readTelemetry()` either raises `SensorFailureException` if a fault is set or computes the hazard value and consumes a small amount of power. Its `evaluateThreshold()` compares the most recently read hazard against a threshold and raises `ThresholdOverflowException` for values outside 0–100.

#### Virtual inheritance diamond

`SolarPoweredNode` and `WirelessLoRaTransceiver` each declare `public virtual DisasterTelemetryDevice` inheritance. `BaseSensorNode` inherits both, and `AutonomousHybridProbe` derives from `BaseSensorNode`. Virtual inheritance ensures that the joined hierarchy shares a single `DisasterTelemetryDevice` subobject. This avoids ambiguity and duplicated root state in the diamond while retaining separate solar-power and LoRa-link behavior.

#### Containership with `PowerSubsystem`

`BaseSensorNode` has a `PowerSubsystem power_` data member, initialized alongside its identifier in `BaseSensorNode(std::string id, double initialPower)`. The subsystem owns its charge value, exposes it through `charge() const`, and limits consumption at zero. This models power as a component used by a sensor rather than as a base class, which keeps the device hierarchy focused on type relationships.

#### Runtime polymorphism

`main.cpp` stores addresses of two concrete probes in `std::vector<BaseSensorNode*>`. Each sweep cycle invokes `readTelemetry()`, `deviceName()`, `powerSource()`, `linkType()`, `powerLevel()`, and `evaluateThreshold()` through base-class pointers. The virtual calls select the concrete probe implementation at runtime (the usual implementation uses a virtual dispatch table, commonly called a vtable). The example demonstrates polymorphic behavior; it does not inspect compiler-specific `_vptr` data, which is not part of the C++ language interface.

### 3.4 Unit IV — Streams, Persistence, Exception Safety & STL

#### Exceptions

`SensorFailureException`, `TelemetryChecksumMismatch`, and `ThresholdOverflowException` each derive directly from `std::exception` and override `what() const noexcept` with a domain-specific message. They form three custom exception types rather than a shared custom inheritance hierarchy. The main simulation catches and prints each type: sensor read failure, invalid threshold, and a checksum mismatch reported after rollback.

#### Binary persistence and rollback

`IncidentLogger::demonstrateRollback(const std::string&, std::ostream&)` in `src/Alerts.cpp` opens a binary `std::fstream` for input and output, truncating an earlier demonstration file. It writes a trivially copyable `IncidentRecord` for a committed incident, saves the transaction boundary from `tellp()`, and appends a deliberately corrupted pending record.

The logger then uses `seekg(rollbackPoint)` and `read()` to inspect the pending record. If the checksum does not match, it clears stream state, calls `seekp(rollbackPoint)` to restore the write position at the transaction boundary, closes the stream, and uses `std::filesystem::resize_file()` to remove the uncommitted tail. Thus, `seekg()` and `seekp()` participate in locating and resetting the transaction boundary, while truncation performs the final physical rollback. `main.cpp` reports success and then throws/catches `TelemetryChecksumMismatch` to demonstrate the exception type. The record is a raw in-process POD-like layout; it is intended for the local demonstration and is not a portable cross-platform file format.

#### STL containers, algorithms, and custom routing functor

`std::vector` is used for station IDs and hazard values in `RegionalZone`, for the base-pointer sweep list, and for evacuation routes. `std::map<std::string, double>` stores sample regional risk values. `std::sort()` orders the route vector with the custom `RoutePriority` functor: higher priority sorts first, and equal priority is resolved by shorter distance. `std::for_each()` runs the sweep operation and prints the ordered evacuation rows.

#### Custom stream manipulators

`tabularAlerts(std::ostream&)` and `endAlertTable(std::ostream&)`, declared in `include/DisasterCore.h` and defined in `src/Alerts.cpp`, are used as manipulators with insertion syntax (`std::cout << tabularAlerts;`). The first emits the dispatch-table heading and column labels; the second emits the closing marker. These are stateless formatting functions and do not maintain formatting state on the stream.

## 4. Build, Execution & Verification Results

The Makefile sets the compiler flags and source list as follows:

```make
CXX := g++
CXXFLAGS := -std=c++17 -Wall -Wextra -pedantic -Iinclude
SOURCES := main.cpp src/Sensors.cpp src/Alerts.cpp
```

Build and run with:

```sh
make
./alf_ewg
```

The equivalent direct compilation command is:

```sh
g++ -std=c++17 -Wall -Wextra -pedantic -Iinclude \
    main.cpp src/Sensors.cpp src/Alerts.cpp -o alf_ewg
```

The project was compiled with these flags and the executable was run. The compiler emitted no warnings or errors for the verified build. The simulation output demonstrated the following behaviors:

1. The two risk overloads produced indices for rainfall/slope and soil/vibration inputs, and the coordinate routine returned the updated coordinate by reference.
2. `TelemetryAuditor` printed private raw metrics; the active station count was two; a copied telemetry buffer retained its earlier first sample after the original was updated.
3. Station output and extraction worked with string and file streams. The regional `+` operation produced a zone with two stations, and `>` compared hazards. The siren updated the global alert level to 3.
4. Two sensor probes completed two cyclical sweeps through `BaseSensorNode*`, reporting hazard and remaining power. The configured fault and out-of-range threshold produced their respective caught exceptions.
5. The evacuation table appeared in priority order, with distance breaking the equal-priority tie. The regional map contained two sample entries.
6. The binary logger detected a corrupt pending record, reported rollback to byte 48, retained one committed record, and the simulation printed the `TelemetryChecksumMismatch` message.

The run also creates `alf_ewg_station.txt` and `alf_ewg_incidents.bin` in the current working directory. The incident log is reset at the start of each demonstration run.

## 5. Conclusion & Future Scope

ALF-EWG demonstrates a broad set of CIC-211 concepts in one coherent disaster-monitoring domain. Its namespace separation, encapsulated station state, explicit ownership and deep copying for the dynamic buffer, abstract sensor contract, virtual inheritance diamond, composition-based power subsystem, and STL-based route prioritization provide a clear foundation for extension. The binary logging example illustrates checksum validation and transaction-boundary rollback, while custom exceptions keep failure causes distinguishable to callers.

The current implementation is intentionally illustrative. Future work could replace raw buffer ownership with `std::vector<double>` or `std::unique_ptr<double[]>`, add move operations if raw ownership is retained, validate all telemetry inputs, and use a versioned portable serialization format instead of writing native struct layout. A production system would also require calibrated risk models, time-stamped sensor acquisition, redundant communications, durable logging with explicit I/O error handling, concurrency controls, geospatial validation, and alert delivery to external services. These additions would preserve the present modular interfaces while moving the project toward field deployment.
