// =====================================================================
// CanvasView.h
// ---------------------------------------------------------------------
// @brief   主画布视图（QGraphicsView 子类）
// @details CanvasView 是整个应用的交互核心，承担以下职责：
//          1. 工具状态机：根据 Tool 切换 鼠标按下/移动/释放 行为；
//          2. 拖拽创建工作流（Line/Rectangle/Circle/Ellipse）：
//             beginDragShape → updateDragPreview → finishDragShape；
//          3. 路径创建工作流（Polyline/Polygon）：
//             beginPathPoint（多次） → finishPathShape；
//          4. 选中通知：把当前选中图形（ShapeItem*）通过信号上报给
//             MainWindow / PropertyPanel；
//          5. 序列化桥接：documentData() / loadDocument()；
//          6. PNG 导出：exportImage()。
// @layer   ui
// @warning 拖拽创建工作流与路径创建工作流共享 `m_previewItem` 字段，
//          二者不可同时处于活跃状态。`cancelDrawing()` 必须能把任一状态完全
//          复位，否则会出现孤儿预览节点。
// =====================================================================

#pragma once

#include <QGraphicsView>

#include <cstdint>
#include <optional>

#include "AppLanguage.h"
#include "FileManager.h"
#include "SelectionTransformOverlayItem.h"
#include "ShapeData.h"
#include "ShapeItem.h"

/// @brief 画布视图。所有用户绘图交互都从此视图发起。
class CanvasView : public QGraphicsView {
    Q_OBJECT

  public:
    /// @brief 工具枚举。
    /// @note  Select  不是可绘制的图形，但用同一枚举统一所有用户操作入口。
    enum class Tool : std::uint8_t {
        Select,    ///< 选中 / 框选模式
        Point,     ///< 点击即得一个点
        Line,      ///< 拖拽出线段
        Polyline,  ///< 多次点击绘制折线
        Circle,    ///< 拖拽出正方形外接圆
        Ellipse,   ///< 拖拽出椭圆
        Rectangle, ///< 拖拽出矩形
        Polygon,   ///< 多次点击绘制多边形（自动闭合）
    };

    /// @brief 构造。初始化默认工具、默认样式、1200×800 白色场景。
    explicit CanvasView(QWidget* parent = nullptr);

    /// @brief 切换界面语言（影响状态栏提示文本）。
    void setLanguage(AppLanguage language);

    /// @brief 切换工具。会先 cancelDrawing()，再更新 drag 模式。
    void setTool(Tool tool);
    /// @brief 当前工具。
    Tool tool() const;

    /// @brief PropertyPanel 调用的回写入口：替换当前选中图形的全部数据。
    void updateSelectedShape(const ShapeData& data);

    /// @brief 删除当前选中图形（无选中则无操作）。
    void deleteSelectedItem();

    /// @brief 删除全部选中图形（无选中则无操作）。
    void deleteSelectedItems();

    /// @brief 把当前选中图形写入剪贴板（m_clipboardShape + 重置 m_pasteCount）。
    void copySelectedItem();

    /// @brief 从剪贴板粘贴。多次粘贴以 16 像素为步长依次偏移。
    void pasteCopiedItem();

    /// @brief 清空画布、重置 z 计数器、释放预览节点。
    void clearCanvas();

    /// @brief 把当前画布（不含 m_previewItem）序列化为 DocumentData。
    /// @note  内部按 z 升序排序，保证磁盘中图形顺序与显示一致。
    DocumentData documentData() const;

    /// @brief 用 DocumentData 替换当前画布。
    void loadDocument(const DocumentData& document);

    /// @brief 把当前画布渲染为 PNG 等格式输出。
    /// @param filePath      目标文件路径（扩展名决定格式）
    /// @param errorMessage  可选输出：失败时填充错误描述
    /// @return true 表示保存成功
    bool exportImage(const QString& filePath, QString* errorMessage = nullptr) const;

