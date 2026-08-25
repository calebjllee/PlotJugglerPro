/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include "curvelist_view.h"
#include <QApplication>
#include <QDrag>
#include <QFontMetrics>
#include <QMessageBox>
#include <QMimeData>
#include <QPainter>
#include <QDebug>
#include <QScrollBar>
#include "curvelist_panel.h"

namespace
{
QPixmap makeDragLabelPixmap(QWidget* widget, QString text)
{
  const QFont font = QFontDatabase::systemFont(QFontDatabase::GeneralFont);
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

QString dragLabelForSelection(const std::vector<std::string>& selected_names, bool new_x_axis)
{
  if (selected_names.empty())
  {
    return {};
  }
  if (new_x_axis && selected_names.size() == 2)
  {
    return QStringLiteral("%1 vs %2")
        .arg(QString::fromStdString(selected_names[0]), QString::fromStdString(selected_names[1]));
  }
  if (selected_names.size() == 1)
  {
    return QString::fromStdString(selected_names.front());
  }
  return QStringLiteral("%1 + %2 more")
      .arg(QString::fromStdString(selected_names.front()))
      .arg(selected_names.size() - 1);
}
}  // namespace

CurveTableView::CurveTableView(CurveListPanel* parent) : QTableWidget(parent), CurvesView(parent)
{
  setColumnCount(2);
  setEditTriggers(NoEditTriggers);
  setDragEnabled(false);
  setDefaultDropAction(Qt::IgnoreAction);
  setDragDropOverwriteMode(false);
  setDragDropMode(NoDragDrop);
  viewport()->installEventFilter(this);

  setSelectionBehavior(QAbstractItemView::SelectRows);
  setSelectionMode(ExtendedSelection);
  setFocusPolicy(Qt::ClickFocus);

  verticalHeader()->setVisible(false);
  horizontalHeader()->setVisible(false);

  horizontalHeader()->setStretchLastSection(true);

  setColumnWidth(1, 100);

  setHorizontalHeaderItem(0, new QTableWidgetItem("Time series"));
  setHorizontalHeaderItem(1, new QTableWidgetItem("Current value"));

  setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
  setShowGrid(false);
}

void CurveTableView::addItem(const QString& prefix, const QString& plot_name,
                             const QString& plot_ID)
{
  if (_inserted_curves.contains(plot_ID))
  {
    return;
  }

  QString row_name = prefix;
  if (!prefix.isEmpty() && !prefix.endsWith('/'))
  {
    row_name += "/";
  }
  row_name += plot_name;

  auto item = new SortedTableItem<QTableWidgetItem>(plot_name);
  QFont font = QFontDatabase::systemFont(QFontDatabase::GeneralFont);
  font.setPointSize(_point_size);
  item->setFont(font);
  item->setData(Qt::UserRole, plot_ID);
  const int row = rowCount();
  QTableWidget::setRowCount(row + 1);
  QTableWidget::setItem(row, 0, item);

  auto val_cell = new QTableWidgetItem("-");
  val_cell->setTextAlignment(Qt::AlignRight);
  val_cell->setFlags(Qt::NoItemFlags | Qt::ItemIsEnabled);
  font = QFontDatabase::systemFont(QFontDatabase::FixedFont);
  font.setPointSize(_point_size - 2);
  val_cell->setFont(font);

  QTableWidget::setItem(row, 1, val_cell);

  _inserted_curves.insert({ plot_ID });
}

void CurveTableView::refreshColumns()
{
  sortByColumn(0, Qt::AscendingOrder);
  setViewResizeEnabled(true);
}

std::vector<std::string> CurveTableView::getSelectedNames()
{
  std::vector<std::string> non_hidden_list;

  for (const QTableWidgetItem* cell : selectedItems())
  {
    non_hidden_list.push_back(cell->text().toStdString());
  }
  return non_hidden_list;
}

void CurveTableView::refreshFontSize()
{
  if (rowCount() == 0)
  {
    return;
  }
  setViewResizeEnabled(false);

  for (int row = 0; row < rowCount(); row++)
  {
    auto cell = item(row, 0);
    QFont font = QFontDatabase::systemFont(QFontDatabase::GeneralFont);
    font.setPointSize(_point_size);
    cell->setFont(font);

    cell = item(row, 1);
    font = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    font.setPointSize(_point_size - 2);
    cell->setFont(font);
  }
  setViewResizeEnabled(true);
}

void CurveTableView::removeCurve(const QString& name)
{
  for (int row = 0; row < model()->rowCount(); row++)
  {
    if (item(row, 0)->text() == name)
    {
      removeRow(row);
      break;
    }
  }
  _inserted_curves.remove(name);
}

bool CurveTableView::applyVisibilityFilter(const QString& search_string)
{
  setViewResizeEnabled(false);

  bool updated = false;
  _hidden_count = 0;

  QStringList spaced_items = search_string.split(' ');

  for (int row = 0; row < model()->rowCount(); row++)
  {
    auto cell = item(row, 0);
    QString name = cell->text();
    bool toHide = false;

    if (search_string.isEmpty() == false)
    {
      for (const auto& item : spaced_items)
      {
        if (name.contains(item, Qt::CaseInsensitive) == false)
        {
          toHide = true;
          break;
        }
      }
    }
    if (toHide)
    {
      _hidden_count++;
    }

    if (toHide != isRowHidden(row))
    {
      updated = true;
    }

    setRowHidden(row, toHide);
  }

  setViewResizeEnabled(true);
  return updated;
}

void CurveTableView::setViewResizeEnabled(bool enable)
{
  verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
  if (enable)
  {
    horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    horizontalHeader()->setSectionResizeMode(1, QHeaderView::Fixed);
  }
  else
  {
    horizontalHeader()->setSectionResizeMode(0, QHeaderView::Fixed);
    horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
  }
}

void CurveTableView::hideValuesColumn(bool hide)
{
  setViewResizeEnabled(true);
  if (hide)
  {
    hideColumn(1);
  }
  else
  {
    showColumn(1);
  }
}

CurvesView::CurvesView(CurveListPanel* parent) : _parent_panel(parent)
{
}

bool CurvesView::eventFilterBase(QObject* object, QEvent* event)
{
  QAbstractItemView* table_widget = qobject_cast<QAbstractItemView*>(object);

  if (qobject_cast<QScrollBar*>(object))
  {
    return false;
  }

  auto obj = object;
  while (obj && !table_widget)
  {
    obj = obj->parent();
    table_widget = qobject_cast<QAbstractItemView*>(obj);
  }

  bool ctrl_modifier_pressed = (QGuiApplication::keyboardModifiers() == Qt::ControlModifier);

  if (event->type() == QEvent::MouseButtonPress)
  {
    QMouseEvent* mouse_event = static_cast<QMouseEvent*>(event);

    _dragging = false;
    _drag_start_pos = mouse_event->pos();

    if (mouse_event->button() == Qt::LeftButton)
    {
      _newX_modifier = false;
    }
    else if (mouse_event->button() == Qt::RightButton)
    {
      _newX_modifier = true;
    }
    else
    {
      return true;
    }
    return false;
  }
  else if (event->type() == QEvent::MouseMove)
  {
    QMouseEvent* mouse_event = static_cast<QMouseEvent*>(event);
    double distance_from_click = (mouse_event->pos() - _drag_start_pos).manhattanLength();

    if ((mouse_event->buttons() == Qt::LeftButton || mouse_event->buttons() == Qt::RightButton) &&
        distance_from_click >= QApplication::startDragDistance() && !_dragging)
    {
      _dragging = true;
      QDrag* drag = new QDrag(table_widget);
      QMimeData* mimeData = new QMimeData;

      QByteArray mdata;
      QDataStream stream(&mdata, QIODevice::WriteOnly);

      auto selected_names = _parent_panel->getSelectedNames();

      std::sort(selected_names.begin(), selected_names.end());

      for (const auto& curve_name : selected_names)
      {
        stream << QString::fromStdString(curve_name);
      }

      if (!_newX_modifier)
      {
        mimeData->setData("curveslist/add_curve", mdata);
      }
      else
      {
        if (selected_names.size() != 2)
        {
          if (selected_names.size() >= 1)
          {
            QMessageBox::warning(table_widget, "New in version 2.3+",
                                 "To create a new XY curve, you must select two "
                                 "timeseries and "
                                 "drag&drop them using the RIGHT mouse button.",
                                 QMessageBox::Ok);
          }
          return true;
        }
        mimeData->setData("curveslist/new_XY_axis", mdata);
      }

      const QPixmap label_pixmap =
          makeDragLabelPixmap(table_widget, dragLabelForSelection(selected_names, _newX_modifier));
      drag->setPixmap(label_pixmap);
      drag->setHotSpot(QPoint(10, label_pixmap.height() / 2));
      drag->setMimeData(mimeData);
      drag->exec(Qt::CopyAction | Qt::MoveAction);
    }
    return true;
  }
  else if (event->type() == QEvent::Wheel)
  {
    QWheelEvent* wheel_event = dynamic_cast<QWheelEvent*>(event);

    if (ctrl_modifier_pressed)
    {
      int prev_size = _point_size;
      if (wheel_event->delta() < 0)
      {
        _point_size = std::max(8, prev_size - 1);
      }
      else if (wheel_event->delta() > 0)
      {
        _point_size = std::min(14, prev_size + 1);
      }
      if (_point_size != prev_size)
      {
        _parent_panel->changeFontSize(_point_size);
      }
      return true;
    }
  }
  return false;
}
