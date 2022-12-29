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
  thread_ = [this] {
    dp.emit();
    thread_.sleep_for(interval_);
  };
}

waybar::modules::Gammastep::~Gammastep() {
  if (pid_ > 0) killChildProcess();
}

auto waybar::modules::Gammastep::killChildProcess() -> void {
  killpg(pid_, SIGTERM);
  waitpid(pid_, NULL, 0);
  pid_ = -1;
  status_ = "deactivated";
}

auto waybar::modules::Gammastep::update() -> void {
  pid_ = util::command::processByName("gammastep");
  status_ = pid_ >= 0 ? "activated" : "deactivated";

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
      killChildProcess();
    } else {
      pid_ = util::command::forkExec("gammastep -m wayland");
      status_ = "activated";
    }
  }
  // Delay updates a bit to let the process start/stop.
  thread_.sleep_for(std::chrono::seconds(1));
  ALabel::handleToggle(e);
  return true;
}
