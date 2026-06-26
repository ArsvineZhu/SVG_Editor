// =====================================================================
// SelectionTransformOverlayItem.h
// ---------------------------------------------------------------------
// @brief   选中图形的「缩放 / 旋转」操控手柄（QGraphicsObject 子类）
// @details 当画布上有图形被选中时，CanvasView 会在场景中放置一个
//          SelectionTransformOverlayItem 实例并实时跟随选中项的 bbox。
//          它本身 **不** 修改任何 ShapeData：
//            - paint()      绘制蓝色虚线选框 + 4 个角点方形手柄 + 上方圆形旋转手柄
//            - shape()      用作 hit-test，仅 5 个手柄区域可被命中
//            - handleAt()   告诉调用方鼠标当前落在哪个手柄上
//          上层（CanvasView）拿到 handle 后再驱动真正的 transform 逻辑。
// @layer   ui
// @warning 该 item 显式 `setZValue(1e6)`，确保始终盖在所有 ShapeItem 之上；
//          接受鼠标事件被设为 `Qt::NoButton`，所以事件不会被它自身消费，
//          CanvasView 仍能在最外层 QGraphicsView 截获所有鼠标事件。
// =====================================================================

#pragma once

#include <QGraphicsObject>
#include <QRectF>

#include <cstdint>

class QPainter;
class QStyleOptionGraphicsItem;
class QWidget;

/// @brief 选中图形的可交互覆盖层（操控手柄）。
/// @details
/// - 形状：选区 4 角各一个 10×10 方形手柄 + 选区正上方一个 6 半径的圆形旋转手柄。
/// - 视觉：1px 蓝色虚线选框（`#2d7ff9`），手柄白底蓝框，旋转手柄蓝底。
/// - 命中区：仅 5 个手柄矩形；选框线条本身不可点。
class SelectionTransformOverlayItem : public QGraphicsObject {
    Q_OBJECT

  public:
    /// @brief 手柄位置枚举。
    /// @note  `None` 表示「未命中任何手柄」，仅作为 `handleAt()` 的返回值使用，
    ///        不参与绘制与命中计算。
    enum class Handle : std::uint8_t {
        None,        ///< 无命中（仅 handleAt 返回值）
        TopLeft,     ///< 左上角缩放手柄
        TopRight,    ///< 右上角缩放手柄
        BottomLeft,  ///< 左下角缩放手柄
        BottomRight, ///< 右下角缩放手柄
        Rotate,      ///< 选区上方的圆形旋转手柄
    };

    /// @brief 构造。手柄默认隐藏，z 值固定为 1e6。
    explicit SelectionTransformOverlayItem(QGraphicsItem* parent = nullptr);

    /// @brief Qt Graphics View 框架要求：返回 item 的外接矩形（含旋转手柄上方的留白）。
    QRectF boundingRect() const override;

    /// @brief Qt Graphics View 框架要求：返回命中测试路径（5 个手柄矩形）。
    QPainterPath shape() const override;

    /// @brief 绘制选框 + 5 个手柄。
    /// @param painter  Qt 传入的画笔
    /// @param option   Qt 传入的样式选项（未使用）
    /// @param widget   Qt 传入的目标 widget（未使用）
    void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override;

    /// @brief 设置当前选区矩形。会先 `prepareGeometryChange()`、归一化（处理反向矩形）、
    ///        再根据是否为空决定显示 / 隐藏。
    /// @param bounds  选区矩形（场景坐标系）
    void setSelectionBounds(const QRectF& bounds);

    /// @brief 清除选区：复位 `m_bounds` 并隐藏 item。
    void clearSelectionBounds();

    /// @brief 当前选区矩形（可能为空矩形）。
    QRectF selectionBounds() const;

    /// @brief 控制是否显示缩放锚点与旋转手柄。
    void setHandlesVisible(bool visible);

    /// @brief 当前是否显示缩放锚点与旋转手柄。
    bool handlesVisible() const;

    /// @brief 命中测试：返回给定场景坐标点上的手柄。
    /// @param scenePoint  场景坐标
    /// @return            命中的手柄；若未命中任何手柄或选区为空，返回 `Handle::None`
    Handle handleAt(const QPointF& scenePoint) const;

  private:
    /// @brief 给定手柄位置，返回其在场景坐标系中的矩形（用作绘制与命中测试）。
    QRectF handleRect(Handle handle) const;

    /// @brief 旋转手柄的圆心 = 选区顶边中点向上偏移 `kRotateHandleOffset`。
    QPointF rotateHandleCenter() const;

    /// @brief 选区是否有效（既非 null 也非空）。
    bool hasBounds() const;

    /// @brief 当前选区矩形（场景坐标系）。空矩形 = 无选区。
    QRectF m_bounds;

    /// @brief true 时绘制四角锚点和旋转手柄；false 时仅绘制虚线矩形。
    bool m_handlesVisible = true;
};
