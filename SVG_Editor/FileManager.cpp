// =====================================================================
// FileManager.cpp
// ---------------------------------------------------------------------
// @brief FileManager.h 中声明的静态方法的实现
// @details 写入采用 QJsonDocument::Indented 便于人读/调试；
//          读取对缺省字段做兜底（canvas 默认 1200×800，shapes 缺失视为空），
//          单个 shape 反序列化失败会被静默跳过（见 FIXME）。
// @layer   core
// =====================================================================

#include "FileManager.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>

bool FileManager::saveToFile(const QString& filePath, const DocumentData& document, QString* errorMessage) {
    // 把所有 ShapeData 预序列化为 JSON 数组
    QJsonArray shapeArray;
    for (const ShapeData& shape : document.shapes) {
        shapeArray.append(shapeDataToJson(shape));
    }

    // 文档根结构：version + canvas + shapes
    const QJsonObject root = {
        {"version", 1},
        {"canvas",
         QJsonObject{
             {"width", document.canvasSize.width()},
             {"height", document.canvasSize.height()},
         }},
        {"shapes", shapeArray},
    };

    // 打开文件（WriteOnly + Truncate 模式）；若路径不存在则创建
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        if (errorMessage != nullptr) {
            *errorMessage = file.errorString();
        }
        return false;
    }

    // 写入缩进格式 JSON（QJsonDocument::Indented），便于人读
    file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    return true;
}

std::optional<DocumentData> FileManager::loadFromFile(const QString& filePath, QString* errorMessage) {
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        if (errorMessage != nullptr) {
            *errorMessage = file.errorString();
        }
        return std::nullopt;
    }

    // 解析整份 JSON；非 object 根视为格式错误
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll());
    if (!document.isObject()) {
        if (errorMessage != nullptr) {
            *errorMessage = "Invalid JSON document.";
        }
        return std::nullopt;
    }

    const QJsonObject root = document.object();
    const QJsonObject canvas = root.value("canvas").toObject();

    DocumentData data;
    // 画布尺寸缺省回退到 1200×800（与 MainWindow 的默认值一致）
    data.canvasSize = QSizeF(canvas.value("width").toDouble(1200.0), canvas.value("height").toDouble(800.0));

    const QJsonArray shapes = root.value("shapes").toArray();
    for (const auto& value : shapes) {
        const std::optional<ShapeData> shape = shapeDataFromJson(value.toObject());
        if (shape.has_value()) {
            data.shapes.append(*shape);
        }
        // FIXME: 失败的单条 shape 当前被静默跳过；
        //        若文件被外部工具破坏，用户感知不到具体哪一条出错。
        //        改进方向：收集 (index, error) 列表，通过 errorMessage 报告。
    }

    return data;
}
