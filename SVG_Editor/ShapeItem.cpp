// =====================================================================
// ShapeItem.cpp
// ---------------------------------------------------------------------
// @brief ShapeItem 的实现
// @details 关键算法/魔法值：
//          - `boundingRect` 在 `shape()` 基础上各方向扩展 4 像素，为粗描边与
//            选区高亮预留空间；
//          - `shape()` 用 QPainterPathStroker 把路径沿描边方向外扩
//            `max(8, strokeWidth + 6)` 像素以扩大点击命中区；
//          - Point 的实际半径为 `max(3, strokeWidth * 1.5)`；
//          - Polygon 在预览模式下只画折线（不闭合成多边形），完成后才闭合。
// @layer   graphics
// =====================================================================

#include "ShapeItem.h"

#include <QGraphicsSceneMouseEvent>
#include <QPainter>
#include <QPainterPathStroker>

#include <algorithm>

ShapeItem::ShapeItem(const ShapeData& data, QGraphicsItem* parent)
    : QGraphicsObject(parent), m_data(normalizedShapeData(data)) {
    // 默认启用选中与移动；预览模式会在 setPreviewMode 中关闭
    setFlags(QGraphicsItem::ItemIsSelectable | QGraphicsItem::ItemIsMovable);
    setZValue(m_data.zValue);
}

QRectF ShapeItem::boundingRect() const { return shape().boundingRect().adjusted(-4.0, -4.0, 4.0, 4.0); }

QPainterPath ShapeItem::shape() const {
    QPainterPath path = buildPath();
    if (path.isEmpty()) {
        return path;
    }

    if (!m_data.style.strokeEnabled) {
        if (shapeSupportsFill(m_data.type) && m_data.style.fillEnabled) {
            return path;
        }

        const qreal fallbackWidth = std::max<qreal>(8.0, m_data.style.strokeWidth + 6.0);
        QPainterPathStroker stroker;
        stroker.setWidth(fallbackWidth);
        stroker.setCapStyle(Qt::RoundCap);
        stroker.setJoinStyle(Qt::RoundJoin);
        return stroker.createStroke(path);
    }

    // 命中区域沿描边方向外扩，至少 8 像素、否则按 strokeWidth+6 决定
    const qreal strokerWidth = std::max<qreal>(8.0, m_data.style.strokeWidth + 6.0);
    QPainterPathStroker stroker;
    stroker.setWidth(strokerWidth);
    stroker.setCapStyle(Qt::RoundCap);
    stroker.setJoinStyle(Qt::RoundJoin);

    if (shapeSupportsFill(m_data.type) && m_data.style.fillEnabled) {
        // 填充图形：把"路径"和"描边外扩"取并集，保证填充区域也能命中
        return stroker.createStroke(path).united(path);
    }

    return stroker.createStroke(path);
}

void ShapeItem::paint(QPainter* painter, const QStyleOptionGraphicsItem*, QWidget*) {
    painter->setRenderHint(QPainter::Antialiasing, true);
    painter->save();

    QPen pen = buildPen();
    painter->setPen(pen);

    // 不支持填充的形状强制无刷子
    const bool fillEnabled = shapeSupportsFill(m_data.type) && m_data.style.fillEnabled;
    painter->setBrush(fillEnabled ? QBrush(m_data.style.fillColor) : Qt::NoBrush);

    painter->drawPath(buildPath());

    // 非 Select 工具下仍保留 item 级选中框；Select 工具由 overlay 接管
    if (isSelected() && !m_previewMode && m_selectionDecorationVisible) {
        QPen selectionPen(QColor("#2d7ff9"));
        selectionPen.setStyle(Qt::DashLine);
        // cosmetic = true 让线宽在所有缩放等级下都保持 1 像素
        selectionPen.setCosmetic(true);
        selectionPen.setWidthF(1.0);
        painter->setPen(selectionPen);
        painter->setBrush(Qt::NoBrush);
        // 在 path 外扩 6 像素作为高亮范围
        painter->drawRect(shape().boundingRect().adjusted(-6.0, -6.0, 6.0, 6.0));
    }
    painter->restore();
}

ShapeData ShapeItem::shapeData() const { return m_data; }

void ShapeItem::setShapeData(const ShapeData& data) {
    // 几何改变前必须通知 Qt，否则 boundingRect 缓存会失效导致绘制异常
    prepareGeometryChange();
    m_data = normalizedShapeData(data);
    setZValue(m_data.zValue);
    update();
}

