/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include "plot_docker.h"
#include "PlotJuggler/save_plot.h"
#include "plotwidget_editor.h"
#include "Qads/DockSplitter.h"
#include <QPushButton>
#include <QBoxLayout>
#include <QMouseEvent>
#include <QSplitter>
#include <QDebug>
#include <QInputDialog>
#include <QLineEdit>
#include <vector>
#include "PlotJuggler/svg_util.h"

class SplittableComponentsFactory : public ads::CDockComponentsFactory
{
public:
  ads::CDockAreaTitleBar* createDockAreaTitleBar(ads::CDockAreaWidget* dock_area) const override
  {
    auto title_bar = new ads::CDockAreaTitleBar(dock_area);
    title_bar->setVisible(false);
    return title_bar;
  }
};

namespace
{
void collectPlotWidgets(QWidget* widget, std::vector<PlotWidget*>& plots)
{
  if (auto splitter = qobject_cast<QSplitter*>(widget))
  {
    for (int i = 0; i < splitter->count(); i++)
    {
      collectPlotWidgets(splitter->widget(i), plots);
    }
    return;
  }

  if (auto dock_area = qobject_cast<ads::CDockAreaWidget*>(widget))
  {
    if (auto dock_widget = dynamic_cast<DockWidget*>(dock_area->currentDockWidget()))
    {
      if (auto plot_widget = dock_widget->plotWidget())
      {
        plots.push_back(plot_widget);
      }
    }
  }
}

void applySharedTimeAxes(QWidget* widget)
{
  auto splitter = qobject_cast<QSplitter*>(widget);
  if (!splitter)
  {
    return;
  }

  if (splitter->orientation() == Qt::Vertical)
  {
    std::vector<PlotWidget*> stacked_plots;
    collectPlotWidgets(splitter, stacked_plots);

    double left_extent = 0.0;
    double right_extent = 0.0;
    for (auto plot : stacked_plots)
    {
      left_extent = std::max(left_extent, plot->yAxisExtent(QwtPlot::yLeft));
      right_extent = std::max(right_extent, plot->yAxisExtent(QwtPlot::yRight));
    }

    for (auto plot : stacked_plots)
    {
      plot->setYAxisMinimumExtent(QwtPlot::yLeft, left_extent);
      plot->setYAxisMinimumExtent(QwtPlot::yRight, right_extent);
    }

    for (size_t i = 0; i + 1 < stacked_plots.size(); i++)
    {
      stacked_plots[i]->setBottomAxisVisible(false);
    }
  }

  for (int i = 0; i < splitter->count(); i++)
  {
    applySharedTimeAxes(splitter->widget(i));
  }
}
}  // namespace

PlotDocker::PlotDocker(QString name, PlotDataMapRef& datamap, QWidget* parent)
  : ads::CDockManager(parent), _name(name), _datamap(datamap)
{
  ads::CDockComponentsFactory::setFactory(new SplittableComponentsFactory());

  auto CreateFirstWidget = [&]() {
    if (dockAreaCount() == 0)
    {
      DockWidget* widget = new DockWidget(datamap, this);

      auto area = addDockWidget(ads::TopDockWidgetArea, widget);
      area->setAllowedAreas(ads::OuterDockAreas);

      registerPlotWidget(widget->plotWidget());

      connect(widget, &DockWidget::undoableChange, this, &PlotDocker::undoableChange);
      refreshSharedTimeAxes();
    }
  };

  connect(this, &ads::CDockManager::dockWidgetRemoved, this, CreateFirstWidget);

  connect(this, &ads::CDockManager::dockAreasAdded, this, &PlotDocker::undoableChange);

  CreateFirstWidget();
}

PlotDocker::~PlotDocker()
{
}

QString PlotDocker::name() const
{
  return _name;
}

void PlotDocker::setName(QString name)
{
  _name = name;
}

