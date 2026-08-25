/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include "plotzoomer.h"
#include <QMouseEvent>
#include <QApplication>
#include <QPainter>
#include <QSettings>
#include <QPen>

#include "qwt_plot_zoomer.h"
#include "qwt_scale_map.h"
#include "qwt_plot.h"
#include "PlotJuggler/svg_util.h"

PlotZoomer::PlotZoomer(QWidget* canvas)
  : QwtPlotZoomer(canvas, false)
  , _mouse_pressed(false)
  , _zoom_enabled(false)
  , _keep_aspect_ratio(false)
  , _x_only_zoom(false)
{
  this->setTrackerMode(AlwaysOff);
}

namespace
{
class XOnlyRubberBand : public QWidget
{
public:
  explicit XOnlyRubberBand(QWidget* parent) : QWidget(parent)
  {
    setAttribute(Qt::WA_TransparentForMouseEvents);
    setAttribute(Qt::WA_NoSystemBackground);
    hide();
  }

  void setPen(const QPen& pen)
  {
    _pen = pen;
    update();
  }

protected:
  void paintEvent(QPaintEvent*) override
  {
    QPainter painter(this);
    painter.fillRect(rect(), QColor(70, 135, 255, 56));
    painter.setBrush(Qt::NoBrush);
    painter.setPen(_pen);
    painter.drawRect(rect().adjusted(0, 0, -1, -1));
  }

private:
  QPen _pen;
};

QMouseEvent xOnlyMouseEvent(const QMouseEvent* event, int y)
{
  const QPointF local_pos(event->pos().x(), y);
  const QPointF window_pos(event->windowPos().x(), event->windowPos().y() + y - event->pos().y());
  const QPointF screen_pos(event->screenPos().x(), event->screenPos().y() + y - event->pos().y());
  return QMouseEvent(event->type(), local_pos, window_pos, screen_pos, event->button(),
                     event->buttons(), event->modifiers(), event->source());
}
}

void PlotZoomer::updateXOnlyBand(bool visible)
{
  if (!_x_only_band)
  {
    _x_only_band = new XOnlyRubberBand(canvas());
  }

  if (!visible)
  {
    const QRect old_rect = _x_only_band->geometry();
    _x_only_band->hide();
    canvas()->repaint(old_rect);
    return;
  }

  auto band = static_cast<XOnlyRubberBand*>(_x_only_band);
  band->setPen(rubberBandPen());

  QRect rect(QPoint(_initial_pos.x(), canvas()->rect().top()),
             QPoint(_current_pos.x(), canvas()->rect().bottom()));
  rect = rect.normalized().intersected(canvas()->rect());
  if (rect.isValid())
  {
    band->setGeometry(rect);
    band->raise();
    band->show();
    band->update();
  }
  else
  {
    band->hide();
  }
}

void PlotZoomer::setXOnlyZoom(bool x_only)
{
  _x_only_zoom = x_only;
}

void PlotZoomer::cancelInteraction()
{
  _mouse_pressed = false;
  if (_zoom_enabled)
  {
    QApplication::restoreOverrideCursor();
    _zoom_enabled = false;
  }
  this->setRubberBand(NoRubberBand);
  this->setTrackerMode(AlwaysOff);
  updateXOnlyBand(false);
  this->updateDisplay();
}

void PlotZoomer::widgetMousePressEvent(QMouseEvent* me)
{
  _mouse_pressed = false;
  auto patterns = this->mousePattern();
  for (QwtEventPattern::MousePattern& pattern : patterns)
  {
    if (this->mouseMatch(pattern, me))
    {
      _mouse_pressed = true;
      // this->setTrackerMode(AlwaysOn);
      _initial_pos = _x_only_zoom ? QPoint(me->pos().x(), canvas()->rect().top()) : me->pos();
      _current_pos = _x_only_zoom ? QPoint(me->pos().x(), canvas()->rect().bottom()) : me->pos();
    }
    break;
  }
  if (_x_only_zoom)
  {
    auto adjusted_event = xOnlyMouseEvent(me, canvas()->rect().top());
    QwtPlotPicker::widgetMousePressEvent(&adjusted_event);
  }
  else
  {
    QwtPlotPicker::widgetMousePressEvent(me);
  }
}

