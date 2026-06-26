// =====================================================================
// ShapeData.cpp
// ---------------------------------------------------------------------
// @brief   ShapeData.h 中声明的自由函数实现
// @details 本文件不包含任何 UI 依赖，可被 core 静态库的所有调用方复用。
//          实现要点：
//          - JSON 序列化前会先 `normalizedShapeData`，避免退化矩形进入磁盘；
//          - 颜色统一为 `#AARRGGBB` 形式以保留 alpha；
//          - 反序列化对 strokeWidth、circle/ellipse 半径等做非负夹紧。
// @layer   core
// =====================================================================

#include "ShapeData.h"

#include <QJsonArray>

#include <algorithm>
#include <cmath>

namespace {

/// @brief 在两种语言之间二选一地返回字符串字面量。
/// @param language 目标语言
/// @param english  英文原文（UTF-8 字节串）
/// @param chinese  简体中文原文（UTF-8 字节串）
/// @return 翻译后的 QString；language 为 SimplifiedChinese 取后者，否则取前者
QString textForLanguage(AppLanguage language, const char* english, const char* chinese) {
    return language == AppLanguage::SimplifiedChinese ? QString::fromUtf8(chinese) : QString::fromUtf8(english);
}

/// @brief 将 QPointF 编码为 {"x":..., "y":...} JSON 对象。
QJsonObject pointToJson(const QPointF& point) {
    return {
        {"x", point.x()},
        {"y", point.y()},
    };
}

/// @brief 从 {"x":..., "y":...} JSON 对象解码 QPointF。
QPointF pointFromJson(const QJsonObject& object) {
    return QPointF(object.value("x").toDouble(), object.value("y").toDouble());
}

/// @brief 将点列编码为 JSON 数组（每个元素是 {x,y}）。
QJsonArray pointsToJson(const QVector<QPointF>& points) {
    QJsonArray array;
    for (const QPointF& point : points) {
        array.append(pointToJson(point));
    }
    return array;
}

/// @brief 从 JSON 数组解码点列。
QVector<QPointF> pointsFromJson(const QJsonArray& array) {
    QVector<QPointF> points;
    points.reserve(array.size());
    for (const auto& value : array) {
        points.append(pointFromJson(value.toObject()));
    }
    return points;
}

/// @brief 把任意方向的 QRectF 修正为非退化矩形：先 normalized() 处理反向，
///        再将负的 width/height 夹紧到 0，避免 Qt 后续绘制抛 warning。
QRectF normalizedRect(const QRectF& rect) {
    QRectF result = rect.normalized();
    if (result.width() < 0.0) {
        result.setWidth(0.0);
    }
    if (result.height() < 0.0) {
        result.setHeight(0.0);
    }
    return result;
}

} // namespace

QString shapeTypeToString(ShapeType type) {
    switch (type) {
    case ShapeType::Point:
        return "point";
    case ShapeType::Line:
        return "line";
    case ShapeType::Polyline:
        return "polyline";
    case ShapeType::Circle:
        return "circle";
    case ShapeType::Ellipse:
        return "ellipse";
    case ShapeType::Rectangle:
        return "rectangle";
    case ShapeType::Polygon:
        return "polygon";
    }
    // FIXME: switch 未覆盖 ShapeType 全部 7 个 case 时 GCC -Wswitch 会警告；
    //        这里依赖 default 兜底，但 QString("unknown") 并非真实有效值，
    //        上层若使用应回退或报错。当前实现依赖"枚举穷尽 + default 兜底"以
    //        保持 ABI 稳定，新增 ShapeType 时请同步此函数。
    return "unknown";
}

std::optional<ShapeType> shapeTypeFromString(const QString& value) {
    if (value == "point") {
        return ShapeType::Point;
    }
    if (value == "line") {
        return ShapeType::Line;
    }
    if (value == "polyline") {
        return ShapeType::Polyline;
    }
    if (value == "circle") {
        return ShapeType::Circle;
    }
    if (value == "ellipse") {
        return ShapeType::Ellipse;
    }
    if (value == "rectangle") {
        return ShapeType::Rectangle;
    }
    if (value == "polygon") {
        return ShapeType::Polygon;
    }
    return std::nullopt;
}

QString penStyleToString(Qt::PenStyle style) {
    switch (style) {
    case Qt::SolidLine:
        return "solid";
    case Qt::DashLine:
        return "dash";
    case Qt::DotLine:
        return "dot";
    case Qt::DashDotLine:
        return "dashdot";
    default:
        return "solid";
    }
}

std::optional<Qt::PenStyle> penStyleFromString(const QString& value) {
    if (value == "solid") {
        return Qt::SolidLine;
    }
    if (value == "dash") {
        return Qt::DashLine;
    }
    if (value == "dot") {
        return Qt::DotLine;
    }
    if (value == "dashdot") {
        return Qt::DashDotLine;
    }
    return std::nullopt;
}