QDomElement saveChildNodesState(QDomDocument& doc, QWidget* widget)
{
  QSplitter* splitter = qobject_cast<QSplitter*>(widget);
  if (splitter)
  {
    QDomElement splitter_elem = doc.createElement("DockSplitter");
    splitter_elem.setAttribute("orientation",
                               (splitter->orientation() == Qt::Horizontal) ? "|" : "-");
    splitter_elem.setAttribute("count", QString::number(splitter->count()));

    QString sizes_str;
    int total_size = 0;
    for (int size : splitter->sizes())
    {
      total_size += size;
    }
    for (int size : splitter->sizes())
    {
      sizes_str += QString::number(double(size) / double(total_size));
      sizes_str += ";";
    }
    sizes_str.resize(sizes_str.size() - 1);
    splitter_elem.setAttribute("sizes", sizes_str);

    for (int i = 0; i < splitter->count(); ++i)
    {
      auto child = saveChildNodesState(doc, splitter->widget(i));
      splitter_elem.appendChild(child);
    }
    return splitter_elem;
  }
  else
  {
    ads::CDockAreaWidget* dockArea = qobject_cast<ads::CDockAreaWidget*>(widget);
    if (dockArea)
    {
      QDomElement area_elem = doc.createElement("DockArea");
      for (int i = 0; i < dockArea->dockWidgetsCount(); ++i)
      {
        auto dock_widget = dynamic_cast<DockWidget*>(dockArea->dockWidget(i));
        if (dock_widget)
        {
          if (auto plot_widget = dock_widget->plotWidget())
          {
            auto plotwidget_elem = plot_widget->xmlSaveState(doc);
            area_elem.appendChild(plotwidget_elem);
          }
          else if (auto map_panel = dock_widget->mapPanel())
          {
            area_elem.appendChild(map_panel->xmlSaveState(doc));
          }
          area_elem.setAttribute("name", dock_widget->toolBar()->label()->text());
        }
      }
      return area_elem;
    }
  }
  return {};
}

QDomElement PlotDocker::xmlSaveState(QDomDocument& doc) const
{
  QDomElement containers_elem = doc.createElement("Tab");

  containers_elem.setAttribute("containers", dockContainers().count());

  for (CDockContainerWidget* container : dockContainers())
  {
    QDomElement elem = doc.createElement("Container");
    auto child = saveChildNodesState(doc, container->rootSplitter());
    elem.appendChild(child);
    containers_elem.appendChild(elem);
  }
  return containers_elem;
}

void PlotDocker::restoreSplitter(QDomElement elem, DockWidget* widget)
{
  QString orientation_str = elem.attribute("orientation");
  int splitter_count = elem.attribute("count").toInt();

  // Check if the orientation string is right
  if (!orientation_str.startsWith("|") && !orientation_str.startsWith("-"))
  {
    return;
  }

  Qt::Orientation orientation = orientation_str.startsWith("|") ? Qt::Horizontal : Qt::Vertical;

  std::vector<DockWidget*> widgets(splitter_count);

  widgets[0] = widget;
  for (int i = 1; i < splitter_count; i++)
  {
    widget = (orientation == Qt::Horizontal) ? widget->splitHorizontalLocal() : widget->splitVertical();
    widgets[i] = widget;
  }

  int tot_size = 0;

  for (int i = 0; i < splitter_count; i++)
  {
    tot_size += (orientation == Qt::Horizontal) ? widgets[i]->width() : widgets[i]->height();
  }

  auto sizes_str = elem.attribute("sizes").split(";");
  QList<int> sizes;

  for (int i = 0; i < splitter_count; i++)
  {
    sizes.push_back(static_cast<int>(sizes_str[i].toDouble() * tot_size));
  }

  auto splitter = ads::internal::findParent<ads::CDockSplitter*>(widget);
  splitter->setSizes(sizes);

  int index = 0;

  QDomElement child_elem = elem.firstChildElement();
  while (child_elem.isNull() == false)
  {
    if (child_elem.tagName() == "DockArea")
    {
      auto plot_elem = child_elem.firstChildElement("plot");
      if (!plot_elem.isNull() && widgets[index]->plotWidget())
      {
        widgets[index]->plotWidget()->xmlLoadState(plot_elem);
      }
      auto map_elem = child_elem.firstChildElement("map_panel");
      if (!map_elem.isNull())
      {
        widgets[index]->convertToMapPanel();
        if (widgets[index]->mapPanel())
        {
          widgets[index]->mapPanel()->xmlLoadState(map_elem);
        }
      }
      if (child_elem.hasAttribute("name"))
      {
        QString area_name = child_elem.attribute("name");
        widgets[index]->toolBar()->label()->setText(area_name);
      }
      index++;
    }
    else if (child_elem.tagName() == "DockSplitter")
    {
      restoreSplitter(child_elem, widgets[index++]);
    }
    else
    {
      return;
    }

    child_elem = child_elem.nextSiblingElement();
  }
};

