#include "cbb_logging.h"

#include <iostream>

namespace {
bool verbose = false;
}

namespace cbb_logging {

void set_verbose(bool enabled) {
    verbose = enabled;
}

bool verbose_enabled() {
    return verbose;
}

std::ostream& debug() {
    return std::cout;
}

std::ostream& error() {
    return std::cerr;
}

} // namespace cbb_logging
