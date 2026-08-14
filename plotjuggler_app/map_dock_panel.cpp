/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include "map_dock_panel.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>

#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonValue>
#include <QLabel>
#include <QMenu>
#include <QPushButton>
#include <QSettings>
#include <QSignalBlocker>
#include <QTextBrowser>
#include <QUrl>
#include <QVBoxLayout>

#include "PlotJuggler/svg_util.h"

#ifndef PJ_HAS_WEBENGINE
#define PJ_HAS_WEBENGINE 0
#endif

#if PJ_HAS_WEBENGINE
#include <QWebEnginePage>
#include <QWebEngineView>
#endif

namespace
{
QObject* findDockWidget(QObject* object)
{
  QObject* current = object;
  while (current)
  {
    if (current->inherits("DockWidget"))
    {
      return current;
    }
    current = current->parent();
  }
  return nullptr;
}
}

MapDockPanel::MapDockPanel(PJ::PlotDataMapRef& plot_data, QWidget* parent)
  : QWidget(parent), _plot_data(plot_data)
{
  auto* layout = new QVBoxLayout(this);
  layout->setContentsMargins(6, 6, 6, 6);
  layout->setSpacing(6);

  auto* row = new QHBoxLayout();
  row->setSpacing(6);

  auto* lat_label = new QLabel("Lat", this);
  _lat_combo = new QComboBox(this);
  _lat_combo->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
  _lat_combo->setMinimumContentsLength(20);

  auto* lon_label = new QLabel("Lon", this);
  _lon_combo = new QComboBox(this);
  _lon_combo->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
  _lon_combo->setMinimumContentsLength(20);

  auto* fit_button = new QPushButton("Fit to View", this);

  {
    QSettings settings;
    const QString theme = settings.value("StyleSheet::theme", "light").toString();
    fit_button->setIcon(LoadSvg(":/resources/svg/zoom_max.svg", theme));
  }

  row->addWidget(lat_label);
  row->addWidget(_lat_combo, 1);
  row->addWidget(lon_label);
  row->addWidget(_lon_combo, 1);
  row->addWidget(fit_button);

  auto* trail_row = new QHBoxLayout();
  trail_row->setSpacing(6);

  auto* trail_label = new QLabel("Trail", this);
  _trail_length_slider = new QSlider(Qt::Horizontal, this);
  _trail_length_slider->setRange(0, 60);
  _trail_length_slider->setSingleStep(1);
  _trail_length_slider->setPageStep(10);
  _trail_length_slider->setTickInterval(10);
  _trail_length_slider->setTickPosition(QSlider::TicksBelow);
  _trail_length_slider->setValue(0);
  _trail_length_label = new QLabel(this);
  _trail_length_label->setMinimumWidth(68);

  trail_row->addWidget(trail_label);
  trail_row->addWidget(_trail_length_slider, 1);
  trail_row->addWidget(_trail_length_label);

  _status_label = new QLabel(this);
  _status_label->setTextInteractionFlags(Qt::TextSelectableByMouse);

  layout->addLayout(row);
  layout->addLayout(trail_row);
  layout->addWidget(_status_label);

#if PJ_HAS_WEBENGINE
  _web_view = new QWebEngineView(this);
  _web_view->setContextMenuPolicy(Qt::CustomContextMenu);
  _web_view->setHtml(mapHtml(), QUrl("https://www.openstreetmap.org"));
  connect(_web_view, &QWidget::customContextMenuRequested, this, &MapDockPanel::showContextMenu);
  layout->addWidget(_web_view, 1);
#else
  auto* fallback = new QTextBrowser(this);
  fallback->setText("Qt WebEngine is not available. Map panel requires Qt5 WebEngineWidgets.");
  fallback->setReadOnly(true);
  layout->addWidget(fallback, 1);
#endif

  connect(_lat_combo, &QComboBox::currentTextChanged, this,
          [this](const QString&) { selectionChanged(); });
  connect(_lon_combo, &QComboBox::currentTextChanged, this,
          [this](const QString&) { selectionChanged(); });
  connect(fit_button, &QPushButton::clicked, this, [this]() {
    loadDetectedCurves();
    selectionChanged();
    zoomToRoute();
  });
  connect(_trail_length_slider, &QSlider::valueChanged, this, [this](int) {
    updateTrailLengthLabel();
    selectionChanged();
  });

#if PJ_HAS_WEBENGINE
  connect(_web_view, &QWebEngineView::loadFinished, this, [this](bool ok) {
    _map_ready = ok;
    if (!ok)
    {
      setStatus("Map initialization failed");
      return;
    }
    _route_dirty = true;
    updateRouteOnMap();
    if (_has_time)
    {
      updateMarkerOnMap(_last_time);
    }
  });
  setStatus("Select latitude and longitude curves");
#else
  setStatus("Qt WebEngine not available in this build");
#endif

  refreshCurveCombos();
  updateTrailLengthLabel();
}

