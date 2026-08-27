/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#ifndef MAP_DOCK_PANEL_H
#define MAP_DOCK_PANEL_H

#include <QWidget>
#include <QComboBox>
#include <QLabel>
#include <QDomDocument>
#include <QStringList>
#include <QSlider>

#include "PlotJuggler/plotdata.h"

class QWebEngineView;

class MapDockPanel : public QWidget
{
  Q_OBJECT

public:
  explicit MapDockPanel(PJ::PlotDataMapRef& plot_data, QWidget* parent = nullptr);

  void onTimeUpdated(double absolute_time);
  void loadDetectedCurves();
  void refreshCurveCombos();
  void refreshAfterDataReload();

  QDomElement xmlSaveState(QDomDocument& doc) const;
  bool xmlLoadState(const QDomElement& element);

signals:
  void curvesLoadRequested(const QStringList& curve_names);

private:
  void autoDetectCurves(bool force_overwrite);
  void requestSelectedCurveLoad();
  void selectionChanged();
  void applyPendingSelection();
  void updateTrailLengthLabel();
  void updateRouteOnMap();
  void updateMarkerOnMap(double absolute_time);
  void zoomToRoute();
  void showContextMenu(const QPoint& pos);
  bool selectedSeries(const PJ::PlotData*& lat_series, const PJ::PlotData*& lon_series) const;
  void setStatus(const QString& text);
  static QString mapHtml();

  PJ::PlotDataMapRef& _plot_data;

  QComboBox* _lat_combo = nullptr;
  QComboBox* _lon_combo = nullptr;
  QSlider* _trail_length_slider = nullptr;
  QLabel* _trail_length_label = nullptr;
  QLabel* _status_label = nullptr;
  QWebEngineView* _web_view = nullptr;

  QString _pending_lat_curve;
  QString _pending_lon_curve;

  bool _map_ready = false;
  bool _route_dirty = true;
  bool _fit_route_once = true;
  bool _has_time = false;
  double _last_time = 0.0;
};

#endif