bool PlotDocker::xmlLoadState(QDomElement& tab_element)
{
  if (!isHidden())
  {
    hide();
  }

  for (auto container_elem = tab_element.firstChildElement("Container"); !container_elem.isNull();
       container_elem = container_elem.nextSiblingElement("Cont"
                                                          "aine"
                                                          "r"))
  {
    auto splitter_elem = container_elem.firstChildElement("DockSplitter");
    if (!splitter_elem.isNull())
    {
      auto widget = dynamic_cast<DockWidget*>(dockArea(0)->currentDockWidget());
      restoreSplitter(splitter_elem, widget);
    }
  }

  if (isHidden())
  {
    show();
  }
  refreshSharedTimeAxes();
  return true;
}

int PlotDocker::plotCount() const
{
  return dockAreaCount();
}

void PlotDocker::registerPlotWidget(PlotWidget* plot_widget)
{
  if (!plot_widget)
  {
    return;
  }

  connect(plot_widget, &PlotWidget::rectChanged, this,
          [this](PlotWidget*, QRectF) { refreshSharedTimeAxes(); });
  emit plotWidgetAdded(plot_widget);
}

PlotWidget* PlotDocker::plotAt(int index)
{
  DockWidget* dock_widget = dynamic_cast<DockWidget*>(dockArea(index)->currentDockWidget());
  return dock_widget ? dock_widget->plotWidget() : nullptr;
}

MapDockPanel* PlotDocker::mapPanelAt(int index)
{
  DockWidget* dock_widget = dynamic_cast<DockWidget*>(dockArea(index)->currentDockWidget());
  return dock_widget ? dock_widget->mapPanel() : nullptr;
}

void PlotDocker::setHorizontalLink(bool enabled)
{
  // TODO
}

void PlotDocker::zoomOut()
{
  for (int index = 0; index < plotCount(); index++)
  {
    if (auto plot = plotAt(index))
    {
      plot->zoomOut(false);  // TODO is it false?
    }
  }
}

void PlotDocker::replot()
{
  for (int index = 0; index < plotCount(); index++)
  {
    if (auto plot = plotAt(index))
    {
      plot->replot();
    }
  }
}

void PlotDocker::refreshSharedTimeAxes()
{
  for (auto container : dockContainers())
  {
    std::vector<PlotWidget*> plots;
    collectPlotWidgets(container->rootSplitter(), plots);
    for (auto plot : plots)
    {
      plot->setBottomAxisVisible(true);
      plot->setYAxisMinimumExtent(QwtPlot::yLeft, 0.0);
      plot->setYAxisMinimumExtent(QwtPlot::yRight, 0.0);
    }
  }

  for (auto container : dockContainers())
  {
    applySharedTimeAxes(container->rootSplitter());
  }
}

void PlotDocker::on_stylesheetChanged(QString theme)
{
  for (int index = 0; index < plotCount(); index++)
  {
    auto dock_widget = static_cast<DockWidget*>(dockArea(index)->currentDockWidget());
    dock_widget->toolBar()->on_stylesheetChanged(theme);
  }
}