void MapDockPanel::loadDetectedCurves()
{
  refreshCurveCombos();
  autoDetectCurves(/*force_overwrite=*/true);
  requestSelectedCurveLoad();
  refreshCurveCombos();
}

QDomElement MapDockPanel::xmlSaveState(QDomDocument& doc) const
{
  auto el = doc.createElement("map_panel");
  el.setAttribute("lat_curve", _lat_combo->currentText());
  el.setAttribute("lon_curve", _lon_combo->currentText());
  el.setAttribute("trail_length_sec", _trail_length_slider->value());
  return el;
}

bool MapDockPanel::xmlLoadState(const QDomElement& element)
{
  _pending_lat_curve = element.attribute("lat_curve");
  _pending_lon_curve = element.attribute("lon_curve");
  if (element.hasAttribute("trail_length_sec"))
  {
    _trail_length_slider->setValue(element.attribute("trail_length_sec").toInt());
    updateTrailLengthLabel();
  }
  applyPendingSelection();
  selectionChanged();
  return true;
}

void MapDockPanel::onTimeUpdated(double absolute_time)
{
  _has_time = true;
  _last_time = absolute_time;
  _route_dirty = true;
#if PJ_HAS_WEBENGINE
  updateMarkerOnMap(absolute_time);
#endif
}

void MapDockPanel::refreshCurveCombos()
{
  const auto current_lat = _lat_combo->currentText();
  const auto current_lon = _lon_combo->currentText();

  QStringList series_names;
  series_names.reserve(static_cast<int>(_plot_data.numeric.size()));

  for (const auto& [name, _] : _plot_data.numeric)
  {
    series_names.push_back(QString::fromStdString(name));
  }

  std::sort(series_names.begin(), series_names.end(),
            [](const QString& a, const QString& b) { return a.toLower() < b.toLower(); });

  {
    const QSignalBlocker b1(_lat_combo);
    const QSignalBlocker b2(_lon_combo);
    _lat_combo->clear();
    _lon_combo->clear();
    _lat_combo->addItems(series_names);
    _lon_combo->addItems(series_names);
  }

  auto restoreCombo = [](QComboBox* combo, const QString& value) {
    if (value.isEmpty())
    {
      return;
    }
    auto idx = combo->findText(value);
    if (idx >= 0)
    {
      combo->setCurrentIndex(idx);
    }
  };

  restoreCombo(_lat_combo, current_lat);
  restoreCombo(_lon_combo, current_lon);

  applyPendingSelection();

  autoDetectCurves(/*force_overwrite=*/false);

  selectionChanged();
}

