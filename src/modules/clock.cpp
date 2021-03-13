#include "modules/clock.hpp"

#include <time.h>
#include <spdlog/spdlog.h>

#include <sstream>
#include <type_traits>
#include "util/ustring_clen.hpp"
#ifdef HAVE_LANGINFO_1STDAY
#include <langinfo.h>
#include <locale.h>
#endif

using waybar::modules::waybar_time;

template <auto fn>
using deleter_from_fn = std::integral_constant<decltype(fn), fn>;

template <typename T, auto fn>
using deleting_unique_ptr = std::unique_ptr<T, deleter_from_fn<fn>>;

waybar::modules::Clock::Clock(const std::string& id, const Json::Value& config)
    : ALabel(config, "clock", id, "{:%H:%M}", 60, false, false, true), fixed_time_zone_(false) {
  if (config_["timezone"].isString()) {
    spdlog::warn("As using a timezone, some format args may be missing as the date library havn't got a release since 2018.");
    time_zone_ = date::locate_zone(config_["timezone"].asString());
    fixed_time_zone_ = true;
  }

  if (config_["locale"].isString()) {
    locale_ = std::locale(config_["locale"].asString());
  } else {
    locale_ = std::locale("");
  }

  {
    // Load full month names from glibc.  Date format %B does not work for month name.
    deleting_unique_ptr<std::remove_pointer<locale_t>::type, freelocale>
      posix_locale{newlocale(LC_ALL, locale_.name().c_str(), nullptr)};
    if (posix_locale) {
      for (int i = 0; i < months_in_year_; ++i) {
        month_names_[i] = nl_langinfo_l(ALTMON_1 + i, posix_locale.get());
      }
    } else {
      for (int i = 0; i < months_in_year_; ++i) {
        month_names_[i] = nl_langinfo(ALTMON_1 + i);
      }
    }
  }

  thread_ = [this] {
    dp.emit();
    auto now = std::chrono::system_clock::now();
    auto timeout = std::chrono::floor<std::chrono::seconds>(now + interval_);
    auto diff = std::chrono::seconds(timeout.time_since_epoch().count() % interval_.count());
    thread_.sleep_until(timeout - diff);
  };
}

auto waybar::modules::Clock::update() -> void {
  if (!fixed_time_zone_) {
    // Time zone can change. Be sure to pick that.
    time_zone_ = date::current_zone();
  }

  auto        now = std::chrono::system_clock::now();
  waybar_time wtime = {locale_,
                       date::make_zoned(time_zone_, date::floor<std::chrono::seconds>(now))};

  std::string text;
  if (!fixed_time_zone_) {
    // As date dep is not fully compatible, prefer fmt
    tzset();
    auto localtime = fmt::localtime(std::chrono::system_clock::to_time_t(now));
    text = fmt::format(format_, localtime);
    label_.set_markup(text);
  } else {
    text = fmt::format(format_, wtime);
    label_.set_markup(text);
  }

  if (tooltipEnabled()) {
    if (config_["tooltip-format"].isString()) {
      auto       tooltip_format = config_["tooltip-format"].asString();
      const auto cal = calendar(wtime, [this](int month_idx) {
        return std::string(month_names_[month_idx]);
      });
      auto       tooltip_text = fmt::format(tooltip_format, wtime,
        fmt::arg("calendar_header", cal.header), fmt::arg("calendar", cal.text));
      label_.set_tooltip_markup(tooltip_text);
    } else {
      label_.set_tooltip_markup(text);
    }
  }
  // Call parent update
  ALabel::update();
}

bool waybar::modules::Clock::handleScroll(GdkEventScroll *e) {
  // defer to user commands if set
  if (config_["on-scroll-up"].isString() || config_["on-scroll-down"].isString()) {
    return AModule::handleScroll(e);
  }

  auto dir = AModule::getScrollDir(e);
  if (dir != SCROLL_DIR::UP && dir != SCROLL_DIR::DOWN) {
    return true;
  }
  if (!config_["timezones"].isArray() || config_["timezones"].empty()) {
    return true;
  }
  auto nr_zones = config_["timezones"].size();
  if (dir == SCROLL_DIR::UP) {
    size_t new_idx = time_zone_idx_ + 1;
    time_zone_idx_ = new_idx == nr_zones ? 0 : new_idx;
  } else {
    time_zone_idx_ = time_zone_idx_ == 0 ? nr_zones - 1 : time_zone_idx_ - 1;
  }
  auto zone_name = config_["timezones"][time_zone_idx_];
  if (!zone_name.isString() || zone_name.empty()) {
    fixed_time_zone_ = false;
  } else {
    time_zone_ = date::locate_zone(zone_name.asString());
    fixed_time_zone_ = true;
  }

  update();
  return true;
}