QRect PlotDocker::plotRelativeFootprint(int index, QSize plot_size) const
{
  const auto factor_x = static_cast<float>(plot_size.width()) / static_cast<float>(rect().width());
  const auto factor_y =
      static_cast<float>(plot_size.height()) / static_cast<float>(rect().height());

  const auto* dock_area = dockArea(index);
  const auto plot_pos = mapFromGlobal(
      dock_area->currentDockWidget()->mapToGlobal(dock_area->currentDockWidget()->pos()));
  const auto plot_rect = dock_area->currentDockWidget()->rect();

  const static float title_margin = 10.f;
  const static float plot_margin = 5.f;
  const auto x = (static_cast<float>(plot_pos.x()) * factor_x) + plot_margin;
  const auto y = (static_cast<float>(plot_pos.y()) * factor_y) + title_margin + plot_margin;
  const auto w = (static_cast<float>(plot_rect.width()) * factor_x) - 2 * plot_margin;
  const auto h =
      (static_cast<float>(plot_rect.height()) * factor_y) - title_margin - 2 * plot_margin;

  return { static_cast<int>(x), static_cast<int>(y), static_cast<int>(w), static_cast<int>(h) };
}

void PlotDocker::savePlotsToFile()
{
  const auto plot_size = plotSize();
  PlotSaveHelper save_plots_helper(plot_size, this);

  for (int index = 0; index < plotCount(); index++)
  {
    const auto* dock_area = dockArea(index);

    const auto plot_footprint = plotRelativeFootprint(index, plot_size);
    auto* plot_at = plotAt(index);
    if (plot_at)
    {
      plot_at->plotOn(save_plots_helper, plot_footprint);
    }

    const static float title_margin = 10.f;
    const auto title_footprint =
        QRectF{ static_cast<qreal>(plot_footprint.x()), plot_footprint.y() - title_margin,
                static_cast<qreal>(plot_footprint.width()), title_margin };
    save_plots_helper.paintTitle(
        static_cast<const DockWidget*>(dock_area->currentDockWidget())->name(), title_footprint,
        this);
  }
}

DockWidget::DockWidget(PlotDataMapRef& datamap, QWidget* parent)
  : ads::CDockWidget("Plot", parent), _datamap(datamap)
{
  setFrameShape(QFrame::NoFrame);

  static int plot_count = 0;
  QString plot_name = QString("_plot_%1_").arg(plot_count++);
  _plot_widget = new PlotWidget(datamap, this);
  setWidget(_plot_widget);
  setFeature(ads::CDockWidget::DockWidgetFloatable, false);
  setFeature(ads::CDockWidget::DockWidgetDeleteOnClose, true);

  _toolbar = new DockToolbar(this);
  _toolbar->label()->setText("...");
  qobject_cast<QBoxLayout*>(layout())->insertWidget(0, _toolbar);

  connect(_toolbar->buttonSplitHorizontal(), &QPushButton::clicked, this,
          &DockWidget::splitHorizontal);

  connect(_toolbar->buttonSplitVertical(), &QPushButton::clicked, this, &DockWidget::splitVertical);

  connect(_toolbar, &DockToolbar::backgroundColorRequest, _plot_widget,
          &PlotWidget::onBackgroundColorRequest);

  connect(_plot_widget, &PlotWidget::splitHorizontal, this, &DockWidget::splitHorizontal);

  connect(_plot_widget, &PlotWidget::splitVertical, this, &DockWidget::splitVertical);

  connect(_plot_widget, &PlotWidget::createMapSplitRequested, this,
          &DockWidget::createMapPanelSplit);

  connect(_plot_widget, &PlotWidget::convertToMapPanelRequested, this,
          &DockWidget::convertToMapPanel);

  connect(_toolbar, &DockToolbar::titleChanged, _plot_widget,
          [this](QString title) { _plot_widget->setStatisticsTitle(title); });

  auto FullscreenAction = [this]() {
    PlotDocker* parent_docker = static_cast<PlotDocker*>(dockManager());

    this->toolBar()->toggleFullscreen();
    bool fullscreen = this->toolBar()->isFullscreen();

    for (int i = 0; i < parent_docker->dockAreaCount(); i++)
    {
      auto area = parent_docker->dockArea(i);
      if (area != dockAreaWidget())
      {
        area->setVisible(!fullscreen);
      }
      this->toolBar()->buttonClose()->setHidden(fullscreen);
    }
  };

  QObject::connect(_toolbar->buttonFullscreen(), &QPushButton::clicked, FullscreenAction);

  QObject::connect(_toolbar->buttonClose(), &QPushButton::pressed, [this]() {
    PlotDocker* parent_docker = static_cast<PlotDocker*>(dockManager());
    dockAreaWidget()->closeArea();
    takeWidget();
    if (_plot_widget)
    {
      _plot_widget->deleteLater();
      _plot_widget = nullptr;
    }
    if (_map_panel)
    {
      _map_panel->deleteLater();
      _map_panel = nullptr;
    }
    this->undoableChange();
    if (parent_docker)
    {
      parent_docker->refreshSharedTimeAxes();
    }
  });

  this->layout()->setMargin(10);
}