void MapDockPanel::autoDetectCurves(bool force_overwrite)
{
  QString latitude_curve;
  QString longitude_curve;

  for (int i = 0; i < _lat_combo->count(); i++)
  {
    const QString name = _lat_combo->itemText(i);

    if (latitude_curve.isEmpty() && name.contains("Latitude", Qt::CaseInsensitive))
    {
      latitude_curve = name;
    }

    if (longitude_curve.isEmpty() && name.contains("Longitude", Qt::CaseInsensitive))
    {
      longitude_curve = name;
    }

    if (!latitude_curve.isEmpty() && !longitude_curve.isEmpty())
    {
      break;
    }
  }

  if (!latitude_curve.isEmpty())
  {
    if (force_overwrite || _lat_combo->currentText().isEmpty() ||
        !_lat_combo->currentText().contains("Latitude", Qt::CaseInsensitive))
    {
      _lat_combo->setCurrentText(latitude_curve);
    }
  }

  if (!longitude_curve.isEmpty())
  {
    if (force_overwrite || _lon_combo->currentText().isEmpty() ||
        !_lon_combo->currentText().contains("Longitude", Qt::CaseInsensitive))
    {
      _lon_combo->setCurrentText(longitude_curve);
    }
  }
}

void MapDockPanel::requestSelectedCurveLoad()
{
  QStringList curve_names;

  auto addIfValid = [&](const QString& curve_name) {
    if (curve_name.isEmpty() || curve_names.contains(curve_name))
    {
      return;
    }
    if (_plot_data.numeric.find(curve_name.toStdString()) != _plot_data.numeric.end())
    {
      curve_names.push_back(curve_name);
    }
  };

  addIfValid(_lat_combo->currentText());
  addIfValid(_lon_combo->currentText());

  if (!curve_names.isEmpty())
  {
    emit curvesLoadRequested(curve_names);
  }
}

void MapDockPanel::selectionChanged()
{
  _fit_route_once = true;
  _route_dirty = true;
#if PJ_HAS_WEBENGINE
  updateRouteOnMap();
  if (_has_time)
  {
    updateMarkerOnMap(_last_time);
  }
#endif
}

void MapDockPanel::zoomToRoute()
{
#if PJ_HAS_WEBENGINE
  if (!_map_ready || !_web_view)
  {
    return;
  }
  _web_view->page()->runJavaScript("window.pj_fitRoute();");
#endif
}

void MapDockPanel::showContextMenu(const QPoint& pos)
{
#if PJ_HAS_WEBENGINE
  if (!_web_view)
  {
    return;
  }

  QMenu menu(this);
  menu.setStyleSheet("QMenu::icon { width: 12px; }");
  auto* fit_action = menu.addAction("Fit to View");
  menu.addSeparator();
  auto* split_h_action = menu.addAction("Add X/Y Panel");
  auto* split_v_action = menu.addAction("Add Time-Series Chart");
  auto* split_map_action = menu.addAction("Add Map View");

  {
    QSettings settings;
    const QString theme = settings.value("StyleSheet::theme", "light").toString();
    fit_action->setIcon(LoadSvg(":/resources/svg/zoom_max.svg", theme));
    split_h_action->setIcon(LoadSvg(":/resources/svg/grid.svg", theme));
    split_v_action->setIcon(LoadSvg(":/resources/svg/colored_charts.svg", theme));
    split_map_action->setIcon(LoadSvg(":/resources/svg/scatter.svg", theme));
  }

  auto* selected_action = menu.exec(_web_view->mapToGlobal(pos));
  if (!selected_action)
  {
    return;
  }

  if (selected_action == fit_action)
  {
    loadDetectedCurves();
    selectionChanged();
    zoomToRoute();
    return;
  }

  QObject* dock = findDockWidget(this);
  if (!dock)
  {
    return;
  }

  if (selected_action == split_h_action)
  {
    QMetaObject::invokeMethod(dock, "splitHorizontal");
  }
  else if (selected_action == split_v_action)
  {
    QMetaObject::invokeMethod(dock, "splitVertical");
  }
  else if (selected_action == split_map_action)
  {
    QMetaObject::invokeMethod(dock, "createMapPanelSplit");
  }
#else
  (void)pos;
#endif
}

void MapDockPanel::applyPendingSelection()
{
  auto setIfFound = [](QComboBox* combo, const QString& target) {
    if (target.isEmpty())
    {
      return;
    }
    const auto idx = combo->findText(target);
    if (idx >= 0)
    {
      combo->setCurrentIndex(idx);
    }
  };

  setIfFound(_lat_combo, _pending_lat_curve);
  setIfFound(_lon_combo, _pending_lon_curve);

  _pending_lat_curve.clear();
  _pending_lon_curve.clear();
}

