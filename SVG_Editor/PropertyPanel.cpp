// =====================================================================
// PropertyPanel.cpp
// ---------------------------------------------------------------------
// @brief PropertyPanel 的实现
// @details 本文件实现"无 Apply/Cancel 的实时编辑器"：
//          - 任意 valueChanged / 按钮点击都会立即构造完整 ShapeData 并 emit；
//          - 程序侧更新（setShapeData / rebuildLineStyleCombo）通过
//            m_updatingWidgets 标志位抑制 emit 反馈。
// @layer   ui
// =====================================================================

#include "PropertyPanel.h"

#include <QCheckBox>
#include <QColorDialog>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

namespace {

/// @brief 二选一返回字符串（与 CanvasView/MainWindow 中的同名工具一致）。
QString textForLanguage(AppLanguage language, const QString& english, const QString& chinese) {
    return language == AppLanguage::SimplifiedChinese ? chinese : english;
}

/// @brief 在 QFormLayout 中按 field 找到其左侧的 QLabel 并设置文本。
void setFormLabelText(QFormLayout* layout, QWidget* field, const QString& text) {
    if (layout == nullptr || field == nullptr) {
        return;
    }

    auto* label = qobject_cast<QLabel*>(layout->labelForField(field));
    if (label != nullptr) {
        label->setText(text);
    }
}

/// @brief 把颜色渲染为 QSS 字符串，作为颜色按钮的背景。
QString colorButtonStyle(const QColor& color) {
    return QString("background:%1; border:1px solid #909090; min-height:24px;").arg(color.name(QColor::HexArgb));
}

} // namespace

PropertyPanel::PropertyPanel(QWidget* parent) : QWidget(parent) {
    setupUi();
    // 构造完毕先进入"无选中"态，避免显示空数据
    clearSelection();
}

void PropertyPanel::setLanguage(AppLanguage language) {
    if (m_language == language) {
        return;
    }

    m_language = language;
    retranslateUi();
}

void PropertyPanel::clearSelection() {
    m_hasSelection = false;
    updateSelectionTexts();

    // 隐藏所有几何编辑字段
    for (int index = 0; index < 6; ++index) {
        m_geometryLabels[index]->hide();
        m_geometryEdits[index]->hide();
    }

    // 禁用所有可交互控件
    m_strokeColorButton->setEnabled(false);
    m_fillColorButton->setEnabled(false);
    m_lineWidthSpin->setEnabled(false);
    m_lineStyleCombo->setEnabled(false);
    m_fillEnabledCheck->setEnabled(false);
    updateButtons();
}

void PropertyPanel::setShapeData(const ShapeData& data) {
    // 关键：进入"程序更新中"状态，避免 setValue 触发的 valueChanged 形成回环
    m_updatingWidgets = true;
    m_currentData = normalizedShapeData(data);
    m_hasSelection = true;

    updateSelectionTexts();
    updateGeometryControls();

    // 启用样式相关控件；填充相关控件还要看 type 是否支持
    m_lineWidthSpin->setEnabled(true);
    m_lineStyleCombo->setEnabled(true);
    m_strokeColorButton->setEnabled(true);
    m_fillEnabledCheck->setEnabled(shapeSupportsFill(m_currentData.type));
    m_fillColorButton->setEnabled(shapeSupportsFill(m_currentData.type));

    rebuildLineStyleCombo();
    m_lineWidthSpin->setValue(m_currentData.style.strokeWidth);
    const int currentIndex = m_lineStyleCombo->findData(static_cast<int>(m_currentData.style.strokeStyle));
    if (currentIndex >= 0) {
        m_lineStyleCombo->setCurrentIndex(currentIndex);
    }
    m_fillEnabledCheck->setChecked(shapeSupportsFill(m_currentData.type) && m_currentData.style.fillEnabled);
    updateButtons();
    m_updatingWidgets = false;
}

