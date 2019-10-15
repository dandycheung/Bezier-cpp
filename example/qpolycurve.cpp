#include "qpolycurve.h"

#include <QPainter>
#include <QPen>

#include "BezierCpp/bezier.h"

bool qPolyCurve::getDraw_control_points() const
{
  return draw_control_points;
}

void qPolyCurve::setDraw_control_points(bool value)
{
  draw_control_points = value;
}

bool qPolyCurve::getDraw_curvature_radious() const
{
  return draw_curvature_radious;
}

void qPolyCurve::setDraw_curvature_radious(bool value)
{
  draw_curvature_radious = value;
}

int qPolyCurve::type() const { return QGraphicsItem::UserType + 2; }

inline double kappaDerived(qPolyCurve* pcurve, double t)
{
  auto det = [](Bezier::Vec2 a, Bezier::Vec2 b)
  {
    return a.x() * b.y() - a.y() * b.x();
  };

  uint idx = t;
  t = t - idx;
  if (idx == pcurve->getSize()){
    idx--;
    t = 1;
  }
  auto curve = pcurve->getCurvePtr(idx);

  auto d1 = curve->getDerivative()->valueAt(t);
  auto d2 = curve->getDerivative()->getDerivative()->valueAt(t);
  auto d3 = curve->getDerivative()->getDerivative()->getDerivative()->valueAt(t);

  return (det(d1, d3) * d1.squaredNorm() - 3 * d1.dot(d2) * det(d1, d2)) / std::pow(d1.norm(), 5);
}

void qPolyCurve::paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
{
  Q_UNUSED(option)
  Q_UNUSED(widget)

  setFlag(GraphicsItemFlag::ItemIsSelectable, true);

  painter->setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform, true);
  QPen pen(Qt::darkBlue);
  pen.setStyle(isSelected() ? Qt::DashDotLine : Qt::SolidLine);
  painter->setPen(pen);

  QPainterPath curve;
  auto poly = getPolyline();
  curve.moveTo(poly[0].x(), poly[0].y());
  for (uint k = 1; k < poly.size(); k++)
    curve.lineTo(poly[k].x(), poly[k].y());
  painter->drawPath(curve);

  if (draw_control_points)
  {
    const int d = 6;
    painter->setBrush(QBrush(Qt::blue, Qt::SolidPattern));
    Bezier::PointVector points = getControlPoints();
    for (uint k = 1; k < points.size(); k++)
    {
      painter->setPen(Qt::blue);
      painter->drawEllipse(QRectF(points[k - 1].x() - d / 2, points[k - 1].y() - d / 2, d, d));
      painter->setPen(QPen(QBrush(Qt::gray), 1, Qt::DotLine));
      painter->drawLine(QLineF(points[k - 1].x(), points[k - 1].y(), points[k].x(), points[k].y()));
    }
    painter->setPen(Qt::blue);
    painter->drawEllipse(QRectF(points.back().x() - d / 2, points.back().y() - d / 2, d, d));
  }

  if (draw_curvature_radious)
  {
    painter->setPen(Qt::green);
    for (double t = 0; t <= getSize(); t += 1.0 / 500)
    {
      painter->setPen(QColor(static_cast<int>(std::fabs(255 * (0.5 - t / getSize()))),
                             static_cast<int>(255 * t / getSize()),
                             static_cast<int>(255 * (1 - t / getSize()))));
      auto p = valueAt(t);
      auto n1 = p + normalAt(t, false) * kappaDerived(this, t);
      auto n2 = p - normalAt(t, false) * kappaDerived(this, t);
      painter->drawLine(QLineF(n1.x(), n1.y(), n2.x(), n2.y()));
    }
  }
}

QRectF qPolyCurve::boundingRect() const
{
  auto bbox = getBBox(false);
  return QRectF(QPointF(bbox.min().x(), bbox.min().y()), QPointF(bbox.max().x(), bbox.max().y()));
}
