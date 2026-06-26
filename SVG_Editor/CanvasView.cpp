// =====================================================================
// CanvasView.cpp
// ---------------------------------------------------------------------
// @brief CanvasView 的实现
// @details 本文件实现两类工作流：
//          - 拖拽工作流（drag）：Line / Rectangle / Circle / Ellipse
//          - 路径工作流（path）：Polyline / Polygon
//          二者共用 m_previewItem 字段；`cancelDrawing()` 是它们的共同出口。
// @layer   ui
// =====================================================================

#include "CanvasView.h"

#include <QFile>
#include <QGraphicsScene>
#include <QImage>
#include <QKeyEvent>
#include <QKeySequence>
#include <QLineF>
#include <QMouseEvent>
#include <QPainter>
#include <QUuid>

#include <algorithm>

#include "ShapeFactory.h"
#include "ShapeItem.h"

namespace {

/// @brief 在两种语言间二选一返回字符串（与 ShapeData.cpp 中类似但签名是 QString）。
QString textForLanguage(AppLanguage language, const QString& english, const QString& chinese) {
    return language == AppLanguage::SimplifiedChinese ? chinese : english;
}

/// @brief 是否为 path 类工具（多次点击绘制折线 / 多边形）。
bool isPathTool(CanvasView::Tool tool) {
    return tool == CanvasView::Tool::Polyline || tool == CanvasView::Tool::Polygon;
}

/// @brief 是否为 drag 类工具（按下并拖拽以绘制）。
bool isDragTool(CanvasView::Tool tool) {
    return tool == CanvasView::Tool::Line || tool == CanvasView::Tool::Rectangle || tool == CanvasView::Tool::Circle ||
           tool == CanvasView::Tool::Ellipse;
}

/// @brief 把工具枚举映射到图形类别。Select 工具落到 Rectangle 作为兜底返回值。
/// @note Select 工具正常不会触发绘制分支，因此 default 分支理论上不可达。
ShapeType toolToShapeType(CanvasView::Tool tool) {
    switch (tool) {
    case CanvasView::Tool::Point:
        return ShapeType::Point;
    case CanvasView::Tool::Line:
        return ShapeType::Line;
    case CanvasView::Tool::Polyline:
        return ShapeType::Polyline;
    case CanvasView::Tool::Circle:
        return ShapeType::Circle;
    case CanvasView::Tool::Ellipse:
        return ShapeType::Ellipse;
    case CanvasView::Tool::Rectangle:
        return ShapeType::Rectangle;
    case CanvasView::Tool::Polygon:
        return ShapeType::Polygon;
    case CanvasView::Tool::Select:
        return ShapeType::Rectangle;
    }
    return ShapeType::Rectangle;
}

/// @brief 从起点和终点构造一个正方形外接矩形（用于 Circle 工具）。
/// @details 圆需要"以起点为角点的等边矩形"，所以以 dx/dy 绝对值较大者为边长，
///          并按拖动方向决定左 / 上偏移。
QRectF circleRectFromPoints(const QPointF& start, const QPointF& end) {
    const qreal dx = end.x() - start.x();
    const qreal dy = end.y() - start.y();
    const qreal side = std::max(std::abs(dx), std::abs(dy));
    const qreal x = dx >= 0.0 ? start.x() : start.x() - side;
    const qreal y = dy >= 0.0 ? start.y() : start.y() - side;
    return QRectF(x, y, side, side);
}

} // namespace

CanvasView::CanvasView(QWidget* parent) : QGraphicsView(parent), m_scene(new QGraphicsScene(this)) {
    // 默认样式：黑色实线 2 像素、淡蓝填充
    m_currentStyle.strokeColor = Qt::black;
    m_currentStyle.strokeWidth = 2.0;
    m_currentStyle.strokeStyle = Qt::SolidLine;
    m_currentStyle.fillColor = QColor("#80c8ff");
    m_currentStyle.fillEnabled = true;

    // 1200×800 白色画布；sceneRect 是图形合法存在的坐标系范围
    m_scene->setSceneRect(0.0, 0.0, 1200.0, 800.0);
    m_scene->setBackgroundBrush(Qt::white);

    setScene(m_scene);
    setRenderHint(QPainter::Antialiasing, true);
    // Select 工具用橡皮筋拖框选；其他工具关闭以避免与"按下即开始画"冲突
    setDragMode(QGraphicsView::RubberBandDrag);
    setViewportUpdateMode(QGraphicsView::FullViewportUpdate);

    // 选中变化即通知外部（PropertyPanel / MainWindow）
    connect(m_scene, &QGraphicsScene::selectionChanged, this, &CanvasView::handleSelectionChanged);
}