void PropertyPanel::setupUi() {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(12, 12, 12, 12);
    layout->setSpacing(12);

    m_titleLabel = new QLabel(this);
    m_titleLabel->setStyleSheet("font-weight:600; font-size:16px;");
    layout->addWidget(m_titleLabel);

    m_hintLabel = new QLabel(this);
    m_hintLabel->setWordWrap(true);
    layout->addWidget(m_hintLabel);

    m_formLayout = new QFormLayout();
    m_formLayout->setLabelAlignment(Qt::AlignLeft);
    m_formLayout->setFormAlignment(Qt::AlignTop);
    m_formLayout->setSpacing(8);

    // 创建 6 个几何字段；其可见性由 updateGeometryControls 决定
    for (int index = 0; index < 6; ++index) {
        m_geometryLabels[index] = new QLabel(this);
        m_geometryEdits[index] = new QDoubleSpinBox(this);
        m_geometryEdits[index]->setRange(-100000.0, 100000.0);
        m_geometryEdits[index]->setDecimals(2);
        m_geometryEdits[index]->setSingleStep(1.0);
        m_formLayout->addRow(m_geometryLabels[index], m_geometryEdits[index]);

        // 任意字段变化都触发重新组装 + emit
        connect(m_geometryEdits[index], QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
                [this](double) { emitEditedShape(); });
    }

    m_strokeColorButton = new QPushButton(this);
    connect(m_strokeColorButton, &QPushButton::clicked, this, [this]() {
        if (!m_hasSelection) {
            return;
        }

        const QColor color = QColorDialog::getColor(m_currentData.style.strokeColor, this,
                                                    textForLanguage(m_language, "Select Stroke Color", "选择描边颜色"));
        if (!color.isValid()) {
            return;
        }

        m_currentData.style.strokeColor = color;
        updateButtons();
        emit shapeEdited(m_currentData);
    });
    m_formLayout->addRow(QString(), m_strokeColorButton);

    m_lineWidthSpin = new QDoubleSpinBox(this);
    m_lineWidthSpin->setRange(0.5, 20.0);
    m_lineWidthSpin->setDecimals(1);
    m_lineWidthSpin->setSingleStep(0.5);
    connect(m_lineWidthSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [this](double value) {
        if (m_updatingWidgets || !m_hasSelection) {
            return;
        }
        m_currentData.style.strokeWidth = value;
        emit shapeEdited(m_currentData);
    });
    m_formLayout->addRow(QString(), m_lineWidthSpin);

    m_lineStyleCombo = new QComboBox(this);
    connect(m_lineStyleCombo, &QComboBox::currentTextChanged, this, [this](const QString& text) {
        if (m_updatingWidgets || !m_hasSelection) {
            return;
        }
        Q_UNUSED(text);
        m_currentData.style.strokeStyle = static_cast<Qt::PenStyle>(m_lineStyleCombo->currentData().toInt());
        emit shapeEdited(m_currentData);
    });
    m_formLayout->addRow(QString(), m_lineStyleCombo);

    m_fillEnabledCheck = new QCheckBox(this);
    connect(m_fillEnabledCheck, &QCheckBox::checkStateChanged, this, [this](Qt::CheckState state) {
        if (m_updatingWidgets || !m_hasSelection) {
            return;
        }
        // 即使用户勾选，不支持填充的 type 也会被强制关闭
        m_currentData.style.fillEnabled = shapeSupportsFill(m_currentData.type) && state == Qt::Checked;
        updateButtons();
        emit shapeEdited(m_currentData);
    });
    m_formLayout->addRow(QString(), m_fillEnabledCheck);

    m_fillColorButton = new QPushButton(this);
    connect(m_fillColorButton, &QPushButton::clicked, this, [this]() {
        if (!m_hasSelection || !shapeSupportsFill(m_currentData.type)) {
            return;
        }

        const QColor color = QColorDialog::getColor(m_currentData.style.fillColor, this,
                                                    textForLanguage(m_language, "Select Fill Color", "选择填充颜色"));
        if (!color.isValid()) {
            return;
        }

        m_currentData.style.fillColor = color;
        updateButtons();
        emit shapeEdited(m_currentData);
    });
    m_formLayout->addRow(QString(), m_fillColorButton);

    layout->addLayout(m_formLayout);
    layout->addStretch();
    retranslateUi();
}

void PropertyPanel::updateButtons() {
    // 把当前颜色同步到按钮的 QSS，方便用户直接看到选中色
    m_strokeColorButton->setStyleSheet(colorButtonStyle(m_currentData.style.strokeColor));
    m_fillColorButton->setStyleSheet(colorButtonStyle(m_currentData.style.fillColor));
    m_fillColorButton->setEnabled(m_hasSelection && shapeSupportsFill(m_currentData.type));
}

