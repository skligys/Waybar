#pragma once

#include "ALabel.hpp"
#include "bar.hpp"
#include "util/sleeper_thread.hpp"

namespace waybar::modules {

class Gammastep : public ALabel {
 public:
  Gammastep(const std::string&, const waybar::Bar&, const Json::Value&);
  ~Gammastep();
  void update();

 private:
  bool handleToggle(GdkEventButton* const& e);
  void killChildProcess();

  const Bar&                    bar_;
  int32_t                       pid_;
  std::string                   status_;
  util::SleeperThread thread_;
};

}  // namespace waybar::modules
