/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include "plotlegend.h"
#include <QApplication>
#include <QByteArray>
#include <QDataStream>
#include <QDrag>
#include <QEvent>
#include <QMimeData>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QPainter>
#include <QFontMetrics>
#include <QMarginsF>
#include "qwt_legend_data.h"
#include "qwt_graphic.h"
#include "qwt_plot_curve.h"
#include "qwt_text.h"
#include "timeseries_qwt.h"

namespace
{
QString legendDisplayName(QString title, const QwtPlotItem* item = nullptr)
{
  const int slash_pos = title.lastIndexOf('/');
  const int backslash_pos = title.lastIndexOf('\\');
  const int separator_pos = std::max(slash_pos, backslash_pos);
  QString display_name = title;
  if (separator_pos >= 0 && separator_pos + 1 < title.size())
  {
    display_name = title.mid(separator_pos + 1);
  }

  const auto* curve = dynamic_cast<const QwtPlotCurve*>(item);
  const auto* series = curve ? dynamic_cast<const QwtSeriesWrapper*>(curve->data()) : nullptr;
  const QVariant unit_attr = series ? series->plotData()->attribute(PJ::UNIT) : QVariant();
  const QString unit = unit_attr.toString();
  if (!unit.isEmpty())
  {
    display_name += QStringLiteral(" (%1)").arg(unit);
  }
  return display_name;
}

QPixmap makeDragLabelPixmap(QWidget* widget, QString text)
{
  const QFont font = widget->font();
  const QFontMetrics fm(font);
  text = fm.elidedText(text, Qt::ElideMiddle, 260);

  const int margin_x = 10;
  const int margin_y = 5;
  const QSize size(fm.horizontalAdvance(text) + margin_x * 2, fm.height() + margin_y * 2);
  QPixmap pixmap(size);
  pixmap.fill(Qt::transparent);

  QPainter painter(&pixmap);
  painter.setRenderHint(QPainter::Antialiasing, true);
  painter.setFont(font);
  painter.setPen(widget->palette().foreground().color());
  painter.setBrush(widget->palette().base().color());
  painter.drawRoundedRect(pixmap.rect().adjusted(0, 0, -1, -1), 6, 6);
  painter.drawText(pixmap.rect().adjusted(margin_x, margin_y, -margin_x, -margin_y),
                   Qt::AlignVCenter | Qt::AlignLeft, text);
  return pixmap;
}

QwtText legendDisplayText(const QwtText& text, const QwtPlotItem* item)
{
  QwtText display_text(text);
  display_text.setText(legendDisplayName(text.text(), item));
  return display_text;
}
}  // namespace

PlotLegend::PlotLegend(QwtPlot* parent) : _parent_plot(parent), _collapsed(false)
{
  setRenderHint(QwtPlotItem::RenderAntialiased);

  setMaxColumns(1);
  setAlignmentInCanvas(Qt::Alignment(Qt::AlignTop | Qt::AlignRight));
  setBackgroundMode(QwtPlotLegendItem::BackgroundMode::LegendBackground);
  ;
  setBorderRadius(0);

  setMargin(2);
  setSpacing(1);
  setItemMargin(2);

  QFont font = this->font();
  font.setPointSize(9);
  setFont(font);
  setVisible(true);

  this->attach(parent);
}

