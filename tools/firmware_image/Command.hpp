#pragma once

#include "tools/firmware_image/KeySet.hpp"
#include <ostream>
#include <string>
#include <vector>

namespace firmware_image
{
    int Run(const std::vector<std::string>& arguments, std::ostream& out, std::ostream& err, const RandomSource& random);
}
