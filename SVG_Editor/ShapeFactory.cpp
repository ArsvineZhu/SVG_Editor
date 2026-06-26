// =====================================================================
// ShapeFactory.cpp
// ---------------------------------------------------------------------
// @brief ShapeFactory 的实现
// @details 创建 / 克隆逻辑都先经过 `normalizedShapeData`，是本项目"归一化
//          集中处理"的关键环节。z 自增 +1 足以让粘贴图形盖住原图形，
//          同时不会显著影响后续粘贴时的层次。
// @layer   graphics
// =====================================================================

#include "ShapeFactory.h"

#include <QUuid>

#include "ShapeItem.h"

std::unique_ptr<ShapeItem> ShapeFactory::createItem(const ShapeData& data) {
    // 归一化后再传入构造函数，避免退化矩形进入渲染层
    return std::make_unique<ShapeItem>(normalizedShapeData(data));
}

ShapeData ShapeFactory::cloneWithOffset(const ShapeData& source, const QPointF& offset) {
    ShapeData copy = source;
    // 1) 分配新的 UUID；WithoutBraces 形式去掉了 QUuid 默认的花括号
    copy.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    // 2) z 自增 1，让粘贴的图形绘制在原图之上
    copy.zValue += 1.0;
    // 3) 几何平移
    translateShapeData(copy, offset);
    // 4) 再次归一化，保证圆仍为正方形外接框
    return normalizedShapeData(copy);
}
