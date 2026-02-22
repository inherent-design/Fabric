#include "fabric/utils/Utils.hh"
#include <iomanip>
#include <mutex>
#include <random>
#include <sstream>

namespace fabric {

std::string Utils::generateUniqueId(const std::string& prefix, int length) {
  static std::mutex idMutex;
  static std::random_device rd;
  static std::mt19937 gen(rd());
  static std::uniform_int_distribution<> dis(0, 15);

  std::lock_guard<std::mutex> lock(idMutex);

  std::stringstream ss;
  ss << prefix;

  for (int i = 0; i < length; i++) {
    ss << std::hex << dis(gen);
  }

  return ss.str();
}

} // namespace fabric