void CanvasView::setLanguage(AppLanguage language) { m_language = language; }

void CanvasView::setTool(Tool tool) {
    if (m_tool == tool) {
        return;
    }

    // 切换工具前先复位任何正在进行的绘制（避免预览节点泄漏）
    cancelDrawing();
    m_tool = tool;
    // Select 工具启用框选；其他工具关闭 Qt 自带拖动以免与我们自己的 drag 逻辑冲突
    setDragMode(tool == Tool::Select ? QGraphicsView::RubberBandDrag : QGraphicsView::NoDrag);
    emit statusMessageChanged(tool == Tool::Select
                                  ? textForLanguage(m_language, "Current tool: Select", "当前工具：选择")
                                  : textForLanguage(m_language, "Current tool: %1", "当前工具：%1")
                                        .arg(shapeDisplayName(toolToShapeType(tool), m_language)));
}

CanvasView::Tool CanvasView::tool() const { return m_tool; }

void CanvasView::updateSelectedShape(const ShapeData& data) {
    ShapeItem* item = selectedShapeItem();
    if (item == nullptr) {
        return;
    }

    // 1) 把 PropertyPanel 的修改写回图形
    item->setShapeData(data);
    // 2) 把修改后的样式作为"当前样式"，让后续新图形继承
    m_currentStyle = data.style;
    refreshSelectionNotification();
}

void CanvasView::deleteSelectedItem() {
    ShapeItem* item = selectedShapeItem();
    if (item == nullptr) {
        return;
    }

    // Qt scene 只接管 item 的 scene 关系，删除仍需手动 delete 以释放内存
    m_scene->removeItem(item);
    delete item;
    refreshSelectionNotification();
}

void CanvasView::copySelectedItem() {
    ShapeItem* item = selectedShapeItem();
    if (item == nullptr) {
        return;
    }

    // 把当前数据完整保存到剪贴板；m_pasteCount 复位以便粘贴偏移从 16 像素开始
    m_clipboardShape = item->shapeData();
    m_pasteCount = 0;
    emit statusMessageChanged(textForLanguage(m_language, "Shape copied.", "图形已复制。"));
}

void CanvasView::pasteCopiedItem() {
    if (!m_clipboardShape.has_value()) {
        return;
    }

    ++m_pasteCount;
    // 1) 复用工厂的 clone + offset + z+1 能力
    ShapeData copy =
        ShapeFactory::cloneWithOffset(*m_clipboardShape, QPointF(16.0 * m_pasteCount, 16.0 * m_pasteCount));
    // 2) 粘贴的图形强制取当前 z 计数器的下一个值，确保盖在所有现有图形之上
    copy.zValue = nextZValue();
    addShape(copy, true);
    emit statusMessageChanged(textForLanguage(m_language, "Shape pasted.", "图形已粘贴。"));
}

void CanvasView::clearCanvas() {
    cancelDrawing();
    // scene->clear() 会删除所有 item（包括 item 持有的 QObject 关系）
    m_scene->clear();
    m_previewItem = nullptr;
    m_zCounter = 0.0;
    m_pasteCount = 0;
    refreshSelectionNotification();
}

DocumentData CanvasView::documentData() const {
    QList<ShapeData> shapes;
    QList<ShapeItem*> items;

    // 1) 收集所有 ShapeItem（跳过预览节点）
    for (QGraphicsItem* graphicsItem : m_scene->items(Qt::AscendingOrder)) {
        if (auto* shapeItem = qgraphicsitem_cast<ShapeItem*>(graphicsItem)) {
            if (shapeItem == m_previewItem) {
                continue;
            }
            items.append(shapeItem);
        }
    }

    // 2) 按 z 升序排序，让磁盘顺序与显示顺序一致
    std::sort(items.begin(), items.end(), [](const ShapeItem* lhs, const ShapeItem* rhs) {
        return lhs->shapeData().zValue < rhs->shapeData().zValue;
    });

    for (const ShapeItem* item : items) {
        shapes.append(item->shapeData());
    }

    return {m_scene->sceneRect().size(), shapes};
}

