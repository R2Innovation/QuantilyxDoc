/**
 * QuantilyxDoc - Professional Document Editor
 * Copyright (C) 2025 R² Innovative Software
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */
#include "DwgDocument.h"
#include "DwgPage.h" // Assuming this will be created
#include "../../core/Logger.h"
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QProcess>
#include <QStandardPaths>
#include <QDebug>

namespace QuantilyxDoc {

class DwgDocument::Private {
public:
    Private() : isLoaded(false), pageCountVal(1), entityCountVal(0), is3dVal(false) {} // Assume single page/layouts for now
    ~Private() = default;

    bool isLoaded;
    int pageCountVal; // Could be number of layouts/views
    QString drawingName;
    QString units;
    QList<QString> layers;
    int entityCountVal;
    bool is3dVal;
    QList<std::unique_ptr<DwgPage>> pages;

    // Helper to find the ODA File Converter executable
    QString findOdaConverterExecutable() const {
        // Common names and locations for ODA File Converter
        QStringList possibleNames = {
    #ifdef Q_OS_WIN
            "ODAFileConverter.exe"
    #else
            "ODAFileConverter" // Or "oda_converter" depending on build/install
    #endif
        };

        for (const QString& name : possibleNames) {
            QString fullPath = QStandardPaths::findExecutable(name);
            if (!fullPath.isEmpty()) {
                LOG_DEBUG("DwgDocument: Found ODA converter at: " << fullPath);
                return fullPath;
            }
        }

        // If not found in PATH, check common installation directories (platform-specific)
        // This is complex and might require user configuration.
        // Example for Windows default install:
    #ifdef Q_OS_WIN
        QString defaultPath = "C:/Program Files/ODA/ODA File Converter/ODAFileConverter.exe";
        if (QFile::exists(defaultPath)) {
            LOG_DEBUG("DwgDocument: Found ODA converter at default Windows path: " << defaultPath);
            return defaultPath;
        }
    #endif

        LOG_ERROR("DwgDocument: ODA File Converter executable not found. Please install ODA File Converter.");
        return QString(); // Not found
    }
};

DwgDocument::DwgDocument(QObject* parent)
    : Document(parent)
    , d(new Private())
{
    LOG_INFO("DwgDocument created. Note: Requires ODA File Converter.");
}

DwgDocument::~DwgDocument()
{
    LOG_INFO("DwgDocument destroyed.");
}

bool DwgDocument::load(const QString& filePath, const QString& password)
{
    Q_UNUSED(password); // DWG passwords are handled by the ODA converter if at all
    d->isLoaded = false;
    d->pages.clear();

    // Find the ODA File Converter executable
    QString converterPath = findOdaConverterExecutable();
    if (converterPath.isEmpty()) {
        setLastError(tr("ODA File Converter not found. DWG support requires the ODA File Converter tool."));
        LOG_ERROR(lastError());
        return false;
    }

    // Use ODA File Converter to export to a more manageable format (e.g., DXF or SVG) temporarily
    // Example command: ODAFileConverter input.dwg output_directory/ DXF 0 ACAD2018 0
    QTemporaryDir tempDir;
    if (!tempDir.isValid()) {
        setLastError(tr("Failed to create temporary directory for DWG conversion."));
        LOG_ERROR(lastError());
        return false;
    }

    QString outputDir = tempDir.path();
    QString outputFilePath = outputDir + "/converted_output.dxf"; // Convert to DXF first

    QStringList args;
    args << filePath // Input file
         << outputDir // Output directory
         << "DXF" // Output format
         << "0" // Version (0 = latest)
         << "ACAD2018" // Specific version if needed
         << "0"; // Sheet set (0 = no)

    QProcess converterProcess;
    converterProcess.start(converterPath, args);
    bool finished = converterProcess.waitForFinished(-1); // Wait indefinitely

    if (!finished) {
        setLastError(tr("ODA File Converter process did not finish."));
        LOG_ERROR(lastError());
        return false;
    }

    if (converterProcess.exitCode() != 0) {
        QString errorOutput = converterProcess.readAllStandardError();
        setLastError(tr("ODA File Converter failed: %1").arg(errorOutput));
        LOG_ERROR(lastError());
        return false;
    }

    // At this point, a DXF file should exist in the temp directory.
    // We could then load this temporary DXF file using our existing DxfDocument class.
    // This is a complex workaround but avoids direct DWG library integration for now.

    // For this stub, let's assume the conversion worked and we have some basic info.
    // In a real implementation, you'd load the converted file (e.g., DxfDocument) and extract metadata/pages.
    d->drawingName = QFileInfo(filePath).baseName(); // Fallback name
    d->units = "Unitless"; // Fallback
    d->layers = QStringList() << "0"; // Default layer
    d->entityCountVal = 100; // Fallback count
    d->is3dVal = false; // Determine from conversion output if possible

    setFilePath(filePath);
    createPages();

    d->isLoaded = true;
    setState(Loaded);
    emit dwgLoaded();
    LOG_INFO("Successfully loaded DWG document (via ODA converter): " << filePath << " (Entities: " << d->entityCountVal << ", 3D: " << d->is3dVal << ")");
    return true;
}

bool DwgDocument::save(const QString& filePath)
{
    LOG_WARN("DwgDocument::save: Saving DWG is not implemented.");
    Q_UNUSED(filePath);
    setLastError(tr("Saving DWG documents is not supported."));
    return false;
}

Document::DocumentType DwgDocument::type() const
{
    return DocumentType::DWG;
}

int DwgDocument::pageCount() const
{
    // Could be number of layouts in the DWG file
    return d->pageCountVal;
}

Page* DwgDocument::page(int index) const
{
    if (index >= 0 && index < d->pages.size()) {
        // return d->pages[index].get(); // Placeholder
        LOG_DEBUG("DwgDocument::page: Planned page " << index);
        return nullptr; // Placeholder
    }
    return nullptr;
}

bool DwgDocument::isLocked() const
{
    // Determined by ODA converter or file properties
    return false; // Placeholder
}

bool DwgDocument::isEncrypted() const
{
    // Determined by ODA converter or file properties
    return false; // Placeholder
}

QString DwgDocument::formatVersion() const
{
    // Retrieved from file or ODA converter output
    return "AC1027"; // Placeholder
}

bool DwgDocument::supportsFeature(const QString& feature) const
{
    static const QSet<QString> supportedFeatures = {
        "VectorGraphics", "CADData", "Layers", "3DGraphics" // DWG specific features
    };
    return supportedFeatures.contains(feature);
}

QString DwgDocument::drawingName() const
{
    return d->drawingName;
}

QString DwgDocument::drawingUnits() const
{
    return d->units;
}

QList<QString> DwgDocument::layerNames() const
{
    return d->layers;
}

int DwgDocument::entityCount() const
{
    return d->entityCountVal;
}

bool DwgDocument::is3dDrawing() const
{
    return d->is3dVal;
}

bool DwgDocument::exportAsImage(const QString& outputPath, const QString& format, int resolution) const
{
    // This would use ODA File Converter to export directly to an image format.
    // Example command: ODAFileConverter input.dwg output_dir PNG 300 1 0
    QString converterPath = d->findOdaConverterExecutable();
    if (converterPath.isEmpty()) {
        LOG_ERROR("DwgDocument::exportAsImage: ODA File Converter not found.");
        return false;
    }

    QFileInfo outputInfo(outputPath);
    QString outputDir = outputInfo.absolutePath();
    QString outputFileBase = outputInfo.baseName();

    QStringList args;
    args << filePath()
         << outputDir
         << format.toUpper() // Output format (PNG, TIFF, etc.)
         << QString::number(resolution) // Resolution
         << "1" // Raster version (1 = latest)
         << "0"; // Sheet set

    QProcess exportProcess;
    exportProcess.start(converterPath, args);
    bool finished = exportProcess.waitForFinished(-1);

    if (!finished) {
        LOG_ERROR("DwgDocument::exportAsImage: ODA export process did not finish.");
        return false;
    }

    if (exportProcess.exitCode() != 0) {
        QString errorOutput = exportProcess.readAllStandardError();
        LOG_ERROR("DwgDocument::exportAsImage: ODA export failed: " << errorOutput);
        return false;
    }

    // Check if the expected output file was created
    QString expectedOutput = outputDir + "/" + outputFileBase + "." + format.toLower();
    if (QFile::exists(expectedOutput)) {
        LOG_INFO("DwgDocument::exportAsImage: Successfully exported to: " << expectedOutput);
        return true;
    } else {
        LOG_ERROR("DwgDocument::exportAsImage: Expected output file not found: " << expectedOutput);
        return false;
    }
}

void DwgDocument::createPages()
{
    d->pages.clear();
    d->pages.reserve(d->pageCountVal);
    for (int i = 0; i < d->pageCountVal; ++i) {
        // pages.append(std::make_unique<DwgPage>(this, i)); // Placeholder
        LOG_DEBUG("DwgDocument: Planned page " << i);
    }
    LOG_INFO("DwgDocument: Created " << d->pages.size() << " page objects.");
}

} // namespace QuantilyxDoc