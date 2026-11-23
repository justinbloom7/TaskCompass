// SPDX-License-Identifier: GPL-3.0-or-later
#include "FractionalRank.h"

#include <algorithm>
#include <array>
#include <bit>
#include <stdexcept>

namespace taskcompass {
namespace {

// Little-endian base-256. Grid interpolation needs seven bits beyond
// the fractional precision for at most 65 participants plus one.
using Integer = std::array<unsigned char, 17>;

Integer add(const Integer &left, const Integer &right) {
   Integer result{};
   unsigned carry = 0;
   for (std::size_t i = 0; i < result.size(); ++i) {
      carry += left[i] + right[i];
      result[i] = static_cast<unsigned char>(carry);
      carry >>= 8;
   }
   if (carry) { throw std::overflow_error("Rank arithmetic overflow."); }
   return result;
}

Integer subtract(const Integer &left, const Integer &right) {
   Integer result{};
   int borrow = 0;
   for (std::size_t i = 0; i < result.size(); ++i) {
      const int digit = left[i] - right[i] - borrow;
      result[i] = static_cast<unsigned char>(digit);
      borrow = digit < 0;
   }
   if (borrow) { throw std::underflow_error("Rank arithmetic underflow."); }
   return result;
}

Integer scale(const Integer &value, unsigned factor) {
   Integer result{};
   unsigned carry = 0;
   for (std::size_t i = 0; i < result.size(); ++i) {
      carry += value[i] * factor;
      result[i] = static_cast<unsigned char>(carry);
      carry >>= 8;
   }
   if (carry) { throw std::overflow_error("Rank product overflow."); }
   return result;
}

Integer divide(const Integer &value, unsigned divisor) {
   Integer result{};
   unsigned remainder = 0;
   for (std::size_t i = result.size(); i-- > 0;) {
      remainder = 256 * remainder + value[i];
      result[i] = static_cast<unsigned char>(remainder / divisor);
      remainder %= divisor;
   }
   return result;
}

bool less(const Integer &left, const Integer &right) {
   return std::lexicographical_compare(left.rbegin(), left.rend(), right.rbegin(), right.rend());
}

Integer gridFloor(std::string_view rank, std::size_t precision, bool upperSentinel = false) {
   Integer result{};
   if (upperSentinel) {
      result[precision / 8] = static_cast<unsigned char>(1u << (precision % 8));
   } else {
      for (std::size_t i = 0; i < std::min(precision, rank.size()); ++i) {
         const auto bit = precision - i - 1;
         if (rank[i] == '1') { result[bit / 8] |= static_cast<unsigned char>(1u << (bit % 8)); }
      }
   }
   return result;
}

std::string encode(const Integer &integer, std::size_t precision) {
   std::string result(precision, '0');
   for (std::size_t i = 0; i < precision; ++i) {
      if ((integer[i / 8] >> (i % 8)) & 1u) { result[precision - i - 1] = '1'; }
   }
   while (!result.empty() && result.back() == '0') { result.pop_back(); }
   return result;
}

std::vector<std::string> normalize(std::string_view lower, std::string_view upper, unsigned count) {
   if (!upper.empty() && lower >= upper) { return {}; }
   const Integer one{1};
   for (std::size_t precision = 1; precision <= MAX_RANK_BYTES; ++precision) {
      const auto floor = gridFloor(lower, precision);
      const auto ceiling = add(gridFloor(upper, precision, upper.empty()), Integer{static_cast<unsigned char>(upper.size() > precision)});
      const auto first = add(floor, one);
      if (less(ceiling, add(first, Integer{static_cast<unsigned char>(count)}))) { continue; }
      const auto pointsPlusOne = add(subtract(ceiling, first), one);
      std::vector<std::string> ranks;
      for (unsigned slot = 1; slot <= count; ++slot) {
         const auto offset = subtract(divide(scale(pointsPlusOne, slot), count + 1), one);
         ranks.push_back(encode(add(first, offset), precision));
      }
      return ranks;
   }
   return {};
}

}

bool validRank(std::string_view rank) {
   return !rank.empty() && rank.size() <= MAX_RANK_BYTES && rank.back() == '1'
          && rank.find_first_not_of("01") == std::string_view::npos;
}

std::string seedRank(std::uint64_t position, std::uint64_t count) {
   if (position == 0 || position > count) { throw std::invalid_argument("Rank seed position is out of range."); }
   Integer digits{};
   for (std::size_t i = 0; i < sizeof(position); ++i) {
      digits[i] = static_cast<unsigned char>(position);
      position >>= 8;
   }
   return encode(digits, std::bit_width(count));
}

RankPlacement placeRank(const std::vector<std::string> &ordered, std::size_t gap) {
   if (gap > ordered.size() || !std::is_sorted(ordered.begin(), ordered.end())
       || !std::all_of(ordered.begin(), ordered.end(), validRank)) {
      throw std::invalid_argument("Invalid rank band or insertion gap.");
   }
   const auto rankBefore = [&](std::size_t i) -> std::string_view { return i ? ordered[i - 1] : std::string_view{}; };
   const auto rankAfter = [&](std::size_t i) -> std::string_view { return i < ordered.size() ? ordered[i] : std::string_view{}; };
   const auto lower = rankBefore(gap);
   const auto upper = rankAfter(gap);
   if (upper.empty() || lower < upper) {
      const auto precision = std::max(lower.size(), upper.size());
      auto midpoint = encode(add(gridFloor(lower, precision), gridFloor(upper, precision, upper.empty())), precision + 1);
      if (validRank(midpoint) && !std::binary_search(ordered.begin(), ordered.end(), midpoint)) {
         return {gap, {std::move(midpoint)}};
      }
   }
   for (std::size_t size = 1; size <= std::min(MAX_RANK_WINDOW, ordered.size()); ++size) {
      std::vector<std::size_t> starts;
      for (std::size_t left = 0; left <= std::min(size, gap); ++left) {
         const auto right = size - left;
         if ((gap && !left) || (gap < ordered.size() && !right) || right > ordered.size() - gap) { continue; }
         starts.push_back(gap - left);
      }
      const auto balance = [gap, size](std::size_t start) {
         const auto left = gap - start;
         const auto right = size - left;
         return std::pair{std::max(left, right) - std::min(left, right), start};
      };
      std::sort(starts.begin(), starts.end(), [&](auto a, auto b) { return balance(a) < balance(b); });
      for (const auto start : starts) {
         auto ranks = normalize(rankBefore(start), rankAfter(start + size), static_cast<unsigned>(size + 1));
         if (!ranks.empty()) { return {start, std::move(ranks)}; }
      }
   }
   throw std::runtime_error("No rank available inside the bounded window. No changes made.");
}

}
