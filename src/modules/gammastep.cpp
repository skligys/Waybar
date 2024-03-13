#include <fmt/format.h>
#include "modules/gammastep.hpp"
#include <spdlog/spdlog.h>
#include "util/command.hpp"

waybar::modules::Gammastep::Gammastep(const std::string& id, const Bar& bar,
                                      const Json::Value& config)
    : ALabel(config, "gammastep", id, "{status}", 5),
      bar_(bar),
      pid_(-1),
      status_("deactivated") {
  pid_ = util::command::processByName("gammastep");
  if (pid_ >= 0) status_ = "activated";
  event_box_.add_events(Gdk::BUTTON_PRESS_MASK);
  event_box_.signal_button_press_event().connect(sigc::mem_fun(*this, &Gammastep::handleToggle));
  dp.emit();
}

waybar::modules::Gammastep::~Gammastep() {
  if (pid_ > 0) {
    kill(-pid_, 9);
  }
}

auto waybar::modules::Gammastep::update() -> void {
  label_.set_markup(
      fmt::format(fmt::runtime(format_), fmt::arg("status", status_), fmt::arg("icon", getIcon(0, status_))));
  label_.get_style_context()->add_class(status_);
  if (tooltipEnabled()) {
    label_.set_tooltip_text(status_);
  }
}

bool waybar::modules::Gammastep::handleToggle(GdkEventButton* const& e) {
  if (e->button == 1) {
    label_.get_style_context()->remove_class(status_);
    if (pid_ > 0) {
      kill(-pid_, 9);
      pid_ = -1;
      status_ = "deactivated";
    } else {
      pid_ = util::command::forkExec("gammastep -m wayland");
      status_ = "activated";
    }
  }
  ALabel::handleToggle(e);
  return true;
}