void ShapeItem::setPreviewMode(bool enabled) {
    m_previewMode = enabled;
    // 预览时不允许选中和拖动，避免破坏"正在绘制"的视觉状态
    setFlag(QGraphicsItem::ItemIsSelectable, !enabled);
    setFlag(QGraphicsItem::ItemIsMovable, !enabled);
    update();
}

void ShapeItem::setSelectionDecorationVisible(bool visible) {
    if (m_selectionDecorationVisible == visible) {
        return;
    }

    m_selectionDecorationVisible = visible;
    update();
}

bool ShapeItem::hasPendingMoveOffset() const { return !qFuzzyIsNull(pos().x()) || !qFuzzyIsNull(pos().y()); }

void ShapeItem::commitPendingMoveOffset() {
    if (m_previewMode || m_committingMove || !hasPendingMoveOffset()) {
        return;
    }

    commitMoveOffset(pos());
    emit shapeChanged(this);
}

void ShapeItem::mouseReleaseEvent(QGraphicsSceneMouseEvent* event) {
    QGraphicsObject::mouseReleaseEvent(event);

    // 预览模式 / 已经在提交位移中：直接返回
    if (m_previewMode || m_committingMove) {
        return;
    }

    // Qt 通过修改 pos() 实现 ItemIsMovable；此时 pos() 即为按下→释放的累计位移
    if (hasPendingMoveOffset()) {
        commitPendingMoveOffset();
    }
}

QPainterPath ShapeItem::buildPath() const { return m_data.transform.map(buildBasePath()); }

QPainterPath ShapeItem::buildBasePath() const {
    QPainterPath path;

    switch (m_data.type) {
    case ShapeType::Point:
        // Point 也用一个小椭圆作为 path 半径，确保命中区域可见
        if (!m_data.points.isEmpty()) {
            const QPointF point = m_data.points.first();
            const qreal radius = std::max<qreal>(3.0, m_data.style.strokeWidth * 1.5);
            path.addEllipse(point, radius, radius);
        }
        break;
    case ShapeType::Line:
        if (m_data.points.size() >= 2) {
            path.moveTo(m_data.points.at(0));
            path.lineTo(m_data.points.at(1));
        }
        break;
    case ShapeType::Polyline:
        if (!m_data.points.isEmpty()) {
            path.moveTo(m_data.points.first());
            for (int index = 1; index < m_data.points.size(); ++index) {
                path.lineTo(m_data.points.at(index));
            }
        }
        break;
    case ShapeType::Circle:
    case ShapeType::Ellipse:
        path.addEllipse(m_data.rect);
        break;
    case ShapeType::Rectangle:
        path.addRect(m_data.rect);
        break;
    case ShapeType::Polygon:
        // 与 paint() 中同样的"预览不闭合，正式闭合"逻辑
        if (m_previewMode && !m_data.points.isEmpty()) {
            path.moveTo(m_data.points.first());
            for (int index = 1; index < m_data.points.size(); ++index) {
                path.lineTo(m_data.points.at(index));
            }
        } else if (m_data.points.size() >= 3) {
            path.addPolygon(QPolygonF(m_data.points));
        }
        break;
    }

    return path;
}

QPen ShapeItem::buildPen() const {
    if (!m_previewMode && !m_data.style.strokeEnabled) {
        return Qt::NoPen;
    }

    QPen pen(m_data.style.strokeColor, m_data.style.strokeWidth, m_data.style.strokeStyle, Qt::RoundCap, Qt::RoundJoin);
    if (m_previewMode) {
        // 预览模式：颜色提亮 15%、改为虚线，提示"尚未提交"
        pen.setColor(m_data.style.strokeColor.lighter(115));
        pen.setStyle(Qt::DashLine);
    }
    return pen;
}

void ShapeItem::commitMoveOffset(const QPointF& delta) {
    if (qFuzzyIsNull(delta.x()) && qFuzzyIsNull(delta.y())) {
        return;
    }

    prepareGeometryChange();
    QTransform translation;
    translation.translate(delta.x(), delta.y());
    applyTransformToShapeData(m_data, translation);
    // 2) 通过 m_committingMove 抑制"setPos(0,0) 触发 mouseReleaseEvent 再提交"
    m_committingMove = true;
    setPos(QPointF());
    m_committingMove = false;
    update();
}
