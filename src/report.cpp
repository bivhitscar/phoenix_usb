// SPDX-License-Identifier: GPL-2.0-only

#include "report.h"

#include <algorithm>
#include <cstring>

namespace phoenix {

bool parseReport(const std::uint8_t* data, std::size_t length, Report& report)
{
    if (data == nullptr || length != kReportSize)
        return false;

    std::copy_n(data, kReportSize, report.raw.begin());
    return true;
}

} // namespace phoenix