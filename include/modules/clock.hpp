#pragma once

#include <fmt/format.h>
#if FMT_VERSION < 60000
#include <fmt/time.h>
#else
#include <fmt/chrono.h>
#endif
#include <date/tz.h>
#include "ALabel.hpp"
#include "util/sleeper_thread.hpp"

namespace waybar::modules {

struct waybar_time {
  std::locale locale;
  date::zoned_seconds ztime;
};

class Clock : public ALabel {
 public:
  Clock(const std::string&, const Json::Value&);
  ~Clock() = default;
  auto update() -> void;

 private:
  util::SleeperThread thread_;
  std::locale locale_;
  const date::time_zone* time_zone_;
  bool fixed_time_zone_;
  int time_zone_idx_;
  date::year_month_day cached_calendar_ymd_ = date::January/1/0;
  struct Calendar {
    std::string header;
    std::string text;
  };
  Calendar cached_calendar_;
  static constexpr int months_in_year_ = 12;
  std::array<const char *, months_in_year_> month_names_;

  bool handleScroll(GdkEventScroll* e);

  template<typename M>
  auto calendar(const waybar_time& wtime, M&& month_name) -> Calendar;
  template<typename M>
  auto header(const date::year_month& ym, M&& month_name) -> std::string const;
  auto calendar_text(const date::year_month_day& ymd) -> std::string const;
  auto weekdays_header(const date::weekday& first_dow, std::ostream& os) -> void const;
  auto first_day_of_week() -> date::weekday const;
};

}  // namespace waybar::modules
