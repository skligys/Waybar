#include <filesystem>
#include <fstream>
#include <fmt/format.h>
#include "modules/gpu.hpp"

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
  int busy_percent = getUsage();
  int temperature_c = getTemperature();
  float fan_krpms = getFanKiloRpms();
  label_.set_markup(fmt::format(fmt::runtime(format_),
                    fmt::arg("busy", busy_percent),
                    fmt::arg("temperature_c", temperature_c),
                    fmt::arg("fan_krpms", fan_krpms)));
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

auto waybar::modules::Gpu::getUsage() -> int {
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
