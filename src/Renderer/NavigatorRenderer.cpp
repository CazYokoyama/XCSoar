// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "NavigatorRenderer.hpp"
#include "BackendComponents.hpp"
#include "Components.hpp"
#include "Formatter/Units.hpp"
#include "Formatter/UserUnits.hpp"
#include "Interface.hpp"
#include "Look/FontDescription.hpp"
#include "Look/Look.hpp"
#include "ProgressBarRenderer.hpp"
#include "Screen/Layout.hpp"
#include "UIGlobals.hpp"
#include "UnitSymbolRenderer.hpp"
#include "Waypoint/Waypoint.hpp"
#include "WaypointIconRenderer.hpp"
#include "ui/canvas/Canvas.hpp"
#include "util/StaticString.hxx"

#define PERCENT_OF(percent, x) (int)((double)x * percent / 100.0)

static int progress_bar_width;
static int progress_bar_bottom_gap;
static PixelPoint position_waypoint_left;
static PixelPoint position_waypoint_right;

void
NavigatorRenderer::DrawTaskText(
    Canvas &canvas, TaskType tp, [[maybe_unused]] const Waypoint &wp_current,
    const PixelRect &rc, [[maybe_unused]] const NavigatorLook &look_nav,
    [[maybe_unused]] const InfoBoxLook &look_infobox) noexcept
{
  const auto &calculated = CommonInterface::Calculated();
  const int rc_width = rc.GetWidth();
  const int rc_height = rc.GetHeight();

  Font font;
  double ratio_dpi = 1.0 / Layout::vdpi * 100;
  int font_height = PERCENT_OF(30,
			       (rc_height -
				(progress_bar_width +
				 progress_bar_bottom_gap)));
  int icon_radius = PERCENT_OF(5, rc_width);
  int left_end = position_waypoint_left.x + icon_radius;
  int right_start = position_waypoint_right.x - icon_radius;
  int for_waypoint_info = right_start - left_end;

  canvas.SetBackgroundTransparent();
  if (look_nav.inverse)
    canvas.SetTextColor(COLOR_WHITE);
  else
    canvas.SetTextColor(COLOR_BLACK);
  font.Load(FontDescription(Layout::VptScale(font_height * ratio_dpi)));
  canvas.Select(font);

  // e_WP_Distance, i.e. draw the distance to the next waypoint
  static StaticString<20> waypoint_distance_s;
  double waypoint_distance;
  if (tp == TaskType::ORDERED)
    waypoint_distance =
        calculated.ordered_task_stats.current_leg.vector_remaining.distance;
  else
    waypoint_distance =
        calculated.task_stats.current_leg.vector_remaining.distance;
  FormatUserDistance(waypoint_distance, waypoint_distance_s.data(), true,
                     1);
  int text_pixel = (int)font.TextSize(waypoint_distance_s).width;
  if (text_pixel <= for_waypoint_info) {
    PixelSize psSize{for_waypoint_info, rc_height};
    PixelRect prRect{{0, 0}, psSize};
    canvas.DrawClippedText({left_end, 0}, prRect,
			   waypoint_distance_s);
    left_end += text_pixel;
    for_waypoint_info -= text_pixel;
  }

  // e_WP_AltReq
  static StaticString<20> waypoint_altitude_diff_s;
  double waypoint_altitude_diff;
  if (tp == TaskType::ORDERED)
    waypoint_altitude_diff = calculated.ordered_task_stats.current_leg
                                 .solution_remaining.GetRequiredAltitude();
  else
    waypoint_altitude_diff = calculated.task_stats.current_leg
                                 .solution_remaining.GetRequiredAltitude();
  FormatAltitude(waypoint_altitude_diff_s.data(), waypoint_altitude_diff,
                 Units::GetUserAltitudeUnit(), true);
  text_pixel = (int)font.TextSize(waypoint_altitude_diff_s).width;
  if (text_pixel <= for_waypoint_info) {
    PixelSize psSize{text_pixel, rc_height};
    PixelRect prRect{{right_start - text_pixel, 0}, psSize};
    canvas.DrawClippedText({right_start - text_pixel, 0}, prRect,
			   waypoint_altitude_diff_s);
    left_end += text_pixel;
    for_waypoint_info -= text_pixel;
  }

  // ---- Next waypoint's name
  static StaticString<50> waypoint_name_s;
  waypoint_name_s.Format(_T("%s"), wp_current.name.c_str());

  int font_height_waypoint = rc_height -
    (progress_bar_width + progress_bar_bottom_gap) - font_height;
  /* Without PERCENT_OF, font is too big. I don't know why */
  font.Load(FontDescription(Layout::VptScale(PERCENT_OF(75, font_height_waypoint) *
					     ratio_dpi)));
  canvas.Select(font);

  text_pixel = (int)font.TextSize(waypoint_name_s).width;
  left_end = position_waypoint_left.x + icon_radius;
  right_start = position_waypoint_right.x - icon_radius;
  if (text_pixel > right_start - left_end)
    text_pixel = right_start - left_end;
  /* locate the name in center of the navigator pane */
  const int pos_x_text_waypoint{
    left_end + (right_start - left_end) / 2 - text_pixel / 2};
  PixelSize psSize = {text_pixel, font_height_waypoint};
  PixelRect prRect = {{pos_x_text_waypoint, 0}, psSize};
  canvas.DrawClippedText({pos_x_text_waypoint, font_height}, prRect,
      waypoint_name_s);
}

