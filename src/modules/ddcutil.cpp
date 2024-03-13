#include <mutex>
#include <fmt/format.h>
#include "modules/ddcutil.hpp"
#include <ddcutil_c_api.h>
#include <ddcutil_status_codes.h>
#include <spdlog/spdlog.h>

namespace {

std::map<uint16_t, std::string> parse_input_names(const Json::Value& config) {
  std::map<uint16_t, std::string> result;
  if (config["input-names"].isObject()) {
    const Json::Value& input_names = config["input-names"];
    // Cannot do range-based iteration since no way to access value's key there.
    for (auto it = input_names.begin(); it != input_names.end(); ++it) {
      if (!it.key().isString()) throw std::runtime_error("Input names key should be a string");
      if (!it->isString()) throw std::runtime_error("Input names value should be a string");
      const std::string key = it.key().asString();
      const std::string value = it->asString();
      const unsigned long key_ul = std::stoul(key, nullptr, 0);
      if (key_ul > UINT16_MAX) throw std::runtime_error("Input names key out of range");
      result.emplace(static_cast<uint16_t>(key_ul), value);
    }
  }
  return result;
}

struct DisplayIdDeleter {
  void operator()(DDCA_Display_Identifier* display_id) {
    const DDCA_Status rc = ddca_free_display_identifier(*display_id);
    if (rc != 0) {
      spdlog::error("ddcutil: Failed to free display id: {}, {}", ddca_rc_name(rc), ddca_rc_desc(rc));
    }
  }
};

std::unique_ptr<DDCA_Display_Identifier, DisplayIdDeleter> display_id_from_busno(int bus_no) {
  std::unique_ptr<DDCA_Display_Identifier, DisplayIdDeleter> display_id(new DDCA_Display_Identifier);
  const DDCA_Status rc = ddca_create_busno_display_identifier(bus_no, display_id.get());
  if (rc != 0) {
    spdlog::error("ddcutil: Failed to create bus number display id: {}, {}", ddca_rc_name(rc), ddca_rc_desc(rc));
    return {};
  }
  return display_id;
}

struct DisplayHandleCloser {
  void operator()(DDCA_Display_Handle* display_handle) {
    const DDCA_Status rc = ddca_close_display(*display_handle);
    if (rc != 0) {
      spdlog::error("ddcutil: Failed to close display handle: {}, {}", ddca_rc_name(rc), ddca_rc_desc(rc));
    }
  }
};

std::unique_ptr<DDCA_Display_Handle, DisplayHandleCloser> open_display(const DDCA_Display_Ref& display_ref) {
  std::unique_ptr<DDCA_Display_Handle, DisplayHandleCloser> display_handle(new DDCA_Display_Handle);
  const DDCA_Status rc = ddca_open_display2(display_ref, false, display_handle.get());
  if (rc != 0) {
    spdlog::error("ddcutil: Failed to open display handle: {}, {}", ddca_rc_name(rc), ddca_rc_desc(rc));
    return {};
  }
  return display_handle;
}

std::string source_to_class(const std::string& source) {
  if (source == "DP1") return "primary";
  else if (source == "DP2") return "secondary";
  else return "error";
}

}

waybar::modules::DdcUtil::DdcUtil(const std::string& id, const Bar& bar,
                                              const Json::Value& config)
    : ALabel(config, "ddcutil", id, "{status}", 5),
      bar_(bar),
      i2c_bus_(config_["bus"].isUInt() ? config_["bus"].asUInt() : -1),
      input_name_(parse_input_names(config)),
      status_("starting") {
  if (i2c_bus_ < 0) {
    throw std::runtime_error("Specify the I2C bus");
  }
  if (input_name_.empty()) {
    throw std::runtime_error("Specify input names");
  }
  // Report DDC/CI errors to stderr.
  ddca_init("--ddc", DDCA_SYSLOG_ERROR, DDCA_INIT_OPTIONS_DISABLE_CONFIG_FILE);
  event_box_.add_events(Gdk::BUTTON_PRESS_MASK);
  event_box_.signal_button_press_event().connect(sigc::mem_fun(*this, &DdcUtil::handleToggle));
  worker();
}

waybar::modules::DdcUtil::~DdcUtil() {
  // TODO if needed
}

