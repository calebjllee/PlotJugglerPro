/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#ifndef PLOTLEGEND_H
#define PLOTLEGEND_H

#include <QObject>
#include <QMap>
#include "qwt_plot_legenditem.h"
#include "qwt_plot.h"

class PlotLegend : public QObject, public QwtPlotLegendItem
{
  Q_OBJECT
public:
  PlotLegend(QwtPlot* parent);

  QRectF hideButtonRect() const;

  const QwtPlotItem* processMousePressEvent(QMouseEvent* mouse_event);

  bool startLegendDrag(QMouseEvent* mouse_event, QWidget* source_widget);

  bool hasPendingLegendDrag() const;

  void resetLegendDrag();

  const QwtPlotItem* itemAt(const QPoint& canvas_pos) const;

  bool setHoveredItem(const QwtPlotItem* item);

  const QwtPlotItem* hoveredItem() const;

  virtual QRect geometry(const QRectF& canvasRect) const override;

private:
  struct LayoutData
  {
    QRectF legend_rect;
    QMap<const QwtPlotItem*, QRectF> item_rects;
    qreal split_x = 0.0;
  };

  LayoutData computeLayout(const QRectF& canvas_rect) const;

  virtual void draw(QPainter* p, const QwtScaleMap& xMap, const QwtScaleMap& yMap,
                    const QRectF& rect) const override;

  virtual void drawLegendData(QPainter* painter, const QwtPlotItem*, const QwtLegendData&,
                              const QRectF&) const override;

  virtual void drawBackground(QPainter* painter, const QRectF& rect) const override;

  QwtPlot* _parent_plot;
  bool _collapsed;
  const QwtPlotItem* _hovered_item = nullptr;
  const QwtPlotItem* _pressed_item = nullptr;
  QPoint _press_pos;
};

#endif  // PLOTLEGEND_H