DockWidget::~DockWidget()
{
}

DockWidget* DockWidget::splitHorizontal()
{
  auto new_widget = new DockWidget(_datamap, qobject_cast<QWidget*>(parent()));

  PlotDocker* parent_docker = static_cast<PlotDocker*>(dockManager());
  auto area = parent_docker->addDockWidget(ads::RightDockWidgetArea, new_widget);

  area->setAllowedAreas(ads::OuterDockAreas);

  parent_docker->registerPlotWidget(new_widget->plotWidget());

  connect(this, &DockWidget::undoableChange, parent_docker, &PlotDocker::undoableChange);

  parent_docker->refreshSharedTimeAxes();
  this->undoableChange();

  return new_widget;
}

DockWidget* DockWidget::splitVertical()
{
  auto new_widget = new DockWidget(_datamap, qobject_cast<QWidget*>(parent()));

  PlotDocker* parent_docker = static_cast<PlotDocker*>(dockManager());

  auto area = parent_docker->addDockWidget(ads::BottomDockWidgetArea, new_widget, dockAreaWidget());

  area->setAllowedAreas(ads::OuterDockAreas);
  parent_docker->registerPlotWidget(new_widget->plotWidget());

  connect(this, &DockWidget::undoableChange, parent_docker, &PlotDocker::undoableChange);

  parent_docker->refreshSharedTimeAxes();
  this->undoableChange();

  return new_widget;
}

DockWidget* DockWidget::splitHorizontalLocal()
{
  auto new_widget = new DockWidget(_datamap, qobject_cast<QWidget*>(parent()));

  PlotDocker* parent_docker = static_cast<PlotDocker*>(dockManager());
  auto area = parent_docker->addDockWidget(ads::RightDockWidgetArea, new_widget, dockAreaWidget());

  area->setAllowedAreas(ads::OuterDockAreas);

  parent_docker->registerPlotWidget(new_widget->plotWidget());

  connect(this, &DockWidget::undoableChange, parent_docker, &PlotDocker::undoableChange);

  parent_docker->refreshSharedTimeAxes();
  this->undoableChange();

  return new_widget;
}

void DockWidget::createMapPanelSplit()
{
  auto new_widget = splitHorizontal();
  if (new_widget)
  {
    new_widget->convertToMapPanel();
  }
}

PlotWidget* DockWidget::plotWidget()
{
  return _plot_widget;
}

MapDockPanel* DockWidget::mapPanel()
{
  return _map_panel;
}

bool DockWidget::isMapPanel() const
{
  return (_map_panel != nullptr);
}

void DockWidget::convertToMapPanel()
{
  if (_map_panel)
  {
    return;
  }

  _map_panel = new MapDockPanel(_datamap, this);
  setWidget(_map_panel);

  if (auto parent_docker = static_cast<PlotDocker*>(dockManager()))
  {
    parent_docker->mapPanelAdded(_map_panel);
  }

  if (_plot_widget)
  {
    _plot_widget->deleteLater();
    _plot_widget = nullptr;
  }

  _toolbar->label()->setText("Map");
  if (auto parent_docker = static_cast<PlotDocker*>(dockManager()))
  {
    parent_docker->refreshSharedTimeAxes();
  }
  undoableChange();
}

DockToolbar* DockWidget::toolBar()
{
  return _toolbar;
}

QString DockWidget::name() const
{
  return _toolbar->label()->text();
}