PlotLegend::LayoutData PlotLegend::computeLayout(const QRectF& canvas_rect) const
{
  LayoutData layout;

  std::vector<const QwtPlotItem*> left_items;
  std::vector<const QwtPlotItem*> right_items;

  QFontMetrics fm(font());
  int left_text_width = 0;
  int right_text_width = 0;

  for (auto item : plotItems())
  {
    QwtAxisId axis_side = QwtPlot::yLeft;
    if (auto curve = dynamic_cast<const QwtPlotCurve*>(item))
    {
      axis_side = curve->yAxis();
    }

    const int title_width = fm.horizontalAdvance(legendDisplayName(item->title().text(), item));
    if (axis_side == QwtPlot::yRight)
    {
      right_items.push_back(item);
      right_text_width = std::max(right_text_width, title_width);
    }
    else
    {
      left_items.push_back(item);
      left_text_width = std::max(left_text_width, title_width);
    }
  }

  const int row_count = std::max(left_items.size(), right_items.size());
  const int row_height = fm.height() + (2 * itemMargin());
  const int row_spacing = itemSpacing();
  const int side_padding = 18;
  const int split_gap = 10;

  const int left_width = left_text_width + side_padding;
  const int right_width = right_text_width + side_padding;
  const bool has_left = !left_items.empty();
  const bool has_right = !right_items.empty();

  int body_width = 0;
  if (has_left && has_right)
  {
    body_width = left_width + split_gap + right_width;
  }
  else if (has_left)
  {
    body_width = left_width;
  }
  else if (has_right)
  {
    body_width = right_width;
  }
  else
  {
    body_width = left_width;
  }

  const int width = (2 * margin()) + body_width;
  const int height = (2 * margin()) +
                     ((row_count > 0) ? ((row_count * row_height) + ((row_count - 1) * row_spacing))
                                      : row_height);

  qreal x = canvas_rect.left() + offsetInCanvas(Qt::Horizontal);
  qreal y = canvas_rect.top() + offsetInCanvas(Qt::Vertical);

  auto align = alignmentInCanvas();
  if (align & Qt::AlignRight)
  {
    x = canvas_rect.right() - width - offsetInCanvas(Qt::Horizontal);
  }
  else if (align & Qt::AlignHCenter)
  {
    x = canvas_rect.center().x() - (width * 0.5);
  }

  if (align & Qt::AlignBottom)
  {
    y = canvas_rect.bottom() - height - offsetInCanvas(Qt::Vertical);
  }
  else if (align & Qt::AlignVCenter)
  {
    y = canvas_rect.center().y() - (height * 0.5);
  }

  layout.legend_rect = QRectF(x, y, width, height);

  const qreal base_x = layout.legend_rect.left() + margin();
  const qreal left_x = base_x;
  const qreal right_x = (has_left && has_right) ? (left_x + left_width + split_gap) : base_x;
  const qreal row0_y = layout.legend_rect.top() + margin();

  for (size_t i = 0; i < left_items.size(); i++)
  {
    const qreal row_y = row0_y + i * (row_height + row_spacing);
    layout.item_rects[left_items[i]] = QRectF(left_x, row_y, left_width, row_height);
  }

  for (size_t i = 0; i < right_items.size(); i++)
  {
    const qreal row_y = row0_y + i * (row_height + row_spacing);
    layout.item_rects[right_items[i]] = QRectF(right_x, row_y, right_width, row_height);
  }

  if (has_left && has_right)
  {
    layout.split_x = left_x + left_width + (split_gap * 0.5);
  }
  else
  {
    layout.split_x = -1.0;
  }
  return layout;
}

QRect PlotLegend::geometry(const QRectF& canvasRect) const
{
  return computeLayout(canvasRect).legend_rect.toRect();
}

QRectF PlotLegend::hideButtonRect() const
{
  const int s = 5;
  auto canvas_rect = _parent_plot->canvas()->rect();
  if (alignmentInCanvas() & Qt::AlignRight)
  {
    return QRectF(geometry(canvas_rect).topRight() + QPoint(-s, -s), QSize(s * 2, s * 2));
  }
  return QRectF(geometry(canvas_rect).topLeft() + QPoint(-s, -s), QSize(s * 2, s * 2));
}