    /// @brief 当前选中图形数量。
    int selectedShapeCount() const;

  signals:
    /// @brief 选中项变更时发出；仅单选时 primaryItem 非空。
    void selectionStateChanged(ShapeItem* primaryItem, int selectedCount);

    /// @brief 状态栏提示文本变更（如"当前工具：xxx"、"图形已复制"）。
    void statusMessageChanged(const QString& message);

    /// @brief Delete / Backspace 请求，由 MainWindow 决定是否二次确认。
    void deleteSelectionRequested();

  protected:
    /// @brief 根据工具分支：Point 直接落点；drag 工具调 beginDragShape；path 工具调 beginPathPoint。
    void mousePressEvent(QMouseEvent* event) override;

    /// @brief 拖拽中实时更新预览；path 工具中按光标位置延展最后一个顶点。
    void mouseMoveEvent(QMouseEvent* event) override;

    /// @brief 拖拽中左键释放时调 finishDragShape。
    void mouseReleaseEvent(QMouseEvent* event) override;

    /// @brief path 工具中双击结束绘制（与 Enter 键等价）。
    void mouseDoubleClickEvent(QMouseEvent* event) override;

    /// @brief Enter 结束 path 绘制；Esc 取消；Delete/Backspace 删除；Ctrl+C/V 复制粘贴。
    void keyPressEvent(QKeyEvent* event) override;

  private:
    /// @brief 在 m_scene->selectedItems() 中寻找第一个 ShapeItem。
    ShapeItem* selectedShapeItem() const;

    /// @brief 返回全部选中 ShapeItem。
    QList<ShapeItem*> selectedShapeItems() const;

    /// @brief scene::selectionChanged → 通知 MainWindow / PropertyPanel。
    void handleSelectionChanged();

    /// @brief 通用入口：构造 ShapeItem、加入 scene、（可选）选中。
    /// @details 还会把 ShapeItem::shapeChanged 桥接到刷新样式与通知。
    /// @param data           待加入的图形
    /// @param selectNewItem  是否清空当前选中并选中新图形
    void addShape(const ShapeData& data, bool selectNewItem = true);

    /// @brief 拖拽工作流 Step 1：记录起点、创建预览 ShapeItem。
    void beginDragShape(const QPointF& scenePoint);

    /// @brief 拖拽工作流 Step 2：随光标位置更新预览。
    void updateDragPreview(const QPointF& scenePoint);

    /// @brief 拖拽工作流 Step 3：释放鼠标时把预览替换为正式图形（满足最小尺寸才落格）。
    void finishDragShape(const QPointF& scenePoint);

    /// @brief 路径工作流 Step 1：添加一个顶点；首次调用时同时创建预览。
    void beginPathPoint(const QPointF& scenePoint);

    /// @brief 路径工作流 Step 2：随光标延展最后一个"幽灵"顶点。
    void updatePathPreview(const QPointF& scenePoint);

    /// @brief 路径工作流 Step 3：结束绘制；closed=true 时多边形自动闭合。
    void finishPathShape(bool closed);

    /// @brief 完整复位所有"正在绘制"状态：drag 标志、path 顶点、预览节点。
    void cancelDrawing();

    /// @brief 把当前 drag 起点 + 终点构造成一个 ShapeData（用于预览 / 落格）。
    ShapeData buildDragShapeData(const QPointF& endPoint) const;

    /// @brief 把当前 path 顶点 + 光标位置构造成一个 ShapeData（用于预览）。
    ShapeData buildPathPreviewData(const QPointF& cursorPoint) const;

    /// @brief 根据当前样式与 type，返回一个 style（不支持填充的 type 强制关闭 fillEnabled）。
    ShapeStyle currentStyleFor(ShapeType type) const;

    /// @brief 把当前选中信息 emit 给监听者。
    void refreshSelectionNotification();

