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

  const Bar&            bar_;
  int                   i2c_bus_;
  std::string           status_;
  util::SleeperThread   thread_;
  std::mutex            ddc_mutex_;
};

}  // namespace waybar::modules
