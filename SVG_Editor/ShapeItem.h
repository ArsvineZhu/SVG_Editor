// =====================================================================
// ShapeItem.h
// ---------------------------------------------------------------------
// @brief   单个图形在 QGraphicsScene 中的渲染节点
// @details `ShapeItem` 是 `ShapeData` 与 Qt Graphics View 框架的桥接类：
//          - 持有 ShapeData 副本（构造时即归一化），所有绘制与命中测试都基于它；
//          - 继承 QGraphicsObject 以支持信号（如 shapeChanged）；
//          - 通过自定义 `Type = UserType + 1` 让 `qgraphicsitem_cast<ShapeItem*>`
//            能从场景 items 中精确识别出本类实例。
// @layer   graphics
// @warning 私有成员 `m_committingMove` 用来抑制"提交位移 → 触发 mouseReleaseEvent
//          → 再次提交"的递归，调用 `setPos()` 前后必须保持其值一致。
// =====================================================================

#pragma once

#include <QGraphicsObject>
#include <QPainterPath>
#include <QPen>

#include "ShapeData.h"

/// @brief 在 QGraphicsScene 中代表一个 ShapeData 的可渲染、可选中、可移动对象。
class ShapeItem : public QGraphicsObject {
    Q_OBJECT

  public:
    /// @brief 自定义 type id，配合 `qgraphicsitem_cast<ShapeItem*>` 使用。
    enum { Type = QGraphicsItem::UserType + 1 };

    /// @brief 构造时即对 data 调用 normalizedShapeData，避免退化几何。
    /// @param data   源 ShapeData
    /// @param parent 父 QGraphicsItem（用于 Qt 对象树）
    explicit ShapeItem(const ShapeData& data, QGraphicsItem* parent = nullptr);

    /// @brief Qt 框架调用，返回 type() 用于 `qgraphicsitem_cast`。
    int type() const override { return Type; }

    /// @brief Qt 绘制/拾取使用；向外扩展 4 像素以容纳描边与选区高亮。
    QRectF boundingRect() const override;

    /// @brief Qt 命中测试用；描边宽度放大到 max(8, stroke+6) 像素以容差。
    QPainterPath shape() const override;

    /// @brief Qt 实际绘制入口；按 type 分支调用 drawXxx。
    void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override;

    /// @brief 取得当前绑定的 ShapeData（按值返回，独立副本）。
    ShapeData shapeData() const;

    /// @brief 替换内部数据。会自动 prepareGeometryChange 并刷新 z。
    /// @param data 新数据（内部会归一化）
    void setShapeData(const ShapeData& data);

    /// @brief 开启/关闭"预览模式"：用于拖拽创建过程中的实时反馈。
    /// @details 预览模式下：
    ///          - 不可被选中（ItemIsSelectable = false）
    ///          - 不可被拖动（ItemIsMovable = false）
    ///          - 描边色变浅 + 改为虚线
    void setPreviewMode(bool enabled);

  signals:
    /// @brief 当用户拖动结束后发出（含选中图形的位移提交）。
    /// @param item 发生变化的 ShapeItem 自身
    void shapeChanged(ShapeItem* item);

  protected:
    /// @brief 重写以在拖动结束后检测到实际位移时提交到 ShapeData 并 emit 信号。
    void mouseReleaseEvent(QGraphicsSceneMouseEvent* event) override;

  private:
    /// @brief 根据 m_data 与 m_previewMode 构造用于绘制 / 命中测试的路径。
    QPainterPath buildPath() const;

    /// @brief 根据 m_data.style 构造 QPen；预览模式下颜色变浅、改为虚线。
    QPen buildPen() const;

    /// @brief 把用户拖动产生的 pos() 偏移写入到 m_data，并把 item 的 pos 归零。
    /// @param delta 鼠标相对按下时的位移（场景坐标）
    void commitMoveOffset(const QPointF& delta);

    /// @brief 绑定的数据（构造时已归一化）
    ShapeData m_data;

    /// @brief 是否处于"拖拽中预览"状态；为 true 时禁用选中 / 移动
    bool m_previewMode = false;

    /// @brief 抑制"提交位移 → 触发 mouseReleaseEvent → 再提交"递归
    bool m_committingMove = false;
};
