/**
 * QuantilyxDoc - Professional Document Editor
 * Copyright (C) 2025 R² Innovative Software
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */
#ifndef QUANTILYX_CHMDOCUMENT_H
#define QUANTILYX_CHMDOCUMENT_H

#include "../../core/Document.h"
#include <memory>
#include <QList>
#include <QMap>

namespace QuantilyxDoc {

class ChmPage; // Forward declaration

/**
 * @brief CHM (Compiled HTML Help) document implementation.
 * 
 * Handles loading and parsing of CHM files using a library like chmlib.
 */
class ChmDocument : public Document
{
    Q_OBJECT

public:
    explicit ChmDocument(QObject* parent = nullptr);
    ~ChmDocument() override;

    // --- Document Interface Implementation ---
    bool load(const QString& filePath, const QString& password = QString()) override;
    bool save(const QString& filePath = QString()) override;
    DocumentType type() const override;
    int pageCount() const override; // Page count = number of HTML files
    Page* page(int index) const override;
    bool isLocked() const override;
    bool isEncrypted() const override;
    QString formatVersion() const override;
    bool supportsFeature(const QString& feature) const override;

    // --- CHM-Specific Metadata ---
    QString helpTitle() const;
    QString helpDefaultTopic() const;
    QMap<QString, QString> helpFileList() const; // URL -> Description
    QString getHelpFileContent(const QString& urlPath) const;

signals:
    void chmLoaded();

private:
    class Private;
    std::unique_ptr<Private> d;

    void createPages();
};

} // namespace QuantilyxDoc

#endif // QUANTILYX_CHMDOCUMENT_H