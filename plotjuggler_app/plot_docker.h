/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#ifndef PLOT_DOCKER_H
#define PLOT_DOCKER_H

#include <QDomElement>
#include <QXmlStreamReader>
#include "PlotJuggler/plotdata.h"
#include "plotwidget.h"
#include "map_dock_panel.h"
#include "plot_docker_toolbar.h"

class DockWidget : public ads::CDockWidget
{
  Q_OBJECT
  friend class PlotDocker;

public:
  DockWidget(PlotDataMapRef& datamap, QWidget* parent = nullptr);

  ~DockWidget() override;

  PlotWidget* plotWidget();
  MapDockPanel* mapPanel();
  bool isMapPanel() const;

  void convertToMapPanel();

  DockToolbar* toolBar();

  QString name() const;

public slots:
  DockWidget* splitHorizontal();

  DockWidget* splitVertical();

  void createMapPanelSplit();

private:
  DockWidget* splitHorizontalLocal();

  PlotWidget* _plot_widget = nullptr;
  MapDockPanel* _map_panel = nullptr;

  DockToolbar* _toolbar;

  PlotDataMapRef& _datamap;

signals:
  void undoableChange();
};

class PlotDocker : public ads::CDockManager
{
  Q_OBJECT
  friend class DockWidget;

public:
  PlotDocker(QString name, PlotDataMapRef& datamap, QWidget* parent = nullptr);

  ~PlotDocker();

  QString name() const;

  void setName(QString name);

  QDomElement xmlSaveState(QDomDocument& doc) const;

  bool xmlLoadState(QDomElement& tab_element);

  int plotCount() const;

  PlotWidget* plotAt(int index);
  MapDockPanel* mapPanelAt(int index);

  void setHorizontalLink(bool enabled);

  void zoomOut();

  void replot();

  void refreshSharedTimeAxes();

public slots:

  void on_stylesheetChanged(QString theme);

  void savePlotsToFile();

private:
  void registerPlotWidget(PlotWidget* plot_widget);

  void restoreSplitter(QDomElement elem, DockWidget* widget);

  QRect plotRelativeFootprint(int index, QSize plot_size) const;

  QString _name;

  PlotDataMapRef& _datamap;

signals:

  void plotWidgetAdded(PlotWidget*);
  void mapPanelAdded(MapDockPanel*);

  void undoableChange();
};

#endif  // PLOT_DOCKER_H
