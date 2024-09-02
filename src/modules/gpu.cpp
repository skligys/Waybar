#include <algorithm>
#include <codecvt>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <locale>
#include <string>
#include <fmt/format.h>
#include "modules/gpu.hpp"

namespace {

template <unsigned size>
std::array<uint64_t, size> make_powers(unsigned base) {
  std::array<uint64_t, size> result;
  uint64_t power = 1;
  for (unsigned i = 0; i < size; ++i) {
    result[i] = power;
    power *= base;
  }
  return result;
}

// Normalize to an integer in range 0..8.
uint8_t normalize(float x, float min, float max) {
  if (min >= max) throw std::runtime_error("Should be min < max");
  const float clamped = std::max(std::min(x, max), min);
  const float normalized = (clamped - min) / (max - min);
  return static_cast<uint8_t>(std::round(normalized * 8.0f));
}

const char16_t BLANK = u'⠀';
const std::array<char16_t, 9> SPARK_CHARS = { u'⠀', u'▁', u'▂', u'▃', u'▄', u'▅', u'▆', u'▇', u'█'};

template <typename T>
std::string to_utf8(const std::basic_string<T, std::char_traits<T>, std::allocator<T>>& source) {
  std::wstring_convert<std::codecvt_utf8_utf16<T>, T> converter;
  return converter.to_bytes(source);
}

template <unsigned base, unsigned size>
std::string spark(const waybar::modules::Ring<base, size>& ring) {
  std::u16string result;
  result.reserve(size);
  bool all_empty = true;
  for (const auto& value : ring) {
    if (value != 0) all_empty = false;
    result.push_back(SPARK_CHARS[value]);
  }
  return all_empty ? "" : to_utf8(result);
}

}

template <unsigned base, unsigned size>
waybar::modules::Ring<base, size>::Ring() : storage_(0), powers_(make_powers<size>(base)) {}

template <unsigned base, unsigned size>
class waybar::modules::Ring<base, size>::iterator {
 public:
  using iterator_category = std::forward_iterator_tag;
  using value_type = waybar::modules::Ring<base, size>::value_type;
  using difference_type = long;
  using pointer = const uint8_t*;
  using reference = const uint8_t&;

  iterator(int idx, const waybar::modules::Ring<base, size>::storage_t& storage, const std::array<uint64_t, size>& powers)
    : idx_(idx), storage_(storage), powers_(powers) {}
  iterator& operator++() { --idx_; return *this; }
  iterator operator++(int) { iterator result = *this; ++(*this); return result; }
  bool operator==(iterator other) const { return idx_ == other.idx_; }
  bool operator!=(iterator other) const { return !(*this == other); }
  value_type operator*() const {
    const unsigned int divisor = powers_[idx_];
    return (storage_ / divisor) % base;
  }
 private:
  int idx_;
  const waybar::modules::Ring<base, size>::storage_t& storage_;
  const std::array<uint64_t, size>& powers_;
};

waybar::modules::Gpu::Gpu(const std::string& id, const Json::Value& config)
    : ALabel(config, "gpu", id, "G {}", 5) {
  if (config_["hwmon-path"].isString()) {
    file_path_ = config_["hwmon-path"].asString();
  } else {
    file_path_ = "/sys/class/drm/card0/device/hwmon/hwmon1";
  }
  std::ifstream hwmon(file_path_);
  if (!hwmon.is_open()) {
    throw std::runtime_error("Can't open " + file_path_);
  }
  thread_ = [this] {
    dp.emit();
    thread_.sleep_for(interval_);
  };
}

auto waybar::modules::Gpu::update() -> void {
  int busy_percent = getBusyPercent();
  busy_history_.push(normalize(busy_percent, 0.0, 100.0));
  int temperature_c = getTemperature();
  temperature_history_.push(normalize(temperature_c, 40.0, 90.0));
  float fan_krpms = getFanKiloRpms();
  fan_history_.push(normalize(fan_krpms, 0.3, 2.4));
  label_.set_markup(fmt::format(fmt::runtime(format_),
                    fmt::arg("busy", busy_percent),
                    fmt::arg("busy_history", spark(busy_history_)),
                    fmt::arg("temperature_c", temperature_c),
                    fmt::arg("temperature_history", spark(temperature_history_)),
                    fmt::arg("fan_krpms", fan_krpms),
                    fmt::arg("fan_history", spark(fan_history_))));
  if (tooltipEnabled()) {
    std::string tooltip_format = "{busy}% {temperature_c}°C {fan_krpms:.1f}k";
    if (config_["tooltip-format"].isString()) {
      tooltip_format = config_["tooltip-format"].asString();
    }
    label_.set_tooltip_text(fmt::format(fmt::runtime(tooltip_format),
                            fmt::arg("busy", busy_percent),
                            fmt::arg("temperature_c", temperature_c),
                            fmt::arg("fan_krpms", fan_krpms)));
  }
  // Call parent update
  ALabel::update();
}

auto waybar::modules::Gpu::getBusyPercent() -> int {
  std::ifstream f(file_path_ + "/device/gpu_busy_percent");
  if (!f.is_open()) {
    throw std::runtime_error("Can't open " + file_path_ + "/device/gpu_busy_percent");
  }
  std::string line;
  if (f.good()) {
    getline(f, line);
  }
  f.close();
  int busy_percent = std::strtol(line.c_str(), nullptr, 10);
  return busy_percent;
}

auto waybar::modules::Gpu::getTemperature() -> int {
  std::ifstream f(file_path_ + "/temp1_input");
  if (!f.is_open()) {
    throw std::runtime_error("Can't open " + file_path_ + "/temp1_input");
  }
  std::string line;
  if (f.good()) {
    getline(f, line);
  }
  f.close();
  int temperature_c = std::strtol(line.c_str(), nullptr, 10) / 1000;
  return temperature_c;
}

auto waybar::modules::Gpu::getFanKiloRpms() -> float {
  std::ifstream f(file_path_ + "/fan1_input");
  if (!f.is_open()) {
    throw std::runtime_error("Can't open " + file_path_ + "/fan1_input");
  }
  std::string line;
  if (f.good()) {
    getline(f, line);
  }
  f.close();
  int fan_rpms = std::strtol(line.c_str(), nullptr, 10);
  return fan_rpms / 1000.0;
}
