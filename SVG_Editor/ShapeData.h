// =====================================================================
// ShapeData.h
// ---------------------------------------------------------------------
// @brief   图形数据模型（核心层，无 Qt Widgets 依赖）
// @details 定义项目中所有图形的纯数据结构、几何归一化规则、JSON 序列化
//          函数以及本地化显示名。位于 `svg_editor_core` 静态库，被 UI 层、
//          图形层、I/O 层共同引用。
// @layer   core
// @deps    Qt6::Core（仅使用 QColor / QPointF / QRectF / QString / QJson）
// @warning 1. `points` 字段的语义随 `type` 变化，详见枚举下方说明；
//          2. JSON 字段名是项目持久化的唯一契约，重命名字段必须同步更新
//             `shapeDataToJson` 与 `shapeDataFromJson`，否则历史文件无法加载；
//          3. `zValue` 越大绘制越靠前，但 `ShapeItem` 内部会进一步叠加
//             自身的微小 z 偏移以解决同层图形的点击拾取问题。
// =====================================================================

#pragma once

#include <QColor>
#include <QJsonObject>
#include <QPointF>
#include <QRectF>
#include <QString>
#include <Qt>
#include <QVector>

#include <cstdint>
#include <optional>

#include "AppLanguage.h"

/// @brief 图形类别。
/// @details 决定：
///          - `ShapeData::points` 与 `ShapeData::rect` 中哪个字段承载几何信息；
///          - `PropertyPanel` 中需要显示哪些编辑字段；
///          - `ShapeItem::buildPath` 的绘制路径分支。
/// @note    枚举值的**顺序**对持久化无影响（序列化走字符串名），
///          但请避免在中间插入新值以减少 diff。
enum class ShapeType : std::uint8_t {
    Point,     ///< 单点；几何存放在 `points[0]`
    Line,      ///< 线段；几何存放在 `points[0..1]`
    Polyline,  ///< 折线；几何存放在 `points`，至少 2 个顶点
    Circle,    ///< 圆；几何存放在 `rect`，强制为正方形外接框
    Ellipse,   ///< 椭圆；几何存放在 `rect`
    Rectangle, ///< 矩形；几何存放在 `rect`
    Polygon,   ///< 多边形；几何存放在 `points`，自动闭合
};

/// @brief 图形的视觉样式（描边 + 填充）。
/// @note  字段顺序与 JSON 序列化顺序一致，请勿随意调整以保持输出可读。
struct ShapeStyle {
    QColor strokeColor = Qt::black;           ///< 描边颜色；alpha 受 Qt 平台支持
    qreal strokeWidth = 2.0;                  ///< 描边宽度（像素），范围 [0.5, +∞)（加载时夹紧）
    Qt::PenStyle strokeStyle = Qt::SolidLine; ///< 描边线型：Solid/Dash/Dot/DashDot
    QColor fillColor = QColor("#80c8ff");     ///< 填充颜色
    bool fillEnabled = false;                 ///< 是否启用填充；不支持填充的形状会被强制关闭
};

/// @brief 单一图形的全部持久化数据。
/// @details `points` 与 `rect` 的使用方式随 `type` 变化：
///          | type      | points 含义        | rect 含义           |
///          |-----------|--------------------|---------------------|
///          | Point     | 1 个点（坐标）     | 不使用              |
///          | Line      | 2 个点（起、终）   | 不使用              |
///          | Polyline  | ≥2 个顶点          | 不使用              |
///          | Polygon   | ≥2 个顶点（自动闭）| 不使用              |
///          | Circle    | 不使用             | 正方形外接框       |
///          | Ellipse   | 不使用             | 外接矩形           |
///          | Rectangle | 不使用             | 矩形（可任意方向）  |
struct ShapeData {
    QString id;                            ///< 唯一标识；用于引用、撤销栈 key 等
    ShapeType type = ShapeType::Rectangle; ///< 图形类别
    QVector<QPointF> points;               ///< 点列；语义见上方表格
    QRectF rect;                           ///< 包围盒/外接矩形；语义见上方表格
    ShapeStyle style;                      ///< 视觉样式
    qreal zValue = 0.0;                    ///< 绘制顺序，值越大越靠前
};

/// @brief 将 ShapeType 序列化为小写字符串（point/line/...）。
/// @param type 要转换的图形类别
/// @return 小写字符串；未知值返回 "unknown"
QString shapeTypeToString(ShapeType type);

/// @brief 字符串反序列化为 ShapeType。
/// @param value 候选字符串（来自 JSON）
/// @return 成功返回对应枚举；未匹配返回 std::nullopt
std::optional<ShapeType> shapeTypeFromString(const QString& value);

/// @brief 将 Qt::PenStyle 序列化为小写字符串。
/// @param style 描边线型
/// @return "solid"/"dash"/"dot"/"dashdot"；未识别值兜底返回 "solid"
QString penStyleToString(Qt::PenStyle style);

/// @brief 字符串反序列化为 Qt::PenStyle。
/// @param value 候选字符串
/// @return 成功返回对应线型；未匹配返回 std::nullopt
std::optional<Qt::PenStyle> penStyleFromString(const QString& value);

/// @brief 判断该图形类别是否支持"填充"。
/// @details 当前规则：Circle / Ellipse / Rectangle / Polygon 支持，Point / Line / Polyline 不支持。
/// @return true 表示该类别的图形可以启用 fillColor
bool shapeSupportsFill(ShapeType type);

/// @brief 图形类别的本地化显示名（用于 UI 菜单、面板标题等）。
/// @param type     图形类别
/// @param language 目标语言
QString shapeDisplayName(ShapeType type, AppLanguage language = AppLanguage::English);

/// @brief 描边线型的本地化显示名。
/// @param style    描边线型
/// @param language 目标语言
QString penStyleDisplayName(Qt::PenStyle style, AppLanguage language = AppLanguage::English);

/// @brief 对图形做几何归一化处理：保证 rect 非负宽高，圆强制为正方形外接框。
/// @param data 输入图形（按值修改前的副本）
/// @return 归一化后的图形；输入与输出 shape 不可变
ShapeData normalizedShapeData(const ShapeData& data);

/// @brief 计算点列的轴对齐包围盒。
/// @param points 输入点列
/// @return 空集返回 `QRectF()`（无效矩形）；否则返回 `.normalized()` 的 bbox
QRectF pointsBoundingRect(const QVector<QPointF>& points);

/// @brief 将图形整体平移 `delta`。
/// @details 根据 type 区分处理：
///          - 基于 points 的图形（Point/Line/Polyline/Polygon）平移每个点
///          - 基于 rect 的图形（Circle/Ellipse/Rectangle）平移 rect
/// @param data  目标图形（in-place 修改）
/// @param delta 平移量（场景坐标系）
void translateShapeData(ShapeData& data, const QPointF& delta);

/// @brief 将图形序列化为 JSON 对象。
/// @details 序列化前会自动调用 normalizedShapeData，确保 rect 非退化。
///          颜色统一使用 #AARRGGBB（带 alpha）十六进制。
/// @param data 源图形
/// @return 可直接放入 QJsonDocument 的对象
QJsonObject shapeDataToJson(const ShapeData& data);

/// @brief 从 JSON 对象反序列化图形。
/// @details 缺省值策略：color 字段缺省回退到不透明黑/全透明，strokeWidth
///          最小夹紧到 0.5，fillEnabled 在不支持填充的类型上会被强制关闭。
/// @param object 解析后的 JSON 对象
/// @return 成功返回归一化后的图形；type 字段缺失或未知返回 std::nullopt
std::optional<ShapeData> shapeDataFromJson(const QJsonObject& object);
