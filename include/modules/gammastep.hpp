#pragma once

#include "ALabel.hpp"
#include "bar.hpp"

namespace waybar::modules {

class Gammastep : public ALabel {
 public:
  Gammastep(const std::string&, const waybar::Bar&, const Json::Value&);
  ~Gammastep();
  void update();

 private:
  bool handleToggle(GdkEventButton* const& e);

  const Bar&                    bar_;
  int32_t                       pid_;
  std::string                   status_;
};

}  // namespace waybar::modules
