#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <exception>
#include <iosfwd>
#include <map>
#include <string>
#include <vector>

namespace DisasterCore {
namespace Sensors {

struct GeoCoordinate { double latitude{0.0}; double longitude{0.0}; };

inline GeoCoordinate& offsetCoordinate(GeoCoordinate& point, double north, double east) {
    point.latitude += north / 111320.0;
    const double cosine = std::max(0.01, std::cos(point.latitude * 3.14159265358979323846 / 180.0));
    point.longitude += east / (111320.0 * cosine);
    return point;
}

inline double computeHazardIndex(double rainfallMmPerHour, double slopeDegrees) {
    const double rain = std::clamp(rainfallMmPerHour / 100.0, 0.0, 1.0);
    const double slope = std::clamp(slopeDegrees / 45.0, 0.0, 1.0);
    return 100.0 * (0.65 * rain + 0.35 * slope);
}

struct SoilSaturation { double percent; };
struct VibrationFrequency { double hertz; };

inline double computeHazardIndex(SoilSaturation soilTelemetry, VibrationFrequency vibrationTelemetry) {
    const double soil = std::clamp(soilTelemetry.percent / 100.0, 0.0, 1.0);
    const double vibration = std::clamp(vibrationTelemetry.hertz / 20.0, 0.0, 1.0);
    return 100.0 * (0.7 * soil + 0.3 * vibration);
}

class SensorStation;
struct RegionalZone;
class TelemetryAuditor {
public:
    static void inspect(const SensorStation& station, std::ostream& out);
};

class SensorStation {
public:
    SensorStation(std::string id, std::string region, GeoCoordinate location,
                  double rainfall, double slope, double soil, double vibration);
    SensorStation(const SensorStation& other);
    SensorStation& operator=(const SensorStation& other);
    ~SensorStation();

    const std::string& id() const noexcept { return id_; }
    const std::string& region() const noexcept { return region_; }
    const GeoCoordinate& location() const noexcept { return location_; }
    double hazard() const noexcept { return hazard_; }
    static std::size_t activeCount() noexcept { return activeCount_; }

    friend bool operator>(const SensorStation& lhs, const SensorStation& rhs) noexcept;
    friend RegionalZone operator+(const SensorStation& lhs, const SensorStation& rhs);
    friend std::ostream& operator<<(std::ostream& out, const SensorStation& station);
    friend std::istream& operator>>(std::istream& in, SensorStation& station);
    friend class TelemetryAuditor;

private:
    std::string id_;
    std::string region_;
    GeoCoordinate location_;
    double rainfall_;
    double slope_;
    double soilSaturation_;
    double vibration_;
    double hazard_;
    static std::size_t activeCount_;
};

struct RegionalZone {
    std::string region;
    std::vector<std::string> stationIds;
    std::vector<double> hazardValues;
};
RegionalZone operator+(const SensorStation& lhs, const SensorStation& rhs);

class TelemetryBuffer {
public:
    explicit TelemetryBuffer(std::size_t capacity = 0);
    TelemetryBuffer(const double* values, std::size_t count);
    TelemetryBuffer(const TelemetryBuffer& other);
    TelemetryBuffer& operator=(const TelemetryBuffer& other);
    ~TelemetryBuffer();
    double& accumulate(double value);
    const double* data() const noexcept { return data_; }
    std::size_t size() const noexcept { return size_; }
private:
    double* data_;
    std::size_t size_;
    double accumulator_{0.0};
};

class DisasterTelemetryDevice {
public:
    DisasterTelemetryDevice() = default;
    virtual ~DisasterTelemetryDevice() = default;
    virtual std::string deviceName() const = 0;
};

class SolarPoweredNode : public virtual DisasterTelemetryDevice {
public:
    std::string powerSource() const { return "solar"; }
};
class WirelessLoRaTransceiver : public virtual DisasterTelemetryDevice {
public:
    std::string linkType() const { return "LoRa"; }
};

class PowerSubsystem {
public:
    explicit PowerSubsystem(double charge = 100.0) : charge_(charge) {}
    double charge() const noexcept { return charge_; }
    void consume(double amount) noexcept { charge_ = std::max(0.0, charge_ - amount); }
private:
    double charge_;
};

class SensorFailureException : public std::exception {
public: const char* what() const noexcept override { return "sensor telemetry read failed"; }
};
class TelemetryChecksumMismatch : public std::exception {
public: const char* what() const noexcept override { return "telemetry checksum mismatch"; }
};
class ThresholdOverflowException : public std::exception {
public: const char* what() const noexcept override { return "hazard threshold exceeds supported range"; }
};

class BaseSensorNode : public SolarPoweredNode, public WirelessLoRaTransceiver {
public:
    explicit BaseSensorNode(std::string id, double initialPower = 100.0);
    ~BaseSensorNode() override = default;
    virtual double readTelemetry() = 0;
    virtual bool evaluateThreshold(double threshold) const = 0;
    std::string deviceName() const override { return id_; }
    double powerLevel() const noexcept { return power_.charge(); }
protected:
    std::string id_;
    PowerSubsystem power_;
};

class AutonomousHybridProbe final : public BaseSensorNode {
public:
    AutonomousHybridProbe(std::string id, double rainfall, double slope, double power = 100.0);
    double readTelemetry() override;
    bool evaluateThreshold(double threshold) const override;
    void setFault(bool fault) noexcept { fault_ = fault; }
private:
    double rainfall_;
    double slope_;
    double latestHazard_{0.0};
    bool fault_{false};
};

} // namespace Sensors

namespace Alerts {

class EarlyWarningSiren {
public:
    explicit EarlyWarningSiren(std::string stationId);
    void dispatch(int level, const std::string& message);
    const std::string& stationId() const noexcept { return stationId_; }
    int alertLevel() const noexcept { return alertLevel_; }
    static int globalAlertLevel() noexcept { return globalAlertLevel_; }
private:
    std::string stationId_;
    int alertLevel_{0};
    static int globalAlertLevel_;
};

struct EvacuationRoute { std::string village; int priority; int distanceKm; };
struct RoutePriority {
    bool operator()(const EvacuationRoute& lhs, const EvacuationRoute& rhs) const noexcept {
        if (lhs.priority != rhs.priority) return lhs.priority > rhs.priority;
        return lhs.distanceKm < rhs.distanceKm;
    }
};

std::ostream& tabularAlerts(std::ostream& out);
std::ostream& endAlertTable(std::ostream& out);

class IncidentLogger {
public:
    static bool demonstrateRollback(const std::string& filePath, std::ostream& report);
};

} // namespace Alerts
} // namespace DisasterCore
