/**
 * QuantilyxDoc - Professional Document Editor
 * Copyright (C) 2025 R² Innovative Software
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */
#ifndef QUANTILYX_FB2DOCUMENT_H
#define QUANTILYX_FB2DOCUMENT_H

#include "../../core/Document.h"
#include <memory>
#include <QList>
#include <QMap>

namespace QuantilyxDoc {

class Fb2Page; // Forward declaration

/**
 * @brief FictionBook (FB2) document implementation.
 * 
 * Handles loading and parsing of FB2 files (XML-based e-book format).
 */
class Fb2Document : public Document
{
    Q_OBJECT

public:
    explicit Fb2Document(QObject* parent = nullptr);
    ~Fb2Document() override;

    // --- Document Interface Implementation ---
    bool load(const QString& filePath, const QString& password = QString()) override;
    bool save(const QString& filePath = QString()) override;
    DocumentType type() const override;
    int pageCount() const override; // Page count = number of sections/chapters
    Page* page(int index) const override;
    bool isLocked() const override;
    bool isEncrypted() const override;
    QString formatVersion() const override;
    bool supportsFeature(const QString& feature) const override;

    // --- FB2-Specific Metadata ---
    QString bookTitle() const;
    QStringList bookAuthors() const;
    QString bookGenre() const;
    QString bookId() const;
    QStringList embeddedImageIds() const;
    QByteArray getEmbeddedImage(const QString& imageId) const;

signals:
    void fb2Loaded();

private:
    class Private;
    std::unique_ptr<Private> d;

    void createPages();
};

} // namespace QuantilyxDoc

#endif // QUANTILYX_FB2DOCUMENT_H