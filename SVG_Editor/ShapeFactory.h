// =====================================================================
// ShapeFactory.h
// ---------------------------------------------------------------------
// @brief   ShapeItem 的轻量工厂 + 复制辅助
// @details 之所以把工厂独立成类，而不是直接在 CanvasView 中 `new ShapeItem`：
//          1. 集中处理"创建前归一化"等通用逻辑；
//          2. `cloneWithOffset` 把"复制 + 平移 + z 自增 + 新 UUID" 这一组
//             语义绑在一起，让 CanvasView::paste 只需要一行调用。
// @layer   graphics（依赖 ShapeItem 完整定义）
// =====================================================================

#pragma once

#include <memory>

#include "ShapeData.h"

class ShapeItem; // 前向声明，避免在头文件引入 ShapeItem.h

/// @brief ShapeItem 构造与克隆的工厂。
class ShapeFactory {
  public:
    /// @brief 用给定的数据构造一个 ShapeItem。
    /// @details 内部会先调用 `normalizedShapeData`，避免退化几何被传入渲染层。
    /// @param data 源 ShapeData（按值传入以避免悬垂引用）
    /// @return 拥有所有权的 unique_ptr；调用方决定挂到哪个 scene
    static std::unique_ptr<ShapeItem> createItem(const ShapeData& data);

    /// @brief 克隆一份数据并施加偏移、生成新 UUID、自增 z。
    /// @details 行为：
    ///          - `id` 重新生成（QUuid），保证唯一；
    ///          - `zValue += 1.0`，让粘贴的图形绘制在新图层之上；
    ///          - 几何整体平移 `offset`；
    ///          - 最后再走一次 `normalizedShapeData`。
    /// @param source 原始数据
    /// @param offset 平移量（场景坐标系）
    /// @return 可直接 `createItem` 的新 ShapeData
    static ShapeData cloneWithOffset(const ShapeData& data, const QPointF& offset);
};
