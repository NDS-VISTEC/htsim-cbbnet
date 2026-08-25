#ifndef CBB_LOGGING_H
#define CBB_LOGGING_H

#include <ostream>

namespace cbb_logging {

void set_verbose(bool enabled);
bool verbose_enabled();
std::ostream& debug();
std::ostream& error();

} // namespace cbb_logging

#endif
