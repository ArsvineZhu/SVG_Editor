// =====================================================================
// MainWindow.h
// ---------------------------------------------------------------------
// @brief   应用主窗口：协调 CanvasView、PropertyPanel、TutorialDialog、菜单/工具栏
// @details 职责划分：
//          - 菜单 / 工具栏：把所有用户入口（File / Edit / Tools / Tutorial）串起来；
//          - 文件 I/O：通过 FileManager 与磁盘交互；当前文件路径保存在 m_currentFilePath；
//          - i18n：通过 QSettings 持久化用户语言选择（ui/language）；
//          - 桥接信号：把 CanvasView 的 selectionChanged / statusMessageChanged
//            转发到 PropertyPanel / 状态栏。
// @layer   ui
// @warning `createToolAction` 创建的 QAction 在 QActionGroup 中是 exclusive 的，
//          但 `connectSignals` 又把每个 action 的 triggered 单独桥接到
//          `setTool(tool)`。这种"组 + 单独"双重绑定的安全性依赖于
//          QActionGroup::setExclusive(true) 的内置排他。
// =====================================================================

#pragma once

#include <QMenu>
#include <QMainWindow>
#include <QToolBar>

#include "AppLanguage.h"
#include "CanvasView.h"

class PropertyPanel;
class QAction;
class QActionGroup;
class QDockWidget;
class TutorialDialog;

/// @brief 应用主窗口（唯一顶层 QMainWindow）。
class MainWindow : public QMainWindow {
    Q_OBJECT

  public:
    /// @brief 构造。完成 UI / Action / 工具栏 / 菜单 / 信号连接 + 应用持久化语言。
    explicit MainWindow(QWidget* parent = nullptr);

  private:
    /// @brief 从 QSettings 读取上次保存的语言；缺失则默认简体中文。
    /// @return 读取到的语言
    AppLanguage loadLanguage() const;

    /// @brief 把当前语言写入 QSettings，key = "ui/language"。
    void saveLanguage() const;

    /// @brief 切换界面语言：保存持久化、通知子模块、刷新菜单勾选、retranslateUi。
    void setLanguage(AppLanguage language);

    /// @brief 创建画布 / 属性面板 dock / 教程对话框。
    void setupUi();

    /// @brief 创建所有 QAction 与 ActionGroup，并绑定快捷键。
    void setupActions();

    /// @brief 把 QAction 挂到 menuBar；语言子菜单作为 Tutorial 菜单的子菜单。
    void setupMenus();

    /// @brief 在工具栏里铺上工具按钮 + 常用动作；锁定不可拖动。
    void setupToolbar();

    /// @brief 把所有 action / 子模块信号接到对应槽。
    void connectSignals();

    /// @brief 把所有"硬编码的中英文字符串"刷为当前语言版本。
    void retranslateUi();

    /// @brief 同步窗口标题（含当前文件名 / Untitled）。
    void updateWindowTitle();

    /// @brief 弹出操作手册对话框。
    void showTutorial();

    /// @brief 工具按钮工厂：创建 + 加入工具 ActionGroup + 绑定 setTool 槽。
    /// @param tool 工具枚举
    /// @return 新建的 QAction（生命周期由 this 接管）
    QAction* createToolAction(CanvasView::Tool tool);

    /// @brief 文件写入的统一入口（被 saveDocument / saveDocumentAs 调用）。
    /// @param filePath 目标文件
    /// @return true 表示成功
    bool saveToPath(const QString& filePath);

    /// @brief 走 saveToPath 流程：m_currentFilePath 非空则直接覆盖，否则转 saveDocumentAs。
    /// @return true 表示成功
    bool saveDocument();

    /// @brief 弹出 QFileDialog 选路径，然后 saveToPath。
    /// @return true 表示成功；用户取消时返回 false
    bool saveDocumentAs();

    /// @brief 弹出 QFileDialog 选文件，读取后通过 m_canvasView->loadDocument 替换画布。
    void openDocument();

    /// @brief 弹出 QFileDialog 选 png 路径，调用 CanvasView::exportImage。
    void exportImage();

    /// @brief 中央画布
    CanvasView* m_canvasView = nullptr;
    /// @brief 右侧属性面板
    PropertyPanel* m_propertyPanel = nullptr;
    /// @brief 容纳属性面板的 dock
    QDockWidget* m_propertyDock = nullptr;
    /// @brief 操作手册对话框（懒构造也行，这里直接构造）
    TutorialDialog* m_tutorialDialog = nullptr;
    /// @brief 工具栏
    QToolBar* m_toolBar = nullptr;
    /// @brief File 菜单
    QMenu* m_fileMenu = nullptr;
    /// @brief Edit 菜单
    QMenu* m_editMenu = nullptr;
    /// @brief Tools 菜单
    QMenu* m_toolMenu = nullptr;
    /// @brief Tutorial 菜单
    QMenu* m_tutorialMenu = nullptr;
    /// @brief Tutorial 菜单下的语言子菜单
    QMenu* m_languageMenu = nullptr;

    /// @brief File → Open
    QAction* m_openAction = nullptr;
    /// @brief File → Save
    QAction* m_saveAction = nullptr;
    /// @brief File → Save As
    QAction* m_saveAsAction = nullptr;
    /// @brief File → Export Image
    QAction* m_exportAction = nullptr;
    /// @brief File → Exit
    QAction* m_exitAction = nullptr;
    /// @brief Edit → Delete
    QAction* m_deleteAction = nullptr;
    /// @brief Edit → Copy
    QAction* m_copyAction = nullptr;
    /// @brief Edit → Paste
    QAction* m_pasteAction = nullptr;
    /// @brief Edit → Clear Canvas
    QAction* m_clearAction = nullptr;
    /// @brief Tutorial → Operation Manual
    QAction* m_showTutorialAction = nullptr;
    /// @brief Language → English
    QAction* m_englishAction = nullptr;
    /// @brief Language → 简体中文
    QAction* m_simplifiedChineseAction = nullptr;

    /// @brief 工具按钮的互斥组（保证只有一个工具处于选中态）
    QActionGroup* m_toolActionGroup = nullptr;
    /// @brief 语言切换的互斥组
    QActionGroup* m_languageActionGroup = nullptr;
    /// @brief 当前语言
    AppLanguage m_language = AppLanguage::SimplifiedChinese;
    /// @brief 当前已打开文件路径；空表示 Untitled
    QString m_currentFilePath;
};