void PlotLegend::draw(QPainter* painter, const QwtScaleMap& xMap, const QwtScaleMap& yMap,
                      const QRectF& rect) const
{
  LayoutData layout = computeLayout(rect);

  if (!_collapsed)
  {
    drawBackground(painter, layout.legend_rect);

    if (layout.split_x >= 0.0)
    {
      painter->save();
      QPen split_pen = textPen();
      split_pen.setColor(_parent_plot->canvas()->palette().foreground().color());
      split_pen.setStyle(Qt::DotLine);
      painter->setPen(split_pen);
      painter->drawLine(QPointF(layout.split_x, layout.legend_rect.top() + margin()),
                        QPointF(layout.split_x, layout.legend_rect.bottom() - margin()));
      painter->restore();
    }

    for (auto item : plotItems())
    {
      auto rect_it = layout.item_rects.find(item);
      if (rect_it == layout.item_rects.end())
      {
        continue;
      }

      QRectF cell_rect = rect_it.value();

      if (item == _hovered_item)
      {
        painter->save();
        QColor hover_color = _parent_plot->canvas()->palette().highlight().color();
        hover_color.setAlpha(45);
        painter->setPen(Qt::NoPen);
        painter->setBrush(hover_color);
        painter->drawRect(cell_rect);
        painter->restore();
      }

      auto legend_data = item->legendData();
      if (legend_data.empty())
      {
        continue;
      }

      const QwtLegendData& data = legend_data.front();
      QRectF inner_rect = cell_rect.adjusted(itemMargin(), itemMargin(), -itemMargin(), -itemMargin());

      int titleOff = 0;
      const QwtGraphic graphic = data.icon();
      if (!graphic.isEmpty())
      {
        QRectF icon_rect(inner_rect.topLeft(), graphic.defaultSize());
        icon_rect.moveCenter(QPoint(icon_rect.center().x(), inner_rect.center().y()));
        if (item->isVisible())
        {
          graphic.render(painter, icon_rect, Qt::KeepAspectRatio);
        }
        titleOff += icon_rect.width() + spacing();
      }

      const QwtText text = legendDisplayText(data.title(), item);
      if (!text.isEmpty())
      {
        painter->save();
        auto pen = textPen();
        if (!item->isVisible())
        {
          pen.setColor(QColor(122, 122, 122));
        }
        else
        {
          pen.setColor(_parent_plot->canvas()->palette().foreground().color());
        }
        painter->setPen(pen);
        painter->setFont(font());
        const QRectF text_rect = inner_rect.adjusted(titleOff + 2, 0, -2, 0);
        text.draw(painter, text_rect);
        painter->restore();
      }
    }
  }

  QRectF iconRect = hideButtonRect();

  if (isVisible() && plotItems().size() > 0)
  {
    painter->save();

    QColor col = _parent_plot->canvas()->palette().foreground().color();
    painter->setPen(col);
    painter->setBrush(QBrush(Qt::white, Qt::SolidPattern));
    painter->drawEllipse(iconRect);

    if (_collapsed)
    {
      iconRect -= QMarginsF(3, 3, 3, 3);
      painter->setBrush(QBrush(col, Qt::SolidPattern));
      painter->drawEllipse(iconRect);
    }

    painter->restore();
  }
}

void PlotLegend::drawLegendData(QPainter* painter, const QwtPlotItem* plotItem,
                                const QwtLegendData& data, const QRectF& rect) const
{
  const int m = margin();
  const QRectF r = rect.toRect().adjusted(m, m, -m, -m);

  painter->setClipRect(r, Qt::IntersectClip);

  const qreal split_x = r.center().x();
  const QRectF left_rect(r.left(), r.top(), split_x - r.left(), r.height());
  const QRectF right_rect(split_x, r.top(), r.right() - split_x, r.height());

  QwtAxisId axis_side = QwtAxis::YLeft;
  if (auto curve = dynamic_cast<const QwtPlotCurve*>(plotItem))
  {
    axis_side = curve->yAxis();
  }

  const QRectF active_rect = (axis_side == QwtPlot::yRight) ? right_rect : left_rect;

  painter->save();
  QPen split_pen = textPen();
  split_pen.setColor(_parent_plot->canvas()->palette().foreground().color());
  split_pen.setStyle(Qt::DotLine);
  painter->setPen(split_pen);
  painter->drawLine(QPointF(split_x, r.top()), QPointF(split_x, r.bottom()));

  if (plotItem == _hovered_item)
  {
    QColor hover_color = _parent_plot->canvas()->palette().highlight().color();
    hover_color.setAlpha(45);
    painter->setPen(Qt::NoPen);
    painter->setBrush(hover_color);
    painter->drawRect(active_rect);
  }
  painter->restore();

  int titleOff = 0;

  const QwtGraphic graphic = data.icon();
  if (!graphic.isEmpty())
  {
    QRectF iconRect(active_rect.topLeft(), graphic.defaultSize());

    iconRect.moveCenter(QPoint(iconRect.center().x(), rect.center().y()));

    if (plotItem->isVisible())
    {
      graphic.render(painter, iconRect, Qt::KeepAspectRatio);
    }

    titleOff += iconRect.width() + spacing();
  }

  const QwtText text = legendDisplayText(data.title(), plotItem);
  if (!text.isEmpty())
  {
    auto pen = textPen();
    if (!plotItem->isVisible())
    {
      pen.setColor(QColor(122, 122, 122));
    }
    else
    {
      pen.setColor(_parent_plot->canvas()->palette().foreground().color());
    }
    painter->setPen(pen);
    painter->setFont(font());

    const QRectF textRect = active_rect.adjusted(titleOff + 2, 0, -2, 0);
    text.draw(painter, textRect);
  }
}

