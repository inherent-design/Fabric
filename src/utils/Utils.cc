#include "fabric/utils/Utils.hh"
#include <iomanip>
#include <mutex>
#include <random>
#include <sstream>

namespace fabric {

std::string Utils::generateUniqueId(const std::string& prefix, int length) {
    // Mutex must be acquired before touching the PRNG so that TSan sees
    // a clean happens-before on the static initialisation of gen/dis.
    static std::mutex idMutex;
    std::lock_guard<std::mutex> lock(idMutex);

    static std::mt19937 gen(std::random_device{}());
    static std::uniform_int_distribution<> dis(0, 15);

    std::stringstream ss;
    ss << prefix;

    for (int i = 0; i < length; i++) {
        ss << std::hex << dis(gen);
    }

    return ss.str();
}

} // namespace fabric