void MapDockPanel::updateTrailLengthLabel()
{
  const int seconds = _trail_length_slider->value();
  if (seconds <= 0)
  {
    _trail_length_label->setText("All past");
    return;
  }

  _trail_length_label->setText(QString("%1 s").arg(seconds));
}

void MapDockPanel::updateRouteOnMap()
{
#if PJ_HAS_WEBENGINE
  if (!_map_ready || !_web_view)
  {
    return;
  }

  const PJ::PlotData* lat_series = nullptr;
  const PJ::PlotData* lon_series = nullptr;
  if (!selectedSeries(lat_series, lon_series))
  {
    setStatus("Select valid latitude/longitude series");
    _web_view->page()->runJavaScript("window.pj_setRoute([], false);");
    return;
  }

  QJsonArray points;
  const auto sample_count = std::min(lat_series->size(), lon_series->size());
  points = QJsonArray();

  const double max_time =
      _has_time ? _last_time : std::numeric_limits<double>::infinity();
  const int trail_length_sec = _trail_length_slider->value();
  const double min_time =
      (_has_time && trail_length_sec > 0) ?
          (_last_time - static_cast<double>(trail_length_sec)) :
          -std::numeric_limits<double>::infinity();

  for (size_t i = 0; i < sample_count; i++)
  {
    const auto& lat_pt = (*lat_series)[i];
    const auto& lon_pt = (*lon_series)[i];
    const double point_time = std::max(lat_pt.x, lon_pt.x);

    if (!std::isfinite(point_time) || point_time > max_time || point_time < min_time)
    {
      continue;
    }

    if (!std::isfinite(lat_pt.y) || !std::isfinite(lon_pt.y))
    {
      continue;
    }
    if (lat_pt.y < -90.0 || lat_pt.y > 90.0 || lon_pt.y < -180.0 || lon_pt.y > 180.0)
    {
      continue;
    }

    QJsonArray ll;
    ll.append(lat_pt.y);
    ll.append(lon_pt.y);
    points.append(ll);
  }

  const QJsonDocument doc(points);
  const QString js = QString("window.pj_setRoute(%1, %2);")
                         .arg(QString::fromUtf8(doc.toJson(QJsonDocument::Compact)))
                         .arg(_fit_route_once ? "true" : "false");

  _web_view->page()->runJavaScript(js);
  _fit_route_once = false;
  _route_dirty = false;
#endif
}

void MapDockPanel::updateMarkerOnMap(double absolute_time)
{
#if PJ_HAS_WEBENGINE
  if (!_map_ready || !_web_view)
  {
    return;
  }

  if (_route_dirty)
  {
    updateRouteOnMap();
  }

  const PJ::PlotData* lat_series = nullptr;
  const PJ::PlotData* lon_series = nullptr;
  if (!selectedSeries(lat_series, lon_series))
  {
    return;
  }

  const int idx_lat = lat_series->getIndexFromX(absolute_time);
  const int idx_lon = lon_series->getIndexFromX(absolute_time);
  if (idx_lat < 0 || idx_lon < 0)
  {
    return;
  }

  const auto& lat_pt = (*lat_series)[idx_lat];
  const auto& lon_pt = (*lon_series)[idx_lon];

  if (!std::isfinite(lat_pt.y) || !std::isfinite(lon_pt.y))
  {
    return;
  }

  if (lat_pt.y < -90.0 || lat_pt.y > 90.0 || lon_pt.y < -180.0 || lon_pt.y > 180.0)
  {
    return;
  }

  const QString js = QString("window.pj_setPosition(%1, %2);")
                         .arg(QString::number(lat_pt.y, 'g', 16))
                         .arg(QString::number(lon_pt.y, 'g', 16));

  _web_view->page()->runJavaScript(js);
#endif
}