void CanvasView::loadDocument(const DocumentData& document) {
    clearCanvas();
    // 用文档中的画布尺寸替换默认 1200×800
    m_scene->setSceneRect(QRectF(QPointF(0.0, 0.0), document.canvasSize));
    for (const ShapeData& shape : document.shapes) {
        addShape(shape, false);
        // 同步 z 计数器到当前最大值，避免后续新图形 z 倒退
        m_zCounter = std::max(m_zCounter, shape.zValue);
    }
    refreshSelectionNotification();
}

bool CanvasView::exportImage(const QString& filePath, QString* errorMessage) const {
    // 1) 优先按"所有图形实际占用的 bbox"导出；空画布回退到 sceneRect
    QRectF bounds = m_scene->itemsBoundingRect();
    if (bounds.isNull()) {
        bounds = m_scene->sceneRect();
    }

    // 2) 四周外扩 20 像素留白；最小尺寸 400×300 防止极小图
    bounds = bounds.adjusted(-20.0, -20.0, 20.0, 20.0);
    const QSize imageSize = bounds.size().toSize().expandedTo(QSize(400, 300));

    QImage image(imageSize, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::white);

    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing, true);
    // 把 scene 渲染到 image 上；前两个参数为目标矩形，第三个为源矩形
    m_scene->render(&painter, QRectF(QPointF(0.0, 0.0), imageSize), bounds);
    painter.end();

    if (!image.save(filePath)) {
        if (errorMessage != nullptr) {
            *errorMessage = textForLanguage(m_language, "Failed to save image file.", "保存图片文件失败。");
        }
        return false;
    }

    return true;
}

void CanvasView::mousePressEvent(QMouseEvent* event) {
    const QPointF scenePoint = mapToScene(event->pos());

    if (event->button() == Qt::LeftButton) {
        if (m_tool == Tool::Point) {
            // Point 工具：单击即创建一个点
            ShapeData data;
            data.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
            data.type = ShapeType::Point;
            data.points = {scenePoint};
            data.style = currentStyleFor(ShapeType::Point);
            data.zValue = nextZValue();
            addShape(data, true);
            return;
        }

        if (isDragTool(m_tool)) {
            // 拖拽工作流 Step 1
            beginDragShape(scenePoint);
            return;
        }

        if (isPathTool(m_tool)) {
            // 路径工作流 Step 1
            beginPathPoint(scenePoint);
            return;
        }
    }

    // 其他情况交给 QGraphicsView 处理（处理选中、拖框等）
    QGraphicsView::mousePressEvent(event);
}

void CanvasView::mouseMoveEvent(QMouseEvent* event) {
    const QPointF scenePoint = mapToScene(event->pos());

    if (m_dragDrawing) {
        // 拖拽中持续更新预览
        updateDragPreview(scenePoint);
        return;
    }

    if (!m_pathPoints.isEmpty()) {
        // 路径绘制中：把光标作为"幽灵顶点"持续延展
        updatePathPreview(scenePoint);
    }

    QGraphicsView::mouseMoveEvent(event);
}

void CanvasView::mouseReleaseEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton && m_dragDrawing) {
        // 拖拽工作流 Step 3
        finishDragShape(mapToScene(event->pos()));
        return;
    }

    QGraphicsView::mouseReleaseEvent(event);
}

void CanvasView::mouseDoubleClickEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton && isPathTool(m_tool) && !m_pathPoints.isEmpty()) {
        const QPointF scenePoint = mapToScene(event->pos());
        // 双击点与上一顶点距离 >1 像素时，作为新顶点加入
        if ((m_pathPoints.last() - scenePoint).manhattanLength() > 1.0) {
            m_pathPoints.append(scenePoint);
        }
        // 结束 path 绘制；Polygon 工具会闭合，Polyline 不会
        finishPathShape(m_tool == Tool::Polygon);
        return;
    }

    QGraphicsView::mouseDoubleClickEvent(event);
}

