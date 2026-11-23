// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace taskcompass {

inline constexpr std::size_t MAX_RANK_BYTES = 128;
inline constexpr std::size_t MAX_RANK_WINDOW = 64;
bool validRank(std::string_view rank);
std::string seedRank(std::uint64_t position, std::uint64_t count);

struct RankPlacement {
   std::size_t first;
   // If normalized, replaces the window including the incoming row at the gap.
   std::vector<std::string> ranks;
};

RankPlacement placeRank(const std::vector<std::string> &ordered, std::size_t gap);

}
