#pragma once

#include <map>
#include <string>

#include "ALabel.hpp"
#include "bar.hpp"
#include "util/sleeper_thread.hpp"

namespace waybar::modules {

class DdcUtil : public ALabel {
 public:
  DdcUtil(const std::string&, const waybar::Bar&, const Json::Value&);
  ~DdcUtil();
  void update();

 private:
  void worker();
  bool handleToggle(GdkEventButton* const& e);
  std::string input_source_name(uint8_t input) const;
  uint8_t get_input_source() const;
  uint8_t set_input_source(uint8_t target_input) const;
  std::string source_to_class(const uint8_t input) const;

  const Bar&            bar_;
  const int             i2c_bus_;
  const std::map<uint8_t, std::string> input_name_;
  const uint8_t        primary_input_;
  const uint8_t        secondary_input_;
  uint8_t              curr_input_;
  std::string           status_;
  util::SleeperThread   thread_;
  std::mutex            ddc_mutex_;
};

}  // namespace waybar::modules
