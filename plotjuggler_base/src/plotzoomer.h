/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#ifndef PLOTZOOMER_H
#define PLOTZOOMER_H

#include <QObject>
#include <QPoint>
#include <QDebug>
#include "qwt_plot_zoomer.h"

class QWidget;

class PlotZoomer : public QwtPlotZoomer
{
public:
  PlotZoomer();

  explicit PlotZoomer(QWidget*);

  virtual ~PlotZoomer() override = default;

  void keepAspectRatio(bool doKeep)
  {
    _keep_aspect_ratio = doKeep;
  }

  void setXOnlyZoom(bool x_only);

  void cancelInteraction();

protected:
  virtual void widgetMousePressEvent(QMouseEvent* event) override;
  virtual void widgetMouseReleaseEvent(QMouseEvent* event) override;
  virtual void widgetMouseMoveEvent(QMouseEvent* event) override;
  virtual bool accept(QPolygon&) const override;
  virtual void drawRubberBand(QPainter* painter) const override;
  virtual QRegion rubberBandMask() const override;

  virtual void zoom(const QRectF& rect) override;

  virtual QSizeF minZoomSize() const override;

private:
  void updateXOnlyBand(bool visible);

  bool _mouse_pressed;
  bool _zoom_enabled;
  bool _keep_aspect_ratio;
  bool _x_only_zoom;
  QPoint _initial_pos;
  QPoint _current_pos;
  QWidget* _x_only_band = nullptr;
};

#endif  // PLOTZOOMER_H
