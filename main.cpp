#include "DisasterCore.h"

#include <algorithm>
#include <fstream>
#include <iostream>
#include <sstream>
#include <vector>

using namespace DisasterCore;

int main() {
    using namespace Sensors;
    using namespace Alerts;
    std::cout << "=== CIC-211 Option 1: ALF-EWG ===\n";

    // Unit I: inline overloads, in-place reference transformation and friend audit.
    std::cout << "Unit I risk overloads: rain/slope=" << computeHazardIndex(72.0, 32.0)
              << " soil/vibration=" << computeHazardIndex(SoilSaturation{84.0}, VibrationFrequency{8.0}) << '\n';
    GeoCoordinate coordinate{30.25, 78.10};
    GeoCoordinate& transformed = offsetCoordinate(coordinate, 250.0, 120.0);
    std::cout << "Coordinate transformed in place and returned by reference: "
              << transformed.latitude << ',' << transformed.longitude << '\n';

    SensorStation ridge("ST-01", "Ridge", coordinate, 72.0, 32.0, 84.0, 8.0);
    SensorStation valley("ST-02", "Valley", {30.31, 78.22}, 93.0, 38.0, 91.0, 12.0);
    TelemetryAuditor::inspect(ridge, std::cout);
    std::cout << "Network active station count: " << SensorStation::activeCount() << '\n';

    // Unit II: dynamic buffer ownership and deep copy; references accumulate newest first.
    const double initial[] = {11.0, 12.0, 13.0};
    TelemetryBuffer buffer(initial, 3);
    TelemetryBuffer copied(buffer);
    double& latest = buffer.accumulate(15.5);
    std::cout << "Telemetry deep-copy buffer latest-by-reference=" << latest
              << ", original[0]=" << buffer.data()[0] << ", copy[0]=" << copied.data()[0] << '\n';
    std::cout << "Station stream <<: " << ridge << '\n';
    std::istringstream stationInput("ST-03 Basin 30.2 78.4 55 27 66 4");
    stationInput >> ridge;
    std::cout << "Station stream >> restored/replaced state: " << ridge << '\n';
    {
        std::fstream stationFile("alf_ewg_station.txt", std::ios::in | std::ios::out | std::ios::trunc);
        stationFile << ridge << '\n';
        stationFile.seekg(0);
        SensorStation fileRoundTrip("TEMP", "TEMP", {}, 0.0, 0.0, 0.0, 0.0);
        stationFile >> fileRoundTrip;
        std::cout << "Station fstream round-trip: " << fileRoundTrip << '\n';
    }
    RegionalZone northZone = ridge + valley;
    std::cout << "Cluster aggregation + => zone " << northZone.region << " has "
              << northZone.stationIds.size() << " stations\n";
    std::cout << "Relational priority: ST-02 > ST-01 is " << std::boolalpha << (valley > ridge) << '\n';
    EarlyWarningSiren siren("ST-02");
    siren.dispatch(3, "landslide threshold exceeded");
    std::cout << "Static network emergency alert level=" << EarlyWarningSiren::globalAlertLevel() << '\n';

    // Unit III: virtual-inheritance diamond, composition, abstract interface and dynamic dispatch.
    AutonomousHybridProbe probeA("PROBE-A", 68.0, 34.0);
    AutonomousHybridProbe probeB("PROBE-B", 95.0, 40.0, 85.0);
    std::vector<BaseSensorNode*> nodes{&probeA, &probeB};
    for (int cycle = 1; cycle <= 2; ++cycle) {
        std::cout << "Sweep cycle " << cycle << " (BaseSensorNode* dynamic dispatch):\n";
        std::for_each(nodes.begin(), nodes.end(), [](BaseSensorNode* node) {
            const double risk = node->readTelemetry();
            std::cout << "  " << node->deviceName() << " via " << node->powerSource() << '/'
                      << node->linkType() << " risk=" << risk << " power=" << node->powerLevel()
                      << " evacuate=" << node->evaluateThreshold(60.0) << '\n';
        });
    }
    probeA.setFault(true);
    try { (void)probeA.readTelemetry(); }
    catch (const SensorFailureException& error) { std::cout << "Caught SensorFailureException: " << error.what() << '\n'; }
    try { (void)probeB.evaluateThreshold(101.0); }
    catch (const ThresholdOverflowException& error) { std::cout << "Caught ThresholdOverflowException: " << error.what() << '\n'; }

    // Unit IV: STL sorting/for_each and custom comparator/functor plus stream manipulators.
    std::vector<EvacuationRoute> routes{{"Upper Hamlet", 2, 4}, {"River Bend", 3, 8}, {"East Ridge", 3, 3}};
    std::sort(routes.begin(), routes.end(), RoutePriority{});
    std::cout << tabularAlerts;
    std::for_each(routes.begin(), routes.end(), [](const EvacuationRoute& route) {
        std::cout << route.village << '\t' << route.priority << '\t' << route.distanceKm << '\n';
    });
    std::cout << endAlertTable;
    std::map<std::string, double> regionalRisk{{"Ridge", 76.4}, {"Valley", 91.8}};
    std::cout << "Regional risk map entries: " << regionalRisk.size() << '\n';

    try {
        if (IncidentLogger::demonstrateRollback("alf_ewg_incidents.bin", std::cout)) {
            throw TelemetryChecksumMismatch{};
        }
    } catch (const TelemetryChecksumMismatch& error) {
        std::cout << "Caught TelemetryChecksumMismatch after rollback: " << error.what() << '\n';
    }
    std::cout << "=== Simulation complete ===\n";
    return 0;
}