    /// @brief 刷新选择变换覆盖层。
    void updateSelectionOverlay();

    /// @brief 同步 ShapeItem 自带选中框的显示策略。
    void updateSelectionDecorations();

    /// @brief 命中锚点后初始化缩放/旋转会话。
    void beginTransformSession(SelectionTransformOverlayItem::Handle handle, const QPointF& scenePoint);

    /// @brief 根据当前鼠标位置更新缩放/旋转预览。
    void updateTransformSession(const QPointF& scenePoint, Qt::KeyboardModifiers modifiers);

    /// @brief 结束缩放/旋转会话，保留当前预览结果。
    void finishTransformSession();

    /// @brief 取消缩放/旋转会话，回滚到开始拖拽前。
    void cancelTransformSession();

    /// @brief 单调递增的 z 值生成器，每次创建图形 +1。
    /// @return 新 z 值
    qreal nextZValue();

    /// @brief 持有的 QGraphicsScene；构造时创建，父对象为 this
    QGraphicsScene* m_scene = nullptr;

    /// @brief 当前界面语言（影响状态栏文案）
    AppLanguage m_language = AppLanguage::SimplifiedChinese;

    /// @brief 当前工具
    Tool m_tool = Tool::Select;

    /// @brief 当前正在使用 / 上次使用的样式
    ShapeStyle m_currentStyle;

    /// @brief drag 工作流：起点（场景坐标）
    QPointF m_dragStartPoint;

    /// @brief drag 工作流：是否正在拖拽
    bool m_dragDrawing = false;

    /// @brief path 工作流：已确认的顶点列表
    QVector<QPointF> m_pathPoints;

    /// @brief 当前正在显示的预览节点（drag / path 共享）
    ShapeItem* m_previewItem = nullptr;

    /// @brief 内存剪贴板（仅保留最后一份）
    std::optional<ShapeData> m_clipboardShape;

    /// @brief z 计数器；创建图形 / 加载文档时更新
    qreal m_zCounter = 0.0;

    /// @brief 已连续粘贴次数（用于 16 像素的累计偏移）
    int m_pasteCount = 0;

    /// @brief 选择变换可视覆盖层
    SelectionTransformOverlayItem* m_selectionOverlay = nullptr;

    struct TransformSelectionEntry {
        /// @brief 该会话涉及的图形指针
        ShapeItem* item = nullptr;
        /// @brief 缩放/旋转开始前的 ShapeData 快照，用于取消时回滚
        ShapeData originalData;
    };

    struct TransformSession {
        /// @brief 是否处于缩放/旋转会话中
        bool active = false;
        /// @brief 当前被拖拽的手柄（4 角点之一 = 缩放；Rotate = 旋转）
        SelectionTransformOverlayItem::Handle handle = SelectionTransformOverlayItem::Handle::None;
        /// @brief 每个被变换图形的起始快照 + 指针
        QList<TransformSelectionEntry> entries;
        /// @brief 会话开始时整个选区的外接矩形
        QRectF originalBounds;
        /// @brief 缩放会话的固定锚点（与被拖拽角点对角的点）
        QPointF pivot;
        /// @brief 选区中心（用于旋转会话的旋转中心）
        QPointF center;
        /// @brief 会话开始时鼠标所在的场景坐标
        QPointF startPoint;
        /// @brief 会话开始时鼠标相对 `center` 的角度（仅旋转使用）
        qreal startAngle = 0.0;
    };

    /// @brief 当前缩放/旋转会话状态
    TransformSession m_transformSession;

    /// @brief Select 工具下，按下到某个图形上后等待是否进入拖动。
    bool m_selectionMoveCandidate = false;

    /// @brief 当前是否处于拖动已选图形的可视反馈阶段。
    bool m_selectionMoveActive = false;

    /// @brief 记录按下时的视口坐标，用于判定是否越过拖动阈值。
    QPoint m_selectionMovePressViewPos;
};