void
NavigatorRenderer::DrawProgressTask(const TaskSummary &summary, Canvas &canvas,
                                    const PixelRect &rc,
                                    const NavigatorLook &look_nav,
                                    const TaskLook &look_task) noexcept
{
  const int rc_height = rc.GetHeight();
  const int rc_width = rc.GetWidth();

  // render the progress bar
  progress_bar_width = PERCENT_OF(10, rc_height);
  progress_bar_bottom_gap = PERCENT_OF(2, rc_height);
  PixelRect r{rc_height * 5 / 24,
	      rc_height - progress_bar_bottom_gap - progress_bar_width,
              rc_width - rc_height * 10 / 48,
	      rc_height - progress_bar_bottom_gap};

  bool task_has_started =
      CommonInterface::Calculated().task_stats.start.HasStarted();
  bool task_is_finished =
      CommonInterface::Calculated().task_stats.task_finished;

  unsigned int progression{};

  if (task_has_started && !task_is_finished)
    progression = 100 * (1 - summary.p_remaining);
  else if (task_has_started && task_is_finished) progression = 100;
  else progression = 0;

  DrawSimpleProgressBar(canvas, r, progression, 0, 100);

  canvas.Select(look_nav.brush_frame);

  // render the waypoints on the progress bar
  const Pen pen_waypoint(Layout::ScalePenWidth(1), COLOR_BLACK);
  const Pen pen_indications(Layout::ScalePenWidth(1),
                            look_nav.inverse ? COLOR_GRAY : COLOR_DARK_GRAY);
  canvas.Select(pen_indications);

  bool target{true};
  unsigned i = 0;
  for (auto it = summary.pts.begin(); it != summary.pts.end(); ++it, ++i) {
    auto p = it->p;

    const PixelPoint position_waypoint(
        p * (rc_width - 10 / 24.0 * rc_height) + 5 / 24.0 * rc_height,
        rc_height - static_cast<int>(1.5 / 24.0 * rc_height));

    int w = Layout::Scale(2);

    /* search for the next Waypoint to reach and draw two horizontal lines
     * left and right if one Waypoint has been missed, the two lines are also
     * drawn
     */
    if (!it->achieved && target) {
      canvas.Select(pen_indications);
      canvas.DrawLine(position_waypoint.At(-w, 0.5 * w),
                      position_waypoint.At(-2 * w, 0.5 * w));
      canvas.DrawLine(position_waypoint.At(w, 0.5 * w),
                      position_waypoint.At(2 * w, 0.5 * w));

      canvas.DrawLine(position_waypoint.At(-w, -0.5 * w),
                      position_waypoint.At(-2 * w, -0.5 * w));
      canvas.DrawLine(position_waypoint.At(w, -0.5 * w),
                      position_waypoint.At(2 * w, -0.5 * w));

      target = false;
    }

    if (i == summary.active) {
      // search for the Waypoint on which the user is looking for and draw
      // two vertical lines left and right
      canvas.Select(pen_indications);
      canvas.DrawLine(position_waypoint.At(-2 * w, w),
                      position_waypoint.At(-2 * w, -w));
      canvas.DrawLine(position_waypoint.At(2 * w, w),
                      position_waypoint.At(2 * w, -w));

      if (it->achieved) canvas.Select(look_task.hbGreen);
      else canvas.Select(look_task.hbOrange);
      w = Layout::Scale(2);
    } else if (i < summary.active) {
      if (it->achieved) canvas.Select(look_task.hbGreen);
      else canvas.Select(look_task.hbNotReachableTerrain);
      w = Layout::Scale(2);
    } else {
      if (it->achieved) canvas.Select(look_task.hbGreen);
      else canvas.Select(look_task.hbLightGray);

      w = Layout::Scale(1);
    }

    canvas.Select(pen_waypoint);
    canvas.DrawRectangle(PixelRect{position_waypoint}.WithMargin(w));
  }
}

void
NavigatorRenderer::DrawWaypointsIconsTitle(
    Canvas &canvas, WaypointPtr waypoint_before, WaypointPtr waypoint_current,
    unsigned task_size, [[maybe_unused]] const NavigatorLook &look) noexcept
{
  const int rc_height = canvas.GetHeight();
  const int rc_width = canvas.GetWidth();

  const WaypointRendererSettings &waypoint_settings =
      CommonInterface::GetMapSettings().waypoint;
  const WaypointLook &waypoint_look = UIGlobals::GetMapLook().waypoint;

  WaypointIconRenderer waypoint_icon_renderer{waypoint_settings, waypoint_look,
                                              canvas};
  /* specify the center of icon */
  position_waypoint_left = PixelPoint(PERCENT_OF(5, rc_width),
				    PERCENT_OF(50, rc_height));
  position_waypoint_right = PixelPoint(rc_width - PERCENT_OF(5, rc_width),
					   PERCENT_OF(50, rc_height));

  // CALCULATE REACHABILITY
  WaypointReachability wr_before{WaypointReachability::UNREACHABLE};
  WaypointReachability wr_current{WaypointReachability::UNREACHABLE};

  auto *protected_task_manager =
      backend_components->protected_task_manager.get();
  if (protected_task_manager != nullptr && task_size > 1) {
    if (waypoint_before != nullptr)
      waypoint_icon_renderer.Draw(*waypoint_before, position_waypoint_left,
                                  wr_before, true);
    if (waypoint_current != nullptr)
      waypoint_icon_renderer.Draw(*waypoint_current, position_waypoint_right,
                                  wr_current, true);
  }
}
