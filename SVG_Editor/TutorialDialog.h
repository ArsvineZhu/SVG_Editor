// =====================================================================
// TutorialDialog.h
// ---------------------------------------------------------------------
// @brief   双语操作手册对话框
// @details 在 Tutorial 菜单 → Operation Manual 中打开，显示一份覆盖界面布局、
//          绘图工具、Polyline/Polygon 详细说明、文件操作、快捷键等的 HTML 手册。
//          内容由 `buildManualHtml()` 产出，不依赖外部 .ts 翻译文件。
// @layer   ui
// =====================================================================

#pragma once

#include <QDialog>

#include "AppLanguage.h"

class QPushButton;
class QTextBrowser;

/// @brief 内置双语操作手册的模态对话框。
class TutorialDialog : public QDialog {
    Q_OBJECT

  public:
    /// @brief 构造。完成 UI 初始化 + 首次 retranslateUi。
    explicit TutorialDialog(QWidget* parent = nullptr);

    /// @brief 切换手册语言；同语言重复设置会被忽略。
    void setLanguage(AppLanguage language);

  private:
    /// @brief 创建 QTextBrowser 与关闭按钮。
    void setupUi();

    /// @brief 重新构造窗口标题、按钮文本、HTML 内容。
    void retranslateUi();

    /// @brief 按当前语言产出完整 HTML 字符串（包含界面、工具、文件、快捷键等章节）。
    QString buildManualHtml() const;

    /// @brief 显示手册内容的 QTextBrowser；外部链接被关闭以避免误跳
    QTextBrowser* m_browser = nullptr;
    /// @brief 关闭按钮（accept）
    QPushButton* m_closeButton = nullptr;
    /// @brief 当前手册语言
    AppLanguage m_language = AppLanguage::SimplifiedChinese;
};
