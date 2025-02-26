// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#pragma once

#include "Engine/Waypoint/Ptr.hpp"

struct PixelRect;
struct NavigatorLook;
class Canvas;
struct TaskLook;
struct TaskSummary;

namespace NavigatorRenderer {
/**
* Draw the progress of the current task with presntation of each taskpoint
*/
void
DrawProgressTask(const TaskSummary &summary, Canvas &canvas,
                 const PixelRect &rc, const NavigatorLook &look_nav,
                 const TaskLook &look_task) noexcept;

/**
* Draw the icon of the current task and of the previous task
*/
void
DrawWaypointsIconsTitle(Canvas &canvas, const WaypointPtr waypoint_before,
                        const WaypointPtr waypoint_current, unsigned task_size,
                        const NavigatorLook &look_nav) noexcept;
}