std::string waybar::modules::DdcUtil::input_source_name(uint16_t input) const {
  const auto it = input_name_.find(input);
  if (it == input_name_.end()) return "???";
  return it->second;
}

std::string waybar::modules::DdcUtil::get_input_source() const {
  ddca_enable_verify(true);
  const std::string failed = "???";

  const auto display_id = display_id_from_busno(i2c_bus_);
  if (!display_id) return failed;
  // Display refs are pre-allocated and don't need to be freed.
  DDCA_Display_Ref display_ref;
  DDCA_Status rc = ddca_get_display_ref(*display_id, &display_ref);
  if (rc != 0) {
    spdlog::error("ddcutil: Failed to get display ref: {}, {}", ddca_rc_name(rc), ddca_rc_desc(rc));
    return failed;
  }
  const auto display_handle = open_display(display_ref);
  if (!display_handle) return failed;
  DDCA_Vcp_Feature_Code input_source(0x60);
  DDCA_Non_Table_Vcp_Value value;
  rc = ddca_get_non_table_vcp_value(*display_handle, input_source, &value);
  if (rc != 0) {
    spdlog::error("ddcutil: Failed to get input source: {}, {}", ddca_rc_name(rc), ddca_rc_desc(rc));
    return failed;
  }
  return input_source_name(value.sh << 8 | value.sl);
}

std::string waybar::modules::DdcUtil::set_input_source(uint8_t target_input) const {
  ddca_enable_verify(true);
  const std::string failed = "???";

  const auto display_id = display_id_from_busno(i2c_bus_);
  if (!display_id) return failed;
  // Display refs are pre-allocated and don't need to be freed.
  DDCA_Display_Ref display_ref;
  DDCA_Status rc = ddca_get_display_ref(*display_id, &display_ref);
  if (rc != 0) {
    spdlog::error("ddcutil: Failed to get display ref: {}, {}", ddca_rc_name(rc), ddca_rc_desc(rc));
    return failed;
  }
  const auto display_handle = open_display(display_ref);
  if (!display_handle) return failed;
  DDCA_Vcp_Feature_Code input_source(0x60);
  rc = ddca_set_non_table_vcp_value(*display_handle, input_source, 0, target_input);
  if (rc != 0) {
    spdlog::error("ddcutil: Failed to set input source: {}, {}", ddca_rc_name(rc), ddca_rc_desc(rc));
    return failed;
  }
  // Since verify is on, DDC library verified switching worked.
  return input_source_name(target_input);
}

void waybar::modules::DdcUtil::worker() {
  thread_ = [this] {
    const std::string prev_class = source_to_class(status_);
    {
      std::lock_guard<std::mutex> guard(ddc_mutex_);
      status_ = get_input_source();
    }
    const std::string curr_class = source_to_class(status_);
    if (prev_class != curr_class) {
      label_.get_style_context()->remove_class(prev_class);
      label_.get_style_context()->add_class(curr_class);
    }

    dp.emit();
    thread_.sleep_for(interval_);
  };
}

auto waybar::modules::DdcUtil::update() -> void {
  label_.set_markup(fmt::format(fmt::runtime(format_), fmt::arg("status", status_)));
  if (tooltipEnabled()) {
    label_.set_tooltip_text(status_);
  }
}

bool waybar::modules::DdcUtil::handleToggle(GdkEventButton* const& e) {
  if (e->button == 1) {  // left click
    std::lock_guard<std::mutex> guard(ddc_mutex_);

    // Only works to switch an input to itself???
    uint8_t target_input;
    if (status_ == "DP2") target_input = 0x0F;
    else if (status_ == "DP1") target_input = 0x10;
    else {
      spdlog::error("ddcutil: Unknown current input: {}", status_);
      ALabel::handleToggle(e);
      return true;
    }

    const std::string input_source = set_input_source(target_input);
    if (input_source != "???") {
      const std::string prev_class = source_to_class(status_);
      status_ = input_source;
      const std::string curr_class = source_to_class(status_);
      if (prev_class != curr_class) {
        label_.get_style_context()->remove_class(prev_class);
        label_.get_style_context()->add_class(curr_class);
      }
    }
  }
  ALabel::handleToggle(e);
  return true;
}
