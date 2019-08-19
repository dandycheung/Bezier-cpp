#include "qcurve.h"

#include <QPainter>
#include <QPen>

void qCurve::setDraw_control_points(bool value) { draw_control_points = value; }

void qCurve::setDraw_curvature_radious(bool value) { draw_curvature_radious = value; }

bool qCurve::getDraw_control_points() const { return draw_control_points; }

bool qCurve::getDraw_curvature_radious() const { return draw_curvature_radious; }

Bezier::CurvePtr qCurve::getSharedPtr()
{
  return shared_from_this();
}

int qCurve::type() const { return QGraphicsItem::UserType + 1; }

void qCurve::paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
{
  Q_UNUSED(option)
  Q_UNUSED(widget)

  setFlag(GraphicsItemFlag::ItemIsSelectable, true);

  painter->setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform, true);

  painter->setPen(QPen(isSelected() ? Qt::DashDotLine : Qt::SolidLine));
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
    for (double t = 0; t <= 1.0; t += 1.0 / 500)
    {
      painter->setPen(QColor(abs(255 * (0.5 - t)), (int)(255 * t), (int)(255 * (1 - t))));
      auto p = valueAt(t);
      auto n1 = p + normalAt(t, false) * curvatureAt(t);
      auto n2 = p - normalAt(t, false) * curvatureAt(t);
      painter->drawLine(QLineF(n1.x(), n1.y(), n2.x(), n2.y()));
    }
  }
}

QRectF qCurve::boundingRect() const
{
  auto bbox = getBBox(false);
  return QRectF(QPointF(bbox.min().x(), bbox.min().y()), QPointF(bbox.max().x(), bbox.max().y()));
}