#include "fabric/core/SystemBase.hh"
#include "fabric/core/AppContext.hh"
#include <cstdlib>

#if defined(__GNUC__) || defined(__clang__)
#include <cxxabi.h>
#endif

namespace fabric {

void SystemBase::init(AppContext& ctx) {
    if (!initialized_) {
        initialized_ = true;
        doInit(ctx);
    }
}

void SystemBase::shutdown() {
    if (initialized_ && !shutDown_) {
        shutDown_ = true;
        doShutdown();
    }
}

std::string SystemBase::name() const {
#if defined(__GNUC__) || defined(__clang__)
    int status = 0;
    const char* mangled = typeid(*this).name();
    char* demangled = abi::__cxa_demangle(mangled, nullptr, nullptr, &status);
    if (status == 0 && demangled) {
        std::string result(demangled);
        std::free(demangled);
        return result;
    }
    return mangled;
#else
    return typeid(*this).name();
#endif
}

void SystemBase::after(std::type_index dep) {
    afterDeps_.push_back(dep);
}

void SystemBase::before(std::type_index dep) {
    beforeDeps_.push_back(dep);
}

} // namespace fabric