void PlotLegend::drawBackground(QPainter* painter, const QRectF& rect) const
{
  painter->save();

  auto pen = textPen();
  pen.setColor(_parent_plot->canvas()->palette().foreground().color());

  painter->setPen(pen);
  painter->setBrush(backgroundBrush());
  const double radius = borderRadius();
  painter->drawRoundedRect(rect, radius, radius);

  painter->restore();
}

const QwtPlotItem* PlotLegend::processMousePressEvent(QMouseEvent* mouse_event)
{
  auto canvas_rect = _parent_plot->canvas()->rect();
  LayoutData layout = computeLayout(canvas_rect);
  const QPoint press_point = mouse_event->pos();

  _pressed_item = nullptr;
  _press_pos = QPoint();

  if (isVisible() && mouse_event->modifiers() == Qt::NoModifier)
  {
    if ((hideButtonRect() + QMargins(2, 2, 2, 2)).contains(press_point))
    {
      _collapsed = !_collapsed;
      _parent_plot->replot();
      return nullptr;
    }

    if (!_collapsed && layout.legend_rect.contains(press_point))
    {
      for (auto it = layout.item_rects.begin(); it != layout.item_rects.end(); ++it)
      {
        if (it.value().contains(press_point))
        {
          _pressed_item = it.key();
          _press_pos = press_point;
          return _pressed_item;
        }
      }
    }
  }
  return nullptr;
}

bool PlotLegend::startLegendDrag(QMouseEvent* mouse_event, QWidget* source_widget)
{
  if (_pressed_item == nullptr || mouse_event->buttons() != Qt::LeftButton)
  {
    return false;
  }

  const auto* curve_item = dynamic_cast<const QwtPlotCurve*>(_pressed_item);
  if (!curve_item)
  {
    resetLegendDrag();
    return false;
  }

  const QPoint delta = mouse_event->pos() - _press_pos;
  if (delta.manhattanLength() < QApplication::startDragDistance())
  {
    return false;
  }

  QDrag* drag = new QDrag(source_widget);
  auto* mimeData = new QMimeData;
  QByteArray mdata;
  QDataStream stream(&mdata, QIODevice::WriteOnly);

  const QString curve_name = curve_item->title().text();
  stream << curve_name;
  mimeData->setData("curveslist/add_curve", mdata);
  const QPixmap label_pixmap = makeDragLabelPixmap(source_widget, legendDisplayName(curve_name));
  drag->setPixmap(label_pixmap);
  drag->setHotSpot(QPoint(10, label_pixmap.height() / 2));
  drag->setMimeData(mimeData);
  drag->exec(Qt::CopyAction | Qt::MoveAction);

  resetLegendDrag();
  return true;
}

bool PlotLegend::hasPendingLegendDrag() const
{
  return _pressed_item != nullptr;
}

void PlotLegend::resetLegendDrag()
{
  _pressed_item = nullptr;
  _press_pos = QPoint();
}

const QwtPlotItem* PlotLegend::itemAt(const QPoint& canvas_pos) const
{
  if (!isVisible())
  {
    return nullptr;
  }

  auto canvas_rect = _parent_plot->canvas()->rect();
  LayoutData layout = computeLayout(canvas_rect);
  if (!_collapsed && layout.legend_rect.contains(canvas_pos))
  {
    for (auto it = layout.item_rects.begin(); it != layout.item_rects.end(); ++it)
    {
      if (it.value().contains(canvas_pos))
      {
        return it.key();
      }
    }
  }
  return nullptr;
}

bool PlotLegend::setHoveredItem(const QwtPlotItem* item)
{
  if (_hovered_item == item)
  {
    return false;
  }
  _hovered_item = item;
  return true;
}

const QwtPlotItem* PlotLegend::hoveredItem() const
{
  return _hovered_item;
}
