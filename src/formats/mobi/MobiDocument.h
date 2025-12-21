/**
 * QuantilyxDoc - Professional Document Editor
 * Copyright (C) 2025 R² Innovative Software
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */
#ifndef QUANTILYX_MOBIDOCUMENT_H
#define QUANTILYX_MOBIDOCUMENT_H

#include "../../core/Document.h"
#include <memory>
#include <QList>
#include <QMap>

namespace QuantilyxDoc {

class MobiPage; // Forward declaration

/**
 * @brief MOBI document implementation.
 * 
 * Handles loading and parsing of MOBI files (Amazon Kindle format).
 * Often requires conversion to HTML for display.
 */
class MobiDocument : public Document
{
    Q_OBJECT

public:
    explicit MobiDocument(QObject* parent = nullptr);
    ~MobiDocument() override;

    // --- Document Interface Implementation ---
    bool load(const QString& filePath, const QString& password = QString()) override;
    bool save(const QString& filePath = QString()) override;
    DocumentType type() const override;
    int pageCount() const override; // Page count might be estimated or based on chapters
    Page* page(int index) const override;
    bool isLocked() const override; // MOBI can be DRM-protected
    bool isEncrypted() const override;
    QString formatVersion() const override; // e.g., "MOBI 6", "KFX"
    bool supportsFeature(const QString& feature) const override;

    // --- MOBI-Specific Metadata ---
    QString mobiTitle() const;
    QString mobiAuthor() const;
    QList<QString> mobiSubjects() const;
    bool hasDrm() const;
    QStringList embeddedFonts() const;

signals:
    void mobiLoaded();

private:
    class Private;
    std::unique_ptr<Private> d;

    void createPages();
};

} // namespace QuantilyxDoc

#endif // QUANTILYX_MOBIDOCUMENT_H