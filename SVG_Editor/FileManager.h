// =====================================================================
// FileManager.h
// ---------------------------------------------------------------------
// @brief   文档级 JSON 持久化（仅本项目自定义 `.vgjson` 格式，非标准 SVG）
// @details 提供整个画布（含画布尺寸与全部图形）的保存 / 加载能力。
//          文件格式（version 1）：
//          ```json
//          {
//            "version": 1,
//            "canvas":  { "width": <number>, "height": <number> },
//            "shapes":  [ <ShapeData JSON>, ... ]
//          }
//          ```
//          单个图形的 JSON 形态见 `ShapeData::shapeDataToJson`。
// @layer   core
// @warning 文件版本号目前固定为 1；后续若格式不兼容升级请改为 2 并在
//          `loadFromFile` 中按 version 分支处理。
// =====================================================================

#pragma once

#include <QList>
#include <QSizeF>
#include <QString>

#include <optional>

#include "ShapeData.h"

/// @brief 完整文档的内存表示：画布尺寸 + 全部图形（按 z 升序存储）。
struct DocumentData {
    QSizeF canvasSize;       ///< 画布逻辑尺寸（场景坐标系单位）
    QList<ShapeData> shapes; ///< 全部图形，列表顺序即 z 升序
};

/// @brief 文档级 JSON I/O。所有方法均为静态工具函数。
class FileManager {
  public:
    /// @brief 将文档序列化为缩进 JSON 并写入文件。
    /// @param filePath      目标文件路径（不存在则创建，已存在则覆盖）
    /// @param document      内存中的文档
    /// @param errorMessage  可选输出：失败时填充 Qt 错误描述；传 nullptr 则忽略
    /// @return true 表示写入成功；false 表示打开文件失败（详见 errorMessage）
    static bool saveToFile(const QString& filePath, const DocumentData& document, QString* errorMessage = nullptr);

    /// @brief 从 JSON 文件反序列化为文档。
    /// @param filePath      源文件路径
    /// @param errorMessage  可选输出：失败时填充错误描述
    /// @return 成功返回 DocumentData；打开失败、JSON 解析失败或图形缺失
    ///         type 字段时返回 std::nullopt
    static std::optional<DocumentData> loadFromFile(const QString& filePath, QString* errorMessage = nullptr);
};
