// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "ShowZoomButton.hpp"
#include "Renderer/ButtonRenderer.hpp"
#include "ui/canvas/Canvas.hpp"
#include "ui/canvas/Pen.hpp"
#include "Screen/Layout.hpp"
#include "Input/InputEvents.hpp"
#include "util/Macros.hpp"

#ifdef ENABLE_OPENGL
#include "ui/canvas/opengl/Scope.hpp"
#endif

class ShowZoomOutButtonRenderer : public ButtonRenderer {
public:
  unsigned GetMinimumButtonWidth() const noexcept override {
    return Layout::GetMinimumControlHeight();
  }

  void DrawButton(Canvas &canvas, const PixelRect &rc,
                  ButtonState state) const noexcept override;
};

void
ShowZoomOutButton::Create(ContainerWindow &parent, const PixelRect &rc,
                       WindowStyle style) noexcept
{
  Button::Create(parent, rc, style,
                 std::make_unique<ShowZoomOutButtonRenderer>());
}

bool
ShowZoomOutButton::OnClicked() noexcept
{
  InputEvents::eventZoom(_T("out"));
  return true;
}

void
ShowZoomOutButtonRenderer::DrawButton(Canvas &canvas, const PixelRect &rc,
                                   ButtonState state) const noexcept
{
  const unsigned padding = Layout::GetTextPadding() + Layout::ScalePenWidth(5);

  canvas.Select(Pen(Layout::ScalePenWidth(1), COLOR_BLACK));
  canvas.DrawRoundRectangle({rc.left, rc.top, rc.right - 1, rc.bottom - 1},
                            PixelSize{Layout::VptScale(8u)});

  canvas.Select(Pen(Layout::ScalePenWidth(2), COLOR_BLACK));
  const BulkPixelPoint minus[] = {
    BulkPixelPoint(rc.left + padding, (rc.top + rc.bottom) / 2),
    BulkPixelPoint(rc.right - padding, (rc.top + rc.bottom) / 2),
  };
  canvas.DrawPolyline(minus, ARRAY_SIZE(minus));

  if (state == ButtonState::PRESSED) {
#ifdef ENABLE_OPENGL
    const ScopeAlphaBlend alpha_blend;
    canvas.DrawFilledRectangle(rc, COLOR_YELLOW.WithAlpha(80));
#else
    canvas.InvertRectangle(rc);
#endif
  }
}
