/**
 * QuantilyxDoc - Professional Document Editor
 * Copyright (C) 2025 R² Innovative Software
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */
#ifndef QUANTILYX_XPSDOCUMENT_H
#define QUANTILYX_XPSDOCUMENT_H

#include "../../core/Document.h"
#include <memory>
#include <QList>

namespace QuantilyxDoc {

class XpsPage; // Forward declaration

/**
 * @brief XPS document implementation.
 * 
 * Handles loading and parsing of XPS (Open XML Paper Specification) files.
 * XPS is Microsoft's fixed-document format, similar to PDF but based on XML and ZIP.
 */
class XpsDocument : public Document
{
    Q_OBJECT

public:
    explicit XpsDocument(QObject* parent = nullptr);
    ~XpsDocument() override;

    // --- Document Interface Implementation ---
    bool load(const QString& filePath, const QString& password = QString()) override;
    bool save(const QString& filePath = QString()) override;
    DocumentType type() const override;
    int pageCount() const override;
    Page* page(int index) const override;
    bool isLocked() const override;
    bool isEncrypted() const override;
    QString formatVersion() const override;
    bool supportsFeature(const QString& feature) const override;

    // --- XPS-Specific Metadata ---
    QString documentTitle() const;
    QString documentAuthor() const;
    QList<QString> documentKeywords() const;
    bool hasDigitalSignature() const;

signals:
    void xpsLoaded();

private:
    class Private;
    std::unique_ptr<Private> d;

    void createPages();
};

} // namespace QuantilyxDoc

#endif // QUANTILYX_XPSDOCUMENT_H