template<typename M>
auto waybar::modules::Clock::calendar(const waybar_time& wtime, M&& month_name) -> Calendar {
  const auto daypoint = date::floor<date::days>(wtime.ztime.get_local_time());
  const auto ymd = date::year_month_day(daypoint);
  if (cached_calendar_ymd_ == ymd) {
    return cached_calendar_;
  }
  auto head = header(ymd.year()/ymd.month(), month_name);
  const Calendar calendar{head, calendar_text(ymd)};
  cached_calendar_ymd_ = ymd;
  cached_calendar_ = calendar;
  return calendar;
}

template<typename M>
auto waybar::modules::Clock::header(const date::year_month& ym, M&& month_name) -> std::string const {
  auto year_string = date::format(locale_, "%Y", ym.year());
  return fmt::format("{} {}", year_string, month_name(unsigned(ym.month()) - 1));
}

auto waybar::modules::Clock::calendar_text(const date::year_month_day& ymd) -> std::string const {
  const date::year_month ym = ymd.year()/ymd.month();
  const auto             curr_day = ymd.day();

  std::stringstream os;
  const auto        first_dow = first_day_of_week();
  weekdays_header(first_dow, os);

  // First week prefixed with spaces if needed.
  auto wd = date::weekday(ym / 1);
  auto empty_days = (wd - first_dow).count();
  if (empty_days > 0) {
    os << std::string(empty_days * 3 - 1, ' ');
  }
  auto last_day = (ym / date::literals::last).day();
  for (auto d = date::day(1); d <= last_day; ++d, ++wd) {
    if (wd != first_dow) {
      os << ' ';
    } else if (unsigned(d) != 1) {
      os << '\n';
    }
    if (d == curr_day) {
      if (config_["today-format"].isString()) {
        auto today_format = config_["today-format"].asString();
        os << fmt::format(today_format, date::format("%e", d));
      } else {
        os << "<b><u>" << date::format("%e", d) << "</u></b>";
      }
    } else {
      os << date::format("%e", d);
    }
  }

  auto result = os.str();
  cached_calendar_ymd_ = ymd;
  cached_calendar_.text = result;
  return result;
}

auto waybar::modules::Clock::weekdays_header(const date::weekday& first_dow, std::ostream& os)
    -> void const {
  auto wd = first_dow;
  do {
    if (wd != first_dow) os << ' ';
    Glib::ustring wd_ustring(date::format(locale_, "%a", wd));
    auto clen = ustring_clen(wd_ustring);
    auto wd_len = wd_ustring.length();
    while (clen > 2) {
      wd_ustring = wd_ustring.substr(0, wd_len-1);
      wd_len--;
      clen = ustring_clen(wd_ustring);
    }
    const std::string pad(2 - clen, ' ');
    os << pad << wd_ustring;
  } while (++wd != first_dow);
  os << "\n";
}

// Computations done similarly to Linux cal utility.
auto waybar::modules::Clock::first_day_of_week() -> date::weekday const {
#ifdef HAVE_LANGINFO_1STDAY
  deleting_unique_ptr<std::remove_pointer<locale_t>::type, freelocale> posix_locale{
      newlocale(LC_ALL, locale_.name().c_str(), nullptr)};
  if (posix_locale) {
    const int i = (std::intptr_t)nl_langinfo_l(_NL_TIME_WEEK_1STDAY, posix_locale.get());
    auto      ymd = date::year(i / 10000) / (i / 100 % 100) / (i % 100);
    auto      wd = date::weekday(ymd);
    uint8_t   j = *nl_langinfo_l(_NL_TIME_FIRST_WEEKDAY, posix_locale.get());
    return wd + date::days(j - 1);
  }
#endif
  return date::Sunday;
}

template <>
struct fmt::formatter<waybar_time> : fmt::formatter<std::tm> {
  template <typename FormatContext>
  auto format(const waybar_time& t, FormatContext& ctx) {
    return format_to(ctx.out(), "{}", date::format(t.locale, fmt::to_string(tm_format), t.ztime));
  }
};
