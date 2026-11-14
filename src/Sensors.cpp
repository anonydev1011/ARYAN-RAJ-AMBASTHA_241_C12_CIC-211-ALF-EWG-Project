#include "DisasterCore.h"
#include <iomanip>
#include <istream>
#include <ostream>
#include <utility>

namespace DisasterCore::Sensors {

std::size_t SensorStation::activeCount_ = 0;

SensorStation::SensorStation(std::string id, std::string region, GeoCoordinate location,
                             double rainfall, double slope, double soil, double vibration)
    : id_(std::move(id)), region_(std::move(region)), location_(location), rainfall_(rainfall),
      slope_(slope), soilSaturation_(soil), vibration_(vibration),
      hazard_(computeHazardIndex(rainfall, slope)) { ++activeCount_; }
SensorStation::SensorStation(const SensorStation& other)
    : id_(other.id_), region_(other.region_), location_(other.location_), rainfall_(other.rainfall_),
      slope_(other.slope_), soilSaturation_(other.soilSaturation_), vibration_(other.vibration_),
      hazard_(other.hazard_) { ++activeCount_; }
SensorStation& SensorStation::operator=(const SensorStation& other) {
    if (this != &other) {
        id_ = other.id_; region_ = other.region_; location_ = other.location_;
        rainfall_ = other.rainfall_; slope_ = other.slope_; soilSaturation_ = other.soilSaturation_;
        vibration_ = other.vibration_; hazard_ = other.hazard_;
    }
    return *this;
}
SensorStation::~SensorStation() { --activeCount_; }

void TelemetryAuditor::inspect(const SensorStation& s, std::ostream& out) {
    out << "[auditor raw/private] " << s.id_ << " rain=" << s.rainfall_
        << " slope=" << s.slope_ << " soil=" << s.soilSaturation_
        << " vibration=" << s.vibration_ << '\n';
}
bool operator>(const SensorStation& lhs, const SensorStation& rhs) noexcept { return lhs.hazard_ > rhs.hazard_; }
std::ostream& operator<<(std::ostream& out, const SensorStation& s) {
    return out << std::fixed << std::setprecision(3) << s.id_ << ' ' << s.region_ << ' '
               << s.location_.latitude << ' ' << s.location_.longitude << ' '
               << s.rainfall_ << ' ' << s.slope_ << ' ' << s.soilSaturation_ << ' ' << s.vibration_;
}
std::istream& operator>>(std::istream& in, SensorStation& s) {
    std::string id, region; double lat, lon, rain, slope, soil, vibration;
    if (in >> id >> region >> lat >> lon >> rain >> slope >> soil >> vibration) {
        s.id_ = std::move(id); s.region_ = std::move(region); s.location_ = {lat, lon};
        s.rainfall_ = rain; s.slope_ = slope; s.soilSaturation_ = soil; s.vibration_ = vibration;
        s.hazard_ = computeHazardIndex(rain, slope);
    }
    return in;
}
RegionalZone operator+(const SensorStation& a, const SensorStation& b) {
    RegionalZone zone{a.region_ == b.region_ ? a.region_ : a.region_ + "+" + b.region_, {}, {}};
    zone.stationIds = {a.id_, b.id_}; zone.hazardValues = {a.hazard_, b.hazard_};
    return zone;
}

TelemetryBuffer::TelemetryBuffer(std::size_t capacity) : data_(capacity ? new double[capacity]{} : nullptr), size_(capacity) {}
TelemetryBuffer::TelemetryBuffer(const double* values, std::size_t count)
    : data_(count ? new double[count] : nullptr), size_(count) {
    for (std::size_t i = 0; i < count; ++i) data_[i] = values[i];
}
TelemetryBuffer::TelemetryBuffer(const TelemetryBuffer& other)
    : data_(other.size_ ? new double[other.size_] : nullptr), size_(other.size_), accumulator_(other.accumulator_) {
    for (std::size_t i = 0; i < size_; ++i) data_[i] = other.data_[i];
}
TelemetryBuffer& TelemetryBuffer::operator=(const TelemetryBuffer& other) {
    if (this != &other) {
        double* copy = other.size_ ? new double[other.size_] : nullptr;
        for (std::size_t i = 0; i < other.size_; ++i) copy[i] = other.data_[i];
        delete[] data_; data_ = copy; size_ = other.size_;
        accumulator_ = other.accumulator_;
    }
    return *this;
}
TelemetryBuffer::~TelemetryBuffer() { delete[] data_; }
double& TelemetryBuffer::accumulate(double value) {
    if (size_ == 0) { accumulator_ += value; return accumulator_; }
    for (std::size_t i = size_ - 1; i > 0; --i) data_[i] = data_[i - 1];
    data_[0] = value; return data_[0];
}

BaseSensorNode::BaseSensorNode(std::string id, double initialPower)
    : id_(std::move(id)), power_(initialPower) {}
AutonomousHybridProbe::AutonomousHybridProbe(std::string id, double rainfall, double slope, double power)
    : BaseSensorNode(std::move(id), power), rainfall_(rainfall), slope_(slope) {}
double AutonomousHybridProbe::readTelemetry() {
    if (fault_) throw SensorFailureException{};
    latestHazard_ = computeHazardIndex(rainfall_, slope_); power_.consume(0.5); return latestHazard_;
}
bool AutonomousHybridProbe::evaluateThreshold(double threshold) const {
    if (threshold > 100.0 || threshold < 0.0) throw ThresholdOverflowException{};
    return latestHazard_ >= threshold;
}

} // namespace DisasterCore::Sensors