bool MapDockPanel::selectedSeries(const PJ::PlotData*& lat_series,
                                  const PJ::PlotData*& lon_series) const
{
  lat_series = nullptr;
  lon_series = nullptr;

  const auto lat_it = _plot_data.numeric.find(_lat_combo->currentText().toStdString());
  if (lat_it != _plot_data.numeric.end())
  {
    lat_series = &lat_it->second;
  }

  const auto lon_it = _plot_data.numeric.find(_lon_combo->currentText().toStdString());
  if (lon_it != _plot_data.numeric.end())
  {
    lon_series = &lon_it->second;
  }

  return (lat_series != nullptr && lon_series != nullptr);
}

void MapDockPanel::setStatus(const QString& text)
{
  _status_label->setText(text);
}

QString MapDockPanel::mapHtml()
{
  auto tile_url = qEnvironmentVariable("PJ_MAP_TILES_URL");
  auto tile_attr = qEnvironmentVariable("PJ_MAP_ATTRIBUTION");

  if (tile_url.isEmpty())
  {
    tile_url = "https://{s}.basemaps.cartocdn.com/light_all/{z}/{x}/{y}{r}.png";
  }
  if (tile_attr.isEmpty())
  {
    tile_attr = "&copy; OpenStreetMap contributors &copy; CARTO";
  }

  auto escape_js = [](QString str) {
    str.replace('\\', "\\\\");
    str.replace('\'', "\\'");
    str.replace('\n', " ");
    str.replace('\r', " ");
    return str;
  };

  const QString tile_url_js = escape_js(tile_url);
  const QString tile_attr_js = escape_js(tile_attr);

  return QString(R"HTML(
<!doctype html>
<html>
<head>
  <meta charset="utf-8"/>
  <meta name="viewport" content="width=device-width, initial-scale=1.0"/>
  <link rel="stylesheet" href="https://unpkg.com/leaflet@1.9.4/dist/leaflet.css"/>
  <script src="https://unpkg.com/leaflet@1.9.4/dist/leaflet.js"></script>
  <style>
    html, body, #map { height: 100%; margin: 0; padding: 0; }
  </style>
</head>
<body>
  <div id="map"></div>
  <script>
    let pjMap = null;
    let pjRoute = null;
    let pjMarker = null;

    function ensureMap() {
      if (pjMap) {
        return;
      }
      pjMap = L.map('map', { preferCanvas: true }).setView([0, 0], 2);
      const tileUrl = '%1';
      const tileAttribution = '%2';
      if (tileUrl && tileUrl.length > 0) {
        L.tileLayer(tileUrl, {
          maxZoom: 19,
          attribution: tileAttribution
        }).addTo(pjMap);
      }
      pjRoute = L.polyline([], {
        color: '#2563eb',
        weight: 3,
        opacity: 0.85
      }).addTo(pjMap);
      pjMarker = L.circleMarker([0, 0], {
        radius: 7,
        color: '#ffffff',
        weight: 2,
        fillColor: '#dc2626',
        fillOpacity: 0.95
      }).addTo(pjMap);
      pjMarker.bringToFront();
    }

    window.pj_setRoute = function(points, fitBounds) {
      ensureMap();
      pjRoute.setLatLngs(points || []);
      pjMarker.bringToFront();
      if (fitBounds) {
        window.pj_fitRoute();
      }
    }

    window.pj_fitRoute = function() {
      ensureMap();
      const pts = pjRoute.getLatLngs();
      if (!pts || pts.length === 0) {
        return;
      }
      if (pts.length === 1) {
        pjMap.setView(pts[0], 16);
        return;
      }
      pjMap.fitBounds(pjRoute.getBounds(), { padding: [20, 20] });
    }

    window.pj_setPosition = function(lat, lon) {
      ensureMap();
      pjMarker.setLatLng([lat, lon]);
      pjMarker.bringToFront();
    }

    ensureMap();
  </script>
</body>
</html>
  )HTML")
      .arg(tile_url_js, tile_attr_js);
}