void PropertyPanel::retranslateUi() {
    rebuildLineStyleCombo();
    updateSelectionTexts();

    m_strokeColorButton->setText(textForLanguage(m_language, "Stroke Color", "描边颜色"));
    m_fillColorButton->setText(textForLanguage(m_language, "Fill Color", "填充颜色"));
    m_fillEnabledCheck->setText(textForLanguage(m_language, "Enable Fill", "启用填充"));

    // 表单左侧标签需要单独刷新
    if (m_formLayout != nullptr) {
        setFormLabelText(m_formLayout, m_strokeColorButton, textForLanguage(m_language, "Stroke", "描边"));
        setFormLabelText(m_formLayout, m_lineWidthSpin, textForLanguage(m_language, "Line Width", "线宽"));
        setFormLabelText(m_formLayout, m_lineStyleCombo, textForLanguage(m_language, "Line Style", "线型"));
        setFormLabelText(m_formLayout, m_fillEnabledCheck, textForLanguage(m_language, "Fill", "填充"));
        setFormLabelText(m_formLayout, m_fillColorButton, textForLanguage(m_language, "Fill Color", "填充颜色"));
    }
}

void PropertyPanel::updateSelectionTexts() {
    if (!m_hasSelection) {
        m_titleLabel->setText(textForLanguage(m_language, "No selection", "未选中图形"));
        m_hintLabel->setText(textForLanguage(m_language, "Select a shape to edit geometry and style.",
                                             "请选择一个图形后再编辑几何参数和样式。"));
        return;
    }

    m_titleLabel->setText(shapeDisplayName(m_currentData.type, m_language));
    m_hintLabel->setText(
        textForLanguage(m_language, "Geometry updates are applied immediately.", "修改参数后会立即更新画布。"));
}

void PropertyPanel::rebuildLineStyleCombo() {
    // 先记下当前选中值（用 data 而非 text，因为 text 跟语言相关）
    const QVariant currentData = m_lineStyleCombo->currentData();

    m_updatingWidgets = true;
    m_lineStyleCombo->clear();
    m_lineStyleCombo->addItem(penStyleDisplayName(Qt::SolidLine, m_language), static_cast<int>(Qt::SolidLine));
    m_lineStyleCombo->addItem(penStyleDisplayName(Qt::DashLine, m_language), static_cast<int>(Qt::DashLine));
    m_lineStyleCombo->addItem(penStyleDisplayName(Qt::DotLine, m_language), static_cast<int>(Qt::DotLine));
    m_lineStyleCombo->addItem(penStyleDisplayName(Qt::DashDotLine, m_language), static_cast<int>(Qt::DashDotLine));

    // 恢复原选中
    const int index = m_lineStyleCombo->findData(currentData);
    if (index >= 0) {
        m_lineStyleCombo->setCurrentIndex(index);
    }
    m_updatingWidgets = false;
}