void PlotZoomer::widgetMouseMoveEvent(QMouseEvent* me)
{
  if (_mouse_pressed)
  {
    auto patterns = this->mousePattern();
    for (QwtEventPattern::MousePattern& pattern : patterns)
    {
      QPoint pos = me->pos();
      if (_x_only_zoom)
      {
        pos.setY(canvas()->rect().bottom());
      }
      _current_pos = pos;
      QRect rect(pos, _initial_pos);
      QRectF zoomRect = invTransform(rect.normalized());

      const bool large_enough =
          zoomRect.width() > minZoomSize().width() && zoomRect.height() > minZoomSize().height();
      const bool show_preview = _x_only_zoom || large_enough;

      if (show_preview)
      {
        if (!_zoom_enabled)
        {
          QSettings settings;
          QString theme = settings.value("Preferences::theme", "light").toString();
          const QPixmap& pixmap = LoadSvg(":/resources/svg/zoom_in.svg", theme);
          QCursor zoom_cursor(pixmap.scaled(24, 24));

          _zoom_enabled = true;
          this->setRubberBand(_x_only_zoom ? NoRubberBand : RectRubberBand);
          this->setTrackerMode(AlwaysOff);
          QPen pen(parentWidget()->palette().foreground().color(), 1, Qt::DashLine);
          this->setRubberBandPen(pen);
          QApplication::setOverrideCursor(zoom_cursor);
        }
        if (_x_only_zoom)
        {
          updateXOnlyBand(true);
        }
      }
      else if (_zoom_enabled)
      {
        _zoom_enabled = false;
        this->setRubberBand(NoRubberBand);
        this->updateDisplay();
        QApplication::restoreOverrideCursor();
      }
      break;
    }
  }
  if (_x_only_zoom)
  {
    auto adjusted_event = xOnlyMouseEvent(me, canvas()->rect().bottom());
    QwtPlotPicker::widgetMouseMoveEvent(&adjusted_event);
  }
  else
  {
    QwtPlotPicker::widgetMouseMoveEvent(me);
  }
}

void PlotZoomer::widgetMouseReleaseEvent(QMouseEvent* me)
{
  _mouse_pressed = false;
  _current_pos = me->pos();
  if (_zoom_enabled)
  {
    QApplication::restoreOverrideCursor();
    _zoom_enabled = false;
  }
  if (_x_only_zoom)
  {
    auto adjusted_event = xOnlyMouseEvent(me, canvas()->rect().bottom());
    QwtPlotPicker::widgetMouseReleaseEvent(&adjusted_event);
    this->setRubberBand(NoRubberBand);
    updateXOnlyBand(false);
  }
  else
  {
    QwtPlotPicker::widgetMouseReleaseEvent(me);
  }
  this->setTrackerMode(AlwaysOff);
}

bool PlotZoomer::accept(QPolygon& pa) const
{
  QApplication::restoreOverrideCursor();

  if (pa.count() < 2)
  {
    return false;
  }

  QRect rect = QRect(pa[0], pa[int(pa.count()) - 1]);
  QRectF zoomRect = invTransform(rect.normalized());

  if (_x_only_zoom)
  {
    if (zoomRect.width() < minZoomSize().width())
    {
      return false;
    }
  }
  else if (zoomRect.width() < minZoomSize().width() && zoomRect.height() < minZoomSize().height())
  {
    return false;
  }
  return QwtPlotZoomer::accept(pa);
}

void PlotZoomer::drawRubberBand(QPainter* painter) const
{
  if (_x_only_zoom && _mouse_pressed && rubberBand() != NoRubberBand)
  {
    QRect rect(QPoint(_initial_pos.x(), canvas()->rect().top()),
               QPoint(_current_pos.x(), canvas()->rect().bottom()));
    rect = rect.normalized().intersected(canvas()->rect());
    if (rect.isValid())
    {
      QColor fill(70, 135, 255, 56);
      painter->fillRect(rect, fill);

      painter->save();
      painter->setBrush(Qt::NoBrush);
      painter->setPen(rubberBandPen());
      painter->drawRect(rect.adjusted(0, 0, -1, -1));
      painter->restore();
    }
    return;
  }

  QwtPlotZoomer::drawRubberBand(painter);
}

QRegion PlotZoomer::rubberBandMask() const
{
  if (_x_only_zoom && _mouse_pressed && rubberBand() != NoRubberBand)
  {
    QRect rect(QPoint(_initial_pos.x(), canvas()->rect().top()),
               QPoint(_current_pos.x(), canvas()->rect().bottom()));
    rect = rect.normalized().intersected(canvas()->rect());
    if (rect.isValid())
    {
      return QRegion(rect.adjusted(-2, -2, 2, 2).intersected(canvas()->rect()));
    }
  }

  return QwtPlotZoomer::rubberBandMask();
}

void PlotZoomer::zoom(const QRectF& zoomRect)
{
  QRectF rect = zoomRect;

  if (_x_only_zoom)
  {
    QRectF current_rect = scaleRect();
    rect.setTop(current_rect.top());
    rect.setBottom(current_rect.bottom());
  }
  else if (_keep_aspect_ratio)
  {
    const QRectF cr = canvas()->contentsRect();
    const double canvas_ratio = cr.width() / cr.height();
    const double zoom_ratio = zoomRect.width() / zoomRect.height();

    if (zoom_ratio < canvas_ratio)
    {
      double new_width = zoomRect.height() * canvas_ratio;
      double increment = new_width - zoomRect.width();
      rect.setWidth(new_width);
      rect.moveLeft(rect.left() - 0.5 * increment);
    }
    else
    {
      double new_height = zoomRect.width() / canvas_ratio;
      double increment = new_height - zoomRect.height();
      rect.setHeight(new_height);
      rect.moveTop(rect.top() - 0.5 * increment);
    }
  }
  QwtPlotZoomer::zoom(rect);
}

QSizeF PlotZoomer::minZoomSize() const
{
  if (_x_only_zoom)
  {
    return QSizeF(scaleRect().width() * 0.005, scaleRect().height() * 0.02);
  }
  return QSizeF(scaleRect().width() * 0.02, scaleRect().height() * 0.02);
}
