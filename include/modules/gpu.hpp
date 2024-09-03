#pragma once

#include <array>
#include <cmath>
#include "ALabel.hpp"
#include "bar.hpp"
#include "util/sleeper_thread.hpp"

namespace waybar::modules {

constexpr unsigned bits_needed(unsigned base, unsigned size) {
  return std::ceil(size * std::log2(base));
}

template <unsigned base, unsigned size>
class Ring {
  // Make sure values fit in 64 bits, up to 20 values of base 9 fit.
  static_assert(base >= 2 && base <= 255 && size >= 1 && bits_needed(base, size) <= 64,
                "Ring<base, size> does not fit in storage");
  using storage_t = uint64_t;
  // Values are stored as: val[0] * base^0 + val[1] * base^1 + ... + val[size - 1] * base^(size - 1).
  storage_t storage_;
  const std::array<uint64_t, size> powers_;

 public:
  Ring();
  using value_type = uint8_t;
  void push(value_type value) {
    storage_ = storage_ % powers_[size - 1] * base + value;
  }

  class iterator;
  iterator begin() const { return iterator(size - 1, storage_, powers_); }
  iterator end() const { return iterator(-1, storage_, powers_); }
};

class Gpu : public ALabel {
 public:
  Gpu(const std::string&, const Json::Value&);
  ~Gpu() = default;
  void update();

 private:
  int                 getUsage();
  int                 getTemperature();
  float               getFanKiloRpms();

  std::string         file_path_;
  util::SleeperThread thread_;
};

}  // namespace waybar::modules
