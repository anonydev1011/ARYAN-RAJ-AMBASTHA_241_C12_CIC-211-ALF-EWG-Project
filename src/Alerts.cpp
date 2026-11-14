#include "DisasterCore.h"
#include <fstream>
#include <filesystem>
#include <iostream>
#include <cstdio>
#include <cstdint>
#include <type_traits>

namespace DisasterCore::Alerts {
int EarlyWarningSiren::globalAlertLevel_ = 0;
EarlyWarningSiren::EarlyWarningSiren(std::string id) : stationId_(std::move(id)) {}
void EarlyWarningSiren::dispatch(int level, const std::string& message) {
    alertLevel_ = std::clamp(level, 0, 3);
    globalAlertLevel_ = std::max(globalAlertLevel_, alertLevel_);
    std::cout << "SIREN " << stationId_ << " level=" << alertLevel_ << ": " << message << '\n';
}

std::ostream& tabularAlerts(std::ostream& out) {
    return out << "\nALERT DISPATCH TABLE\n" << "Village\tPriority\tDistance(km)\n";
}
std::ostream& endAlertTable(std::ostream& out) { return out << "-- end dispatch --\n"; }

namespace {
struct IncidentRecord {
    char id[24]{};
    std::int32_t level{0};
    double hazard{0.0};
    std::uint32_t checksum{0};
};
std::uint32_t checksum(const IncidentRecord& r) {
    return static_cast<std::uint32_t>(r.level * 31 + static_cast<int>(r.hazard * 100.0));
}
}

bool IncidentLogger::demonstrateRollback(const std::string& filePath, std::ostream& report) {
    static_assert(std::is_trivially_copyable<IncidentRecord>::value, "binary record must be POD-like");
    std::fstream file(filePath, std::ios::binary | std::ios::in | std::ios::out | std::ios::trunc);
    if (!file) return false;
    IncidentRecord committed{};
    std::snprintf(committed.id, sizeof(committed.id), "%s", "INC-COMMITTED");
    committed.level = 1; committed.hazard = 42.5; committed.checksum = checksum(committed);
    file.write(reinterpret_cast<const char*>(&committed), sizeof(committed));
    const std::streampos rollbackPoint = file.tellp();

    IncidentRecord pending{};
    std::snprintf(pending.id, sizeof(pending.id), "%s", "INC-UNCOMMITTED");
    pending.level = 3; pending.hazard = 98.0; pending.checksum = 0; // deliberately corrupt
    file.write(reinterpret_cast<const char*>(&pending), sizeof(pending));
    file.flush();

    file.seekg(rollbackPoint);
    IncidentRecord readBack{};
    file.read(reinterpret_cast<char*>(&readBack), sizeof(readBack));
    if (!file || readBack.checksum != checksum(readBack)) {
        file.clear();
        file.seekp(rollbackPoint); // restore write cursor to the transaction boundary
        file.flush();
        file.close();
        std::filesystem::resize_file(filePath, static_cast<std::uintmax_t>(rollbackPoint));
        report << "Binary transaction checksum failed; rolled back to byte "
               << static_cast<std::streamoff>(rollbackPoint)
               << " using seekg/seekp. Remaining committed records: 1\n";
        return true;
    }
    file.close();
    return false;
}
} // namespace DisasterCore::Alerts