bool shapeSupportsFill(ShapeType type) {
    return type == ShapeType::Circle || type == ShapeType::Ellipse || type == ShapeType::Rectangle ||
           type == ShapeType::Polygon;
}

QString shapeDisplayName(ShapeType type, AppLanguage language) {
    switch (type) {
    case ShapeType::Point:
        return textForLanguage(language, "Point", "点");
    case ShapeType::Line:
        return textForLanguage(language, "Line", "直线");
    case ShapeType::Polyline:
        return textForLanguage(language, "Polyline", "折线");
    case ShapeType::Circle:
        return textForLanguage(language, "Circle", "圆");
    case ShapeType::Ellipse:
        return textForLanguage(language, "Ellipse", "椭圆");
    case ShapeType::Rectangle:
        return textForLanguage(language, "Rectangle", "矩形");
    case ShapeType::Polygon:
        return textForLanguage(language, "Polygon", "多边形");
    }
    return textForLanguage(language, "Unknown", "未知图形");
}

QString penStyleDisplayName(Qt::PenStyle style, AppLanguage language) {
    switch (style) {
    case Qt::SolidLine:
        return textForLanguage(language, "Solid", "实线");
    case Qt::DashLine:
        return textForLanguage(language, "Dash", "虚线");
    case Qt::DotLine:
        return textForLanguage(language, "Dot", "点线");
    case Qt::DashDotLine:
        return textForLanguage(language, "Dash Dot", "点划线");
    default:
        return textForLanguage(language, "Solid", "实线");
    }
}

ShapeData normalizedShapeData(const ShapeData& source) {
    ShapeData data = source;
    // 先把 rect 修正为非退化矩形（统一处理反向、负宽高）
    data.rect = normalizedRect(data.rect);

    // Rectangle / Ellipse 保持任意矩形即可
    if (data.type == ShapeType::Rectangle || data.type == ShapeType::Ellipse) {
        return data;
    }

    if (data.type == ShapeType::Circle) {
        // 圆的几何必须以正方形外接框表达；
        // 取宽高中的较大者作为直径，原点不变以保留用户拖拽起点
        QRectF rect = normalizedRect(data.rect);
        const qreal side = std::max(rect.width(), rect.height());
        data.rect = QRectF(rect.topLeft(), QSizeF(side, side));
        return data;
    }

    return data;
}

QRectF pointsBoundingRect(const QVector<QPointF>& points) {
    if (points.isEmpty()) {
        // 空点列返回无效矩形（width/height 为 0），
        // 调用方应自行检查 isNull()/isEmpty()。
        return QRectF();
    }

    qreal minX = points.first().x();
    qreal minY = points.first().y();
    qreal maxX = points.first().x();
    qreal maxY = points.first().y();

    // 单次线性扫描计算 AABB，时间复杂度 O(n)
    for (const QPointF& point : points) {
        minX = std::min(minX, point.x());
        minY = std::min(minY, point.y());
        maxX = std::max(maxX, point.x());
        maxY = std::max(maxY, point.y());
    }

    return QRectF(QPointF(minX, minY), QPointF(maxX, maxY)).normalized();
}

void translateShapeData(ShapeData& data, const QPointF& delta) {
    switch (data.type) {
    case ShapeType::Point:
    case ShapeType::Line:
    case ShapeType::Polyline:
    case ShapeType::Polygon:
        // 基于点列的图形：平移每个顶点
        for (QPointF& point : data.points) {
            point += delta;
        }
        break;
    case ShapeType::Circle:
    case ShapeType::Ellipse:
    case ShapeType::Rectangle:
        // 基于包围盒的图形：直接平移 rect 即可
        data.rect.translate(delta);
        break;
    }
}

