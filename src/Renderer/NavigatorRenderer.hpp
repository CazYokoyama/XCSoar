// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#pragma once

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
}
