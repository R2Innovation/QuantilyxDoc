/**
 * QuantilyxDoc - Professional Document Editor
 * Copyright (C) 2025 R² Innovative Software
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */
#ifndef QUANTILYX_MDDOCUMENT_H
#define QUANTILYX_MDDOCUMENT_H

#include "../../core/Document.h"
#include <memory>
#include <QList>

namespace QuantilyxDoc {

class MdPage; // Forward declaration

/**
 * @brief Markdown document implementation.
 * 
 * Handles loading and parsing of Markdown files (.md, .markdown).
 * Rendering to HTML or other formats is handled separately.
 */
class MdDocument : public Document
{
    Q_OBJECT

public:
    explicit MdDocument(QObject* parent = nullptr);
    ~MdDocument() override;

    // --- Document Interface Implementation ---
    bool load(const QString& filePath, const QString& password = QString()) override;
    bool save(const QString& filePath = QString()) override;
    DocumentType type() const override;
    int pageCount() const override; // Single page document
    Page* page(int index) const override;
    bool isLocked() const override;
    bool isEncrypted() const override;
    QString formatVersion() const override; // Could be "CommonMark", "GitHub Flavored Markdown", etc.
    bool supportsFeature(const QString& feature) const override;

    // --- Markdown-Specific Properties ---
    QString markdownContent() const;
    QString renderedHtml() const; // Cached HTML rendering

signals:
    void mdLoaded();

private:
    class Private;
    std::unique_ptr<Private> d;

    void createPages();
};

} // namespace QuantilyxDoc

#endif // QUANTILYX_MDDOCUMENT_H