void CanvasView::keyPressEvent(QKeyEvent* event) {
    // Enter 结束路径绘制
    if ((event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) && isPathTool(m_tool) &&
        !m_pathPoints.isEmpty()) {
        finishPathShape(m_tool == Tool::Polygon);
        return;
    }

    // Esc 取消任何正在进行的绘制
    if (event->key() == Qt::Key_Escape) {
        cancelDrawing();
        return;
    }

    // Delete / Backspace 删除选中
    if (event->key() == Qt::Key_Delete || event->key() == Qt::Key_Backspace) {
        deleteSelectedItem();
        return;
    }

    if (event->matches(QKeySequence::Copy)) {
        copySelectedItem();
        return;
    }

    if (event->matches(QKeySequence::Paste)) {
        pasteCopiedItem();
        return;
    }

    QGraphicsView::keyPressEvent(event);
}

ShapeItem* CanvasView::selectedShapeItem() const {
    const QList<QGraphicsItem*> items = m_scene->selectedItems();
    for (QGraphicsItem* item : items) {
        if (auto* shapeItem = qgraphicsitem_cast<ShapeItem*>(item)) {
            return shapeItem;
        }
    }
    return nullptr;
}

void CanvasView::handleSelectionChanged() { refreshSelectionNotification(); }

void CanvasView::addShape(const ShapeData& data, bool selectNewItem) {
    std::unique_ptr<ShapeItem> item = ShapeFactory::createItem(data);
    // 桥接 shapeChanged → 更新当前样式 + 通知外部
    connect(item.get(), &ShapeItem::shapeChanged, this, [this](ShapeItem* changedItem) {
        m_currentStyle = changedItem->shapeData().style;
        refreshSelectionNotification();
    });

    // 转移所有权到 scene 后再由 Qt 内存管理
    ShapeItem* rawItem = item.release();
    m_scene->addItem(rawItem);
    m_zCounter = std::max(m_zCounter, data.zValue);

    if (selectNewItem) {
        m_scene->clearSelection();
        rawItem->setSelected(true);
    }
}

void CanvasView::beginDragShape(const QPointF& scenePoint) {
    // 任何新 drag 开始前都先复位一次（避免与遗留 path 状态冲突）
    cancelDrawing();
    m_dragDrawing = true;
    m_dragStartPoint = scenePoint;

    // 创建预览 ShapeData；用当前工具类型与样式
    ShapeData preview = buildDragShapeData(scenePoint);
    preview.zValue = nextZValue();
    std::unique_ptr<ShapeItem> item = ShapeFactory::createItem(preview);
    item->setPreviewMode(true);

    m_previewItem = item.release();
    m_scene->addItem(m_previewItem);
}

void CanvasView::updateDragPreview(const QPointF& scenePoint) {
    if (m_previewItem == nullptr) {
        return;
    }

    // 持续更新预览形状
    m_previewItem->setShapeData(buildDragShapeData(scenePoint));
}

void CanvasView::finishDragShape(const QPointF& scenePoint) {
    // 取最终形状的快照
    const ShapeData data = buildDragShapeData(scenePoint);

    m_dragDrawing = false;
    // 删除预览节点（最终 ShapeItem 会由 addShape 创建新的）
    if (m_previewItem != nullptr) {
        m_scene->removeItem(m_previewItem);
        delete m_previewItem;
        m_previewItem = nullptr;
    }

    // 最小尺寸校验：避免误点造成 0 长度 / 0 面积图形
    bool validShape = false;
    switch (data.type) {
    case ShapeType::Line:
        validShape = data.points.size() >= 2 && QLineF(data.points.at(0), data.points.at(1)).length() > 1.0;
        break;
    case ShapeType::Circle:
    case ShapeType::Ellipse:
    case ShapeType::Rectangle:
        validShape = data.rect.width() > 1.0 && data.rect.height() > 1.0;
        break;
    default:
        break;
    }

    if (validShape) {
        addShape(data, true);
    }
}

void CanvasView::beginPathPoint(const QPointF& scenePoint) {
    if (m_pathPoints.isEmpty()) {
        // 第一次落点：创建预览节点（此时点列只含 1 个点，但点列过少不会绘制任何东西，
        // 所以预览仅在 updatePathPreview 中追加"幽灵"顶点后才可见）
        ShapeData preview;
        preview.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
        preview.type = toolToShapeType(m_tool);
        preview.style = currentStyleFor(preview.type);
        preview.zValue = nextZValue();

        std::unique_ptr<ShapeItem> item = ShapeFactory::createItem(preview);
        item->setPreviewMode(true);
        m_previewItem = item.release();
        m_scene->addItem(m_previewItem);
    }

    m_pathPoints.append(scenePoint);
    updatePathPreview(scenePoint);
}

void CanvasView::updatePathPreview(const QPointF& scenePoint) {
    if (m_previewItem == nullptr || m_pathPoints.isEmpty()) {
        return;
    }

    // 预览 = 已确认顶点 + 鼠标当前位置（作为"幽灵"尾点）
    m_previewItem->setShapeData(buildPathPreviewData(scenePoint));
}

void CanvasView::finishPathShape(bool closed) {
    if (m_pathPoints.isEmpty()) {
        // 没有有效顶点，直接复位
        cancelDrawing();
        return;
    }

    ShapeData data = buildPathPreviewData(m_pathPoints.last());
    // 用真正的顶点列表覆盖预览的"含幽灵点"列表
    data.points = m_pathPoints;
    data.type = closed ? ShapeType::Polygon : ShapeType::Polyline;

    // 删除预览节点
    if (m_previewItem != nullptr) {
        m_scene->removeItem(m_previewItem);
        delete m_previewItem;
        m_previewItem = nullptr;
    }

    // 最小顶点数：多边形 ≥3、折线 ≥2
    const int minimumPoints = closed ? 3 : 2;
    if (data.points.size() >= minimumPoints) {
        addShape(data, true);
    }

    m_pathPoints.clear();
}

void CanvasView::cancelDrawing() {
    // drag 状态机复位
    m_dragDrawing = false;
    // path 状态机复位
    m_pathPoints.clear();
    // 释放预览节点
    if (m_previewItem != nullptr) {
        m_scene->removeItem(m_previewItem);
        delete m_previewItem;
        m_previewItem = nullptr;
    }
}

ShapeData CanvasView::buildDragShapeData(const QPointF& endPoint) const {
    ShapeData data;
    data.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    data.type = toolToShapeType(m_tool);
    data.style = currentStyleFor(data.type);
    // 用 zCounter+1 作为临时 z；提交时会被 addShape 内的 max() 同步
    data.zValue = m_zCounter + 1.0;

    switch (data.type) {
    case ShapeType::Line:
        // Line：起点 + 终点
        data.points = {m_dragStartPoint, endPoint};
        break;
    case ShapeType::Rectangle:
    case ShapeType::Ellipse:
        // Rectangle / Ellipse：QRectF 自动 normalized() 处理反向拖动
        data.rect = QRectF(m_dragStartPoint, endPoint).normalized();
        break;
    case ShapeType::Circle:
        // Circle：必须用正方形外接框
        data.rect = circleRectFromPoints(m_dragStartPoint, endPoint);
        break;
    default:
        break;
    }

    return normalizedShapeData(data);
}

ShapeData CanvasView::buildPathPreviewData(const QPointF& cursorPoint) const {
    ShapeData data;
    data.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    data.type = toolToShapeType(m_tool);
    data.style = currentStyleFor(data.type);
    data.zValue = m_zCounter + 1.0;
    // 复用已确认的顶点，并附加鼠标位置作为"幽灵尾点"
    data.points = m_pathPoints;
    data.points.append(cursorPoint);
    return normalizedShapeData(data);
}

ShapeStyle CanvasView::currentStyleFor(ShapeType type) const {
    ShapeStyle style = m_currentStyle;
    // 不支持填充的形状（如 Point / Line / Polyline）必须关闭 fillEnabled，
    // 否则 UI 在 PropertyPanel 中会出现无意义的勾选状态
    if (!shapeSupportsFill(type)) {
        style.fillEnabled = false;
    }
    return style;
}

void CanvasView::refreshSelectionNotification() {
    if (ShapeItem* item = selectedShapeItem(); item != nullptr) {
        emit selectedShapeChanged(item);
    } else {
        emit selectedShapeChanged(nullptr);
    }
}

qreal CanvasView::nextZValue() {
    // 单调递增：每次创建图形 / 启动预览都 +1，确保新图形绘制在旧图形之上
    m_zCounter += 1.0;
    return m_zCounter;
}
