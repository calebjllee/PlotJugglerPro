/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#ifndef REALSLIDER_H
#define REALSLIDER_H

#include <QSlider>
#include <algorithm>
#include <cmath>
#include <limits>

class RealSlider : public QSlider
{
  Q_OBJECT
public:
  RealSlider(QWidget* parent = nullptr);

  void setLimits(double min, double max, int steps);

  double getValue() const;

  void setRealValue(double val);

  double getMaximum() const
  {
    return _max_value;
  }

  double getMinimum() const
  {
    return _min_value;
  }

  void setRealStepValue(double step);

private slots:
  void onValueChanged(int value);

signals:
  void realValueChanged(double);

private:
  double _min_value;
  double _max_value;
};
//-------------------------------------------------------------

inline RealSlider::RealSlider(QWidget* parent) : QSlider(parent)
{
  setLimits(0.0, 1.0, 1);
  connect(this, &QSlider::valueChanged, this, &RealSlider::onValueChanged);
}

inline void RealSlider::setLimits(double min, double max, int steps)
{
  _min_value = min;
  _max_value = std::max(max, min + std::numeric_limits<double>::epsilon());
  QSlider::setRange(0, std::max(1, steps));
}

inline double RealSlider::getValue() const
{
  int min = minimum();
  int max = maximum();
  if (max <= min)
  {
    return _min_value;
  }
  const double ratio = (double)(value() - min) / (double)(max - min);
  return (_max_value - _min_value) * ratio + _min_value;
}

inline void RealSlider::setRealValue(double val)
{
  val = std::max(val, _min_value);
  val = std::min(val, _max_value);
  const double denom = _max_value - _min_value;
  const double ratio = denom > 0.0 ? (val - _min_value) / denom : 0.0;
  long pos = std::round((double)(maximum() - minimum()) * ratio + minimum());
  QSlider::setValue(pos);
}

inline void RealSlider::setRealStepValue(double step)
{
  const int steps = std::max(1, maximum() - minimum());
  const double ratio = (_max_value - _min_value) / (double)steps;
  int new_step = std::max(1, static_cast<int>(std::round(step / ratio)));
  QSlider::setSingleStep(new_step);
}

inline void RealSlider::onValueChanged(int value)
{
  int min = minimum();
  int max = maximum();
  if (max <= min)
  {
    emit realValueChanged(_min_value);
    return;
  }
  const double ratio = (double)(value - min) / (double)(max - min);
  double posX = (_max_value - _min_value) * ratio + _min_value;
  emit realValueChanged(posX);
}

#endif  // REALSLIDER_H
