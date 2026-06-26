// =====================================================================
// TutorialDialog.cpp
// ---------------------------------------------------------------------
// @brief TutorialDialog 的实现
// @details buildManualHtml() 内部包含大段 HTML 字符串（中英两版），
//          它们是用户文档而非代码逻辑，因此本文件不逐行注释其内容。
// @layer   ui
// =====================================================================

#include "TutorialDialog.h"

#include <QPushButton>
#include <QTextBrowser>
#include <QVBoxLayout>

namespace {

/// @brief 二选一返回字符串。
QString textForLanguage(AppLanguage language, const QString& english, const QString& chinese) {
    return language == AppLanguage::SimplifiedChinese ? chinese : english;
}

} // namespace

TutorialDialog::TutorialDialog(QWidget* parent) : QDialog(parent) {
    setupUi();
    retranslateUi();
}

void TutorialDialog::setLanguage(AppLanguage language) {
    if (m_language == language) {
        return;
    }

    m_language = language;
    retranslateUi();
}

void TutorialDialog::setupUi() {
    resize(900, 720);
    setModal(true);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(12, 12, 12, 12);
    layout->setSpacing(12);

    m_browser = new QTextBrowser(this);
    // 关闭外链跳转，避免用户点击 HTML 中的外部链接被强制打开浏览器
    m_browser->setOpenExternalLinks(false);
    layout->addWidget(m_browser, 1);

    m_closeButton = new QPushButton(this);
    connect(m_closeButton, &QPushButton::clicked, this, &QDialog::accept);
    layout->addWidget(m_closeButton, 0, Qt::AlignRight);
}

void TutorialDialog::retranslateUi() {
    setWindowTitle(textForLanguage(m_language, "Tutorial", "Tutorial"));
    m_browser->setHtml(buildManualHtml());
    m_closeButton->setText(textForLanguage(m_language, "Close", "关闭"));
}

/// @brief 构造当前语言版本的手册 HTML。内容覆盖：
///        1) 界面布局、2) 绘图工具、3) Polyline/Polygon 详解、
///        4) 选择与编辑、5) 文件与图片、6) 复制粘贴删除、7) 快捷键、
///        8) 语言切换、9) 当前实现边界。
QString TutorialDialog::buildManualHtml() const {
    if (m_language == AppLanguage::SimplifiedChinese) {
        return QStringLiteral(R"(
<h1>简单矢量图形处理系统操作手册</h1>
<p>本手册覆盖当前版本中主窗口、绘图工具、编辑操作、文件操作、折线与多边形的具体用法。</p>

<h2>1. 界面组成</h2>
<ul>
  <li><b>File</b>：打开、保存、另存为、导出图片、退出。</li>
  <li><b>Edit</b>：复制、粘贴、删除、清空画布。</li>
  <li><b>Tools</b>：选择、点、直线、折线、圆、椭圆、矩形、多边形工具。</li>
  <li><b>Tutorial</b>：查看本手册，并切换界面语言。</li>
  <li><b>中央画布</b>：实际绘制、选择、移动图形的区域。</li>
  <li><b>属性面板</b>：编辑选中图形的几何参数、描边、线宽、线型、填充。</li>
  <li><b>状态栏</b>：显示当前工具和复制粘贴等操作提示。</li>
</ul>

<h2>2. 绘图工具用法</h2>
<ul>
  <li><b>选择</b>：点击图形选中；拖动画布中的选中图形可移动。</li>
  <li><b>点</b>：单击画布一次立即生成点。</li>
  <li><b>直线</b>：按下确定起点，拖动预览，松开确定终点。</li>
  <li><b>矩形</b>：按下确定角点，拖动预览，松开生成矩形。</li>
  <li><b>圆</b>：按下确定起点，拖动时自动生成正方形外接框，松开生成圆。</li>
  <li><b>椭圆</b>：按下确定外接框起点，拖动预览，松开生成椭圆。</li>
  <li><b>折线 Polyline</b>：连续单击添加顶点；双击或按 Enter 结束。</li>
  <li><b>多边形 Polygon</b>：连续单击添加顶点；双击或按 Enter 闭合并结束。</li>
</ul>

<h2>3. Polyline 与 Polygon 详细说明</h2>
<ul>
  <li><b>Polyline</b> 适合表示开口折线，不支持填充。</li>
  <li><b>Polygon</b> 适合表示封闭区域，结束时会自动作为闭合图形保存，并支持填充颜色。</li>
  <li>绘制过程中，鼠标移动会显示预览边。</li>
  <li>如果当前点数不足，结束操作不会生成图形：折线至少 2 个点，多边形至少 3 个点。</li>
  <li>按 <b>Esc</b> 可以取消当前尚未完成的折线或多边形绘制。</li>
</ul>

<h2>4. 选择与编辑</h2>
<ul>
  <li>切回<b>选择</b>工具后，点击图形即可选中。</li>
  <li>选中后可直接拖动移动图形。</li>
  <li>属性面板会显示当前图形的主要几何参数。</li>
  <li>修改参数后，画布会立即刷新。</li>
  <li>封闭图形可开启填充并设置填充颜色。</li>
  <li>当前版本中，折线和多边形支持整体移动，不支持逐顶点编辑。</li>
</ul>

<h2>5. 文件与图片</h2>
<ul>
  <li><b>Open</b>：打开已保存的 <code>.vgjson</code> 或 <code>.json</code> 文件。</li>
  <li><b>Save</b>：保存到当前文件。</li>
  <li><b>Save As</b>：另存为新文件。</li>
  <li><b>Export Image</b>：导出当前画布为 PNG 图片。</li>
  <li><b>Clear Canvas</b>：清空当前画布中的全部图形。</li>
</ul>

<h2>6. 复制、粘贴、删除</h2>
<ul>
  <li>先选中图形，再执行<b>复制</b>或按 <b>Ctrl+C</b>。</li>
  <li>执行<b>粘贴</b>或按 <b>Ctrl+V</b> 后，会生成一份带偏移的新图形。</li>
  <li>按 <b>Delete</b> 或 <b>Backspace</b> 可删除当前选中图形。</li>
</ul>

<h2>7. 常用快捷键</h2>
<ul>
  <li><b>Ctrl+O</b>：打开</li>
  <li><b>Ctrl+S</b>：保存</li>
  <li><b>Ctrl+Shift+S</b>：另存为</li>
  <li><b>Ctrl+C</b>：复制</li>
  <li><b>Ctrl+V</b>：粘贴</li>
  <li><b>Delete / Backspace</b>：删除</li>
  <li><b>Enter</b>：结束折线或多边形绘制</li>
  <li><b>Esc</b>：取消当前绘制</li>
</ul>

<h2>8. 语言切换</h2>
<ul>
  <li>打开 <b>Tutorial</b> 菜单。</li>
  <li>在 <b>Language</b> 子菜单中选择 <b>English</b> 或 <b>简体中文</b>。</li>
  <li>切换后，菜单、工具栏、属性面板和提示文本会立即更新。</li>
</ul>

<h2>9. 当前实现边界</h2>
<ul>
  <li>不支持贝塞尔曲线、复杂布尔运算、图层系统和 SVG 深度导入。</li>
  <li>折线和多边形当前不支持顶点级编辑。</li>
  <li>图片填充和高级样式仍属于后续增强项。</li>
</ul>
)");
    }

    return QStringLiteral(R"(
<h1>Vector Editor Tutorial</h1>
<p>This manual explains the current UI, drawing workflow, editing operations, file handling, and the detailed usage of polyline and polygon tools.</p>

<h2>1. Interface Layout</h2>
<ul>
  <li><b>File</b>: open, save, save as, export image, exit.</li>
  <li><b>Edit</b>: copy, paste, delete, clear canvas.</li>
  <li><b>Tools</b>: select, point, line, polyline, circle, ellipse, rectangle, polygon.</li>
  <li><b>Tutorial</b>: open this manual and switch UI language.</li>
  <li><b>Canvas</b>: the central drawing and editing area.</li>
  <li><b>Property panel</b>: edit geometry, stroke, line width, line style, and fill.</li>
  <li><b>Status bar</b>: shows tool status and action feedback.</li>
</ul>

<h2>2. Drawing Tools</h2>
<ul>
  <li><b>Select</b>: click to select a shape, drag to move it.</li>
  <li><b>Point</b>: single-click once to create a point.</li>
  <li><b>Line</b>: press to set the start point, drag to preview, release to finish.</li>
  <li><b>Rectangle</b>: press to set a corner, drag to preview, release to create.</li>
  <li><b>Circle</b>: drag from a start point and the editor keeps a square bounding box.</li>
  <li><b>Ellipse</b>: drag to define the ellipse bounding box.</li>
  <li><b>Polyline</b>: click repeatedly to add vertices, then double-click or press Enter to finish.</li>
  <li><b>Polygon</b>: click repeatedly to add vertices, then double-click or press Enter to close and finish.</li>
</ul>

<h2>3. Polyline and Polygon</h2>
<ul>
  <li><b>Polyline</b> is an open path and does not use fill.</li>
  <li><b>Polygon</b> is stored as a closed shape and supports fill color.</li>
  <li>While drawing, moving the mouse shows a live preview edge.</li>
  <li>A polyline needs at least 2 points; a polygon needs at least 3 points.</li>
  <li>Press <b>Esc</b> to cancel an unfinished polyline or polygon.</li>
</ul>

<h2>4. Selection and Editing</h2>
<ul>
  <li>Switch back to <b>Select</b> to pick a shape.</li>
  <li>Drag a selected shape to move it.</li>
  <li>The property panel updates to the selected shape immediately.</li>
  <li>Changing geometry values updates the canvas in real time.</li>
  <li>Closed shapes can enable fill and choose a fill color.</li>
  <li>Polyline and polygon currently support whole-shape movement, not per-vertex editing.</li>
</ul>

<h2>5. Files and Export</h2>
<ul>
  <li><b>Open</b>: load a saved <code>.vgjson</code> or <code>.json</code> file.</li>
  <li><b>Save</b>: save to the current file.</li>
  <li><b>Save As</b>: save to a new file path.</li>
  <li><b>Export Image</b>: export the canvas to PNG.</li>
  <li><b>Clear Canvas</b>: remove every shape from the current canvas.</li>
</ul>

<h2>6. Copy, Paste, Delete</h2>
<ul>
  <li>Select a shape first, then use <b>Copy</b> or <b>Ctrl+C</b>.</li>
  <li>Use <b>Paste</b> or <b>Ctrl+V</b> to create an offset copy.</li>
  <li>Use <b>Delete</b> or <b>Backspace</b> to remove the selected shape.</li>
</ul>

<h2>7. Shortcuts</h2>
<ul>
  <li><b>Ctrl+O</b>: open</li>
  <li><b>Ctrl+S</b>: save</li>
  <li><b>Ctrl+Shift+S</b>: save as</li>
  <li><b>Ctrl+C</b>: copy</li>
  <li><b>Ctrl+V</b>: paste</li>
  <li><b>Delete / Backspace</b>: delete selected item</li>
  <li><b>Enter</b>: finish polyline or polygon</li>
  <li><b>Esc</b>: cancel current drawing</li>
</ul>

<h2>8. Language Switching</h2>
<ul>
  <li>Open the <b>Tutorial</b> menu.</li>
  <li>Use <b>Language</b> to switch between <b>English</b> and <b>Simplified Chinese</b>.</li>
  <li>Menus, toolbar labels, the property panel, and prompts update immediately.</li>
</ul>

<h2>9. Current Boundaries</h2>
<ul>
  <li>No bezier editing, boolean operations, layer system, or deep SVG import.</li>
  <li>Polyline and polygon do not yet support vertex-level editing.</li>
  <li>Image fill and advanced styling remain future enhancements.</li>
</ul>
)");
}
