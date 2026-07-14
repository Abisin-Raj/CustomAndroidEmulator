#include "firmware/smc_types.h"
#include <sstream>
#include <iomanip>

namespace firmware {

// ---------------------------------------------------------------------------
// SmcId::toString() — human-readable representation of an SMC identifier
// ---------------------------------------------------------------------------
std::string SmcId::toString() const {
    std::ostringstream oss;
    oss << "SMC_ID[0x" << std::hex << std::uppercase << std::setw(8) << std::setfill('0') << raw << "]"
        << " FastCall=" << (isFastCall() ? "Y" : "N")
        << " SMC64="    << (is64bit()    ? "Y" : "N")
        << " Owner=0x"  << std::setw(2) << std::setfill('0') << static_cast<int>(owner())
        << " Svc=0x"    << std::setw(2) << std::setfill('0') << static_cast<int>(service())
        << " Cmd=0x"    << std::setw(2) << std::setfill('0') << static_cast<int>(command());
    return oss.str();
}

} // namespace firmware