void PropertyPanel::updateGeometryControls() {
    // 先全部隐藏，再按 type 决定显示哪些字段
    for (int index = 0; index < 6; ++index) {
        m_geometryLabels[index]->hide();
        m_geometryEdits[index]->hide();
    }

    // 局部辅助：显示一个字段并设置当前值
    auto showField = [this](int index, const QString& label, double value) {
        m_geometryLabels[index]->setText(label);
        m_geometryEdits[index]->setValue(value);
        m_geometryLabels[index]->show();
        m_geometryEdits[index]->show();
    };

    // 局部辅助：单独设置某个字段的取值范围（默认 -100000~100000）
    auto setFieldRange = [this](int index, double minimum, double maximum) {
        m_geometryEdits[index]->setRange(minimum, maximum);
    };

    switch (m_currentData.type) {
    case ShapeType::Point:
        // Point：单点 → x/y
        if (!m_currentData.points.isEmpty()) {
            showField(0, "x", m_currentData.points.first().x());
            showField(1, "y", m_currentData.points.first().y());
        }
        break;
    case ShapeType::Line:
        // Line：起终点 → x1/y1/x2/y2
        if (m_currentData.points.size() >= 2) {
            showField(0, "x1", m_currentData.points.at(0).x());
            showField(1, "y1", m_currentData.points.at(0).y());
            showField(2, "x2", m_currentData.points.at(1).x());
            showField(3, "y2", m_currentData.points.at(1).y());
        }
        break;
    case ShapeType::Polyline:
    case ShapeType::Polygon: {
        // Polyline/Polygon 当前不支持逐顶点编辑，仅允许整体平移 → 只显示 bbox 的 x/y
        const QRectF bounds = pointsBoundingRect(m_currentData.points);
        showField(0, "x", bounds.x());
        showField(1, "y", bounds.y());
        break;
    }
    case ShapeType::Circle:
        // Circle：cx/cy/r，r 范围 [0, 100000]
        showField(0, "cx", m_currentData.rect.center().x());
        showField(1, "cy", m_currentData.rect.center().y());
        setFieldRange(2, 0.0, 100000.0);
        showField(2, "r", m_currentData.rect.width() / 2.0);
        break;
    case ShapeType::Ellipse:
        // Ellipse：cx/cy/rx/ry，半轴非负
        showField(0, "cx", m_currentData.rect.center().x());
        showField(1, "cy", m_currentData.rect.center().y());
        setFieldRange(2, 0.0, 100000.0);
        setFieldRange(3, 0.0, 100000.0);
        showField(2, "rx", m_currentData.rect.width() / 2.0);
        showField(3, "ry", m_currentData.rect.height() / 2.0);
        break;
    case ShapeType::Rectangle:
        // Rectangle：x/y/width/height，宽高非负
        showField(0, "x", m_currentData.rect.x());
        showField(1, "y", m_currentData.rect.y());
        setFieldRange(2, 0.0, 100000.0);
        setFieldRange(3, 0.0, 100000.0);
        showField(2, "width", m_currentData.rect.width());
        showField(3, "height", m_currentData.rect.height());
        break;
    }
}

void PropertyPanel::emitEditedShape() {
    if (m_updatingWidgets || !m_hasSelection) {
        return;
    }

    ShapeData data = m_currentData;
    switch (data.type) {
    case ShapeType::Point:
        if (!data.points.isEmpty()) {
            data.points[0] = QPointF(m_geometryEdits[0]->value(), m_geometryEdits[1]->value());
        }
        break;
    case ShapeType::Line:
        if (data.points.size() >= 2) {
            data.points[0] = QPointF(m_geometryEdits[0]->value(), m_geometryEdits[1]->value());
            data.points[1] = QPointF(m_geometryEdits[2]->value(), m_geometryEdits[3]->value());
        }
        break;
    case ShapeType::Polyline:
    case ShapeType::Polygon: {
        // 把 bbox 起点平移到新位置（保持宽高不变）
        const QRectF currentBounds = pointsBoundingRect(data.points);
        const QPointF delta(m_geometryEdits[0]->value() - currentBounds.x(),
                            m_geometryEdits[1]->value() - currentBounds.y());
        translateShapeData(data, delta);
        break;
    }
    case ShapeType::Circle: {
        // 用 center + r 重建正方形外接框
        const qreal radius = m_geometryEdits[2]->value();
        const QPointF center(m_geometryEdits[0]->value(), m_geometryEdits[1]->value());
        data.rect = QRectF(center.x() - radius, center.y() - radius, radius * 2.0, radius * 2.0);
        break;
    }
    case ShapeType::Ellipse: {
        // 用 center + 半轴重建
        const qreal rx = m_geometryEdits[2]->value();
        const qreal ry = m_geometryEdits[3]->value();
        const QPointF center(m_geometryEdits[0]->value(), m_geometryEdits[1]->value());
        data.rect = QRectF(center.x() - rx, center.y() - ry, rx * 2.0, ry * 2.0);
        break;
    }
    case ShapeType::Rectangle:
        // 直接 4 个标量
        data.rect = QRectF(m_geometryEdits[0]->value(), m_geometryEdits[1]->value(), m_geometryEdits[2]->value(),
                           m_geometryEdits[3]->value());
        break;
    }

    m_currentData = normalizedShapeData(data);
    emit shapeEdited(m_currentData);
}
