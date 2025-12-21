/**
 * QuantilyxDoc - Professional Document Editor
 * Copyright (C) 2025 R² Innovative Software
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */
#ifndef QUANTILYX_EPSDOCUMENT_H
#define QUANTILYX_EPSDOCUMENT_H

#include "PsDocument.h" // Inherits from PsDocument

namespace QuantilyxDoc {

/**
 * @brief Encapsulated PostScript document implementation.
 * 
 * Specializes PsDocument for Encapsulated PostScript (.eps) files.
 * Adds specific handling for EPS conventions like preview images.
 */
class EpsDocument : public PsDocument
{
    Q_OBJECT

public:
    explicit EpsDocument(QObject* parent = nullptr);
    ~EpsDocument() override;

    // --- Document Interface Implementation ---
    // Override type() to return EPS
    DocumentType type() const override;

    // --- EPS-Specific Properties ---
    bool hasPreviewImage() const;
    QImage previewImage() const; // Extracted preview from EPS file

signals:
    void epsLoaded();

private:
    class Private;
    std::unique_ptr<Private> d;

    // Helper to check for and extract EPS preview (e.g., %%BeginPreview section)
    void parseEpsSpecificElements();
};

} // namespace QuantilyxDoc

#endif // QUANTILYX_EPSDOCUMENT_H