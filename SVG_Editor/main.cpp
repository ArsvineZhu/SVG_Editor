// =====================================================================
// main.cpp
// ---------------------------------------------------------------------
// @brief   应用入口：构造 QApplication、主窗口、启动事件循环
// @details 流程：
//          1) QApplication：Qt GUI 程序的必备对象；它会解析命令行参数并初始化
//             平台插件（windows / xcb / cocoa ...）；
//          2) setApplicationName / setOrganizationName：影响 QSettings 持久化路径
//             （Windows 下注册表 / macOS plist / Linux ~/.config）；
//          3) MainWindow 内部已完成全部 UI 装配与语言加载；
//          4) app.exec() 启动事件循环，程序在窗口关闭时退出并返回退出码。
// @layer   entry
// =====================================================================

#include <QApplication>

#include <QSettings>

#include "MainWindow.h"

int main(int argc, char* argv[]) {
    // 1) Qt GUI 程序的根对象；持有事件循环、字体、调色板、剪贴板等
    QApplication app(argc, argv);
    // 2) 持久化标识：QSettings 会写到 CourseProject/SVG Editor/ 路径下
    QApplication::setApplicationName("SVG Editor");
    QApplication::setOrganizationName("CourseProject");

    // 3) 主窗口：构造时即完成 UI / 工具栏 / 菜单 / 信号的装配
    MainWindow window;
    window.show();

    // 4) 进入事件循环；exec() 在所有顶层窗口关闭后返回
    return app.exec();
}
