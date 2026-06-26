// =====================================================================
// PropertyPanel.h
// ---------------------------------------------------------------------
// @brief   右侧属性面板：当前选中图形的几何 / 描边 / 填充编辑器
// @details PropertyPanel 是一个"反向控制器"——它持有当前图形的拷贝，
//          用户每改一个控件就会 emit `shapeEdited(data)` 给 MainWindow
//          桥接到 CanvasView::updateSelectedShape。整套交互是即时的，
//          不存在 Apply / Cancel 按钮。
// @layer   ui
// @warning `m_updatingWidgets` 是防重入标志：setShapeData 期间由程序触发的
//          valueChanged 不会再次 emit shapeEdited，否则会形成
//          setShapeData → emit → updateSelectedShape → selectionChanged
//          → setShapeData 的死循环。
// =====================================================================

#pragma once

#include <QFormLayout>
#include <QWidget>

#include "AppLanguage.h"
#include "ShapeData.h"

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QPushButton;

/// @brief 图形属性编辑器（dock widget 内的子 widget）。
class PropertyPanel : public QWidget {
    Q_OBJECT

  public:
    /// @brief 构造。内部创建所有子控件并调用 clearSelection 进入"无选中"态。
    explicit PropertyPanel(QWidget* parent = nullptr);

    /// @brief 切换界面语言。重复设置同语言会被忽略。
    void setLanguage(AppLanguage language);

    /// @brief 进入"无选中"态：清空所有字段、禁用输入控件。
    void clearSelection();

    /// @brief 用 data 填充所有字段。会自动根据 type 切换可见的几何编辑项。
    /// @details 调用期间 m_updatingWidgets == true，控件的 valueChanged 不会触发 emit。
    void setShapeData(const ShapeData& data);

  signals:
    /// @brief 任何控件被用户修改后发出（携带最新的 ShapeData 完整快照）。
    void shapeEdited(const ShapeData& data);

  private:
    /// @brief 创建子控件、布局、连接信号。
    void setupUi();

    /// @brief 把所有"硬编码的中英文字符串"重置为当前语言的版本。
    void retranslateUi();

    /// @brief 把颜色按钮的 QSS 同步到当前颜色，便于直接看到选中色。
    void updateButtons();

    /// @brief 根据 m_currentData.type 切换几何字段（不同类型显示不同字段）。
    /// @details Point/Line 显示 2~4 个坐标字段；Circle/Ellipse 显示 cx/cy/r 或 cx/cy/rx/ry；
    ///          Rectangle 显示 x/y/width/height；Polyline/Polygon 仅允许整体平移。
    void updateGeometryControls();

    /// @brief 同步标题与提示文本（与是否有选中、当前语言相关）。
    void updateSelectionTexts();

    /// @brief 把当前所有控件值组装回 ShapeData 并 emit shapeEdited。
    /// @note  内部会过滤 m_updatingWidgets == true 的情况。
    void emitEditedShape();

    /// @brief 重建 m_lineStyleCombo 的条目（语言变更时需要重写标签）。
    void rebuildLineStyleCombo();

    /// @brief 面板顶部标题
    QLabel* m_titleLabel = nullptr;
    /// @brief 标题下方的提示文本
    QLabel* m_hintLabel = nullptr;
    /// @brief 6 个几何字段标签（与 m_geometryEdits 一一对应）
    QLabel* m_geometryLabels[6] = {};
    /// @brief 6 个几何编辑框；范围 ±100000，精度 2 位，单步 1
    QDoubleSpinBox* m_geometryEdits[6] = {};
    /// @brief 描边颜色按钮（点击弹出 QColorDialog）
    QPushButton* m_strokeColorButton = nullptr;
    /// @brief 填充颜色按钮
    QPushButton* m_fillColorButton = nullptr;
    /// @brief 描边宽度输入（范围 0.5~20.0，精度 1 位）
    QDoubleSpinBox* m_lineWidthSpin = nullptr;
    /// @brief 描边线型下拉框（Solid / Dash / Dot / DashDot）
    QComboBox* m_lineStyleCombo = nullptr;
    /// @brief 是否启用填充
    QCheckBox* m_fillEnabledCheck = nullptr;
    /// @brief 表单布局；用于在 retranslateUi 中按字段查找其标签
    QFormLayout* m_formLayout = nullptr;

    /// @brief 面板内持有的当前数据副本
    ShapeData m_currentData;
    /// @brief 当前界面语言
    AppLanguage m_language = AppLanguage::SimplifiedChinese;
    /// @brief 是否处于"有选中"态
    bool m_hasSelection = false;
    /// @brief 防重入标志；setShapeData / rebuildLineStyleCombo 期间为 true
    bool m_updatingWidgets = false;
};