QJsonObject shapeDataToJson(const ShapeData& source) {
    // 序列化前先归一化，保证磁盘里不会出现负宽高的矩形
    ShapeData data = normalizedShapeData(source);

    QJsonObject geometry;
    switch (data.type) {
    case ShapeType::Point:
        // Point 只持久化 1 个坐标
        if (!data.points.isEmpty()) {
            geometry = pointToJson(data.points.first());
        }
        break;
    case ShapeType::Line:
        // Line 用 4 个标量字段；要求至少有 2 个点，否则 geometry 留空
        if (data.points.size() >= 2) {
            geometry = {
                {"x1", data.points.at(0).x()},
                {"y1", data.points.at(0).y()},
                {"x2", data.points.at(1).x()},
                {"y2", data.points.at(1).y()},
            };
        }
        break;
    case ShapeType::Polyline:
    case ShapeType::Polygon:
        // 折线/多边形：直接序列化整个点列
        geometry = {
            {"points", pointsToJson(data.points)},
        };
        break;
    case ShapeType::Circle: {
        // 圆：center + radius；由于归一化后 rect 必为正方形，半径取 width/2
        const QPointF center = data.rect.center();
        geometry = {
            {"cx", center.x()},
            {"cy", center.y()},
            {"r", data.rect.width() / 2.0},
        };
        break;
    }
    case ShapeType::Ellipse: {
        // 椭圆：center + 半轴
        const QPointF center = data.rect.center();
        geometry = {
            {"cx", center.x()},
            {"cy", center.y()},
            {"rx", data.rect.width() / 2.0},
            {"ry", data.rect.height() / 2.0},
        };
        break;
    }
    case ShapeType::Rectangle:
        // 矩形：4 个标量字段
        geometry = {
            {"x", data.rect.x()},
            {"y", data.rect.y()},
            {"width", data.rect.width()},
            {"height", data.rect.height()},
        };
        break;
    }

    return {
        {"id", data.id},
        {"type", shapeTypeToString(data.type)},
        {"geometry", geometry},
        // 颜色用 #AARRGGBB 形式保留 alpha；QColor::name 在 alpha=255 时仍输出 #RRGGBB，需调用 HexArgb 强制 8 位
        {"strokeColor", data.style.strokeColor.name(QColor::HexArgb)},
        {"strokeWidth", data.style.strokeWidth},
        {"strokeStyle", penStyleToString(data.style.strokeStyle)},
        {"fillColor", data.style.fillColor.name(QColor::HexArgb)},
        {"fillEnabled", data.style.fillEnabled},
        {"zValue", data.zValue},
    };
}

std::optional<ShapeData> shapeDataFromJson(const QJsonObject& object) {
    const std::optional<ShapeType> type = shapeTypeFromString(object.value("type").toString());
    if (!type.has_value()) {
        // type 字段缺失或非已知值时整体反序列化失败
        return std::nullopt;
    }

    ShapeData data;
    data.id = object.value("id").toString();
    data.type = *type;
    // 颜色缺省：描边回退到不透明黑，填充回退到全透明
    data.style.strokeColor = QColor(object.value("strokeColor").toString("#ff000000"));
    // 描边宽度最小夹紧到 0.5，避免出现不可见或反走样的描边
    data.style.strokeWidth = std::max(0.5, object.value("strokeWidth").toDouble(2.0));
    data.style.fillColor = QColor(object.value("fillColor").toString("#00000000"));
    // 不支持填充的形状类型强制关闭 fillEnabled，避免 UI 出现无意义勾选
    data.style.fillEnabled = object.value("fillEnabled").toBool(false) && shapeSupportsFill(data.type);
    data.zValue = object.value("zValue").toDouble(0.0);

    const std::optional<Qt::PenStyle> penStyle = penStyleFromString(object.value("strokeStyle").toString("solid"));
    data.style.strokeStyle = penStyle.value_or(Qt::SolidLine);

    const QJsonObject geometry = object.value("geometry").toObject();
    switch (data.type) {
    case ShapeType::Point:
        // Point 直接用 geometry 的 {x,y}
        data.points = {pointFromJson(geometry)};
        break;
    case ShapeType::Line:
        // Line 用 4 个标量；缺省 0.0
        data.points = {
            QPointF(geometry.value("x1").toDouble(), geometry.value("y1").toDouble()),
            QPointF(geometry.value("x2").toDouble(), geometry.value("y2").toDouble()),
        };
        break;
    case ShapeType::Polyline:
    case ShapeType::Polygon:
        // Polyline / Polygon：直接反序列化点列
        data.points = pointsFromJson(geometry.value("points").toArray());
        break;
    case ShapeType::Circle: {
        // 圆：center + r；r 夹紧非负
        const qreal r = std::max(0.0, geometry.value("r").toDouble());
        const qreal cx = geometry.value("cx").toDouble();
        const qreal cy = geometry.value("cy").toDouble();
        data.rect = QRectF(cx - r, cy - r, r * 2.0, r * 2.0);
        break;
    }
    case ShapeType::Ellipse: {
        // 椭圆：center + rx/ry；半径夹紧非负
        const qreal rx = std::max(0.0, geometry.value("rx").toDouble());
        const qreal ry = std::max(0.0, geometry.value("ry").toDouble());
        const qreal cx = geometry.value("cx").toDouble();
        const qreal cy = geometry.value("cy").toDouble();
        data.rect = QRectF(cx - rx, cy - ry, rx * 2.0, ry * 2.0);
        break;
    }
    case ShapeType::Rectangle:
        // 矩形：4 个标量，宽高夹紧非负
        data.rect = QRectF(geometry.value("x").toDouble(), geometry.value("y").toDouble(),
                           std::max(0.0, geometry.value("width").toDouble()),
                           std::max(0.0, geometry.value("height").toDouble()));
        break;
    }

    // 加载后再次归一化，防止外部编辑过的 JSON 文件绕过写时归一化
    return normalizedShapeData(data);
}
