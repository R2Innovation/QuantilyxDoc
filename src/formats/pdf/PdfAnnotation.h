/**
 * QuantilyxDoc - Professional Document Editor
 * Copyright (C) 2025 R² Innovative Software
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */
#ifndef QUANTILYX_PDFANNOTATION_H
#define QUANTILYX_PDFANNOTATION_H

#include "../../annotations/Annotation.h" // Assuming a base Annotation class exists or will be created in src/annotations/
#include <poppler-qt5.h>
#include <memory>
#include <QPointF>
#include <QRectF>
#include <QColor>
#include <QDateTime>

namespace QuantilyxDoc {

class PdfDocument;

/**
 * @brief PDF annotation implementation using Poppler.
 * 
 * Concrete implementation of the Annotation interface specifically for PDF annotations.
 * Wraps a Poppler::Annotation object and provides QuantilyxDoc-specific properties and methods.
 */
class PdfAnnotation : public Annotation // Assuming Annotation inherits from QObject
{
    Q_OBJECT

public:
    /**
     * @brief Type of PDF annotation.
     * Mirrors Poppler::Annotation::SubType or defines a compatible set.
     */
    enum class Type {
        Unknown,
        Text,           // Sticky note
        Link,
        FreeText,       // Text box annotation
        Line,
        Square,         // Highlight box
        Circle,
        Polygon,
        PolyLine,
        Highlight,      // Text highlight
        Underline,
        Squiggly,       // Squiggly underline
        StrikeOut,
        Stamp,          // Stamp annotation (e.g., "Approved")
        Caret,          // Insertion caret
        Ink,            // Freehand drawing
        Popup,          // Popup window for another annotation
        FileAttachment, // Attached file
        Sound,          // Sound clip
        Movie,          // Movie clip
        Widget,         // Form widget (not a visual annotation per se)
        Screen,         // Screen for multimedia
        PrinterMark,    // Printer's mark
        TrapNet,        // Trap network color
        Watermark,      // Watermark
        // 3D,          // 3D annotation (if supported by Poppler version)
    };

    /**
     * @brief Constructor wrapping an existing Poppler annotation.
     * @param popplerAnnot The underlying Poppler annotation object.
     * @param document The parent PdfDocument this annotation belongs to.
     * @param pageIndex The page index this annotation is on.
     * @param parent Parent object.
     */
    explicit PdfAnnotation(Poppler::Annotation* popplerAnnot, PdfDocument* document, int pageIndex, QObject* parent = nullptr);

    /**
     * @brief Constructor for creating a new annotation (e.g., by UI tools).
     * @param type The type of annotation to create.
     * @param bounds The initial bounds of the annotation.
     * @param document The parent PdfDocument this annotation belongs to.
     * @param pageIndex The page index this annotation is on.
     * @param parent Parent object.
     */
    explicit PdfAnnotation(Type type, const QRectF& bounds, PdfDocument* document, int pageIndex, QObject* parent = nullptr);

    /**
     * @brief Destructor.
     */
    ~PdfAnnotation() override;

    // --- Annotation Interface Implementation (if such an interface exists in src/annotations/) ---
    // If Annotation is just a base class without pure virtuals, these might not be overrides.
    // If it's an interface, define the methods here.
    // For now, let's assume it has common properties like bounds, content, etc.
    /**
     * @brief Get the type of this annotation.
     * @return The annotation type.
     */
    Type type() const;

    /**
     * @brief Get the bounds of this annotation in PDF coordinates.
     * @return Rectangle in PDF coordinate system.
     */
    QRectF bounds() const override; // Assuming base Annotation class has this

    /**
     * @brief Get the author of this annotation.
     * @return Author name.
     */
    QString author() const override;

    /**
     * @brief Get the contents/note text of this annotation.
     * @return Contents string.
     */
    QString contents() const override;

    /**
     * @brief Get the modification date of this annotation.
     * @return Modification date/time.
     */
    QDateTime modificationDate() const override;

    /**
     * @brief Get the color of this annotation.
     * @return Color.
     */
    QColor color() const override;

    /**
     * @brief Set the contents/note text of this annotation.
     * @param contents New contents string.
     */
    void setContents(const QString& contents) override;

    /**
     * @brief Set the color of this annotation.
     * @param color New color.
     */
    void setColor(const QColor& color) override;

    // --- PDF-Specific Properties ---
    /**
     * @brief Get the underlying Poppler annotation object.
     * @return Poppler annotation object or nullptr.
     */
    Poppler::Annotation* popplerAnnotation() const;

    /**
     * @brief Get the page index this annotation is associated with.
     * @return Page index (0-based).
     */
    int pageIndex() const;

    /**
     * @brief Get the name of this annotation (optional PDF field).
     * @return Annotation name.
     */
    QString name() const;

    /**
     * @brief Get the subject of this annotation (optional PDF field).
     * @return Subject string.
     */
    QString subject() const;

    /**
     * @brief Get the opacity of this annotation.
     * @return Opacity value (0.0 - 1.0).
     */
    qreal opacity() const;

    /**
     * @brief Check if this annotation is hidden.
     * @return True if hidden.
     */
    bool isHidden() const;

    /**
     * @brief Check if this annotation is read-only.
     * @return True if read-only.
     */
    bool isReadOnly() const;

    /**
     * @brief Check if this annotation is locked.
     * @return True if locked.
     */
    bool isLocked() const;

    /**
     * @brief Get the border style information for this annotation.
     * @return Border style struct/object (if applicable).
     */
    QVariant borderStyle() const; // Could be a custom struct

    /**
     * @brief Get the appearance string for this annotation (for stamps, etc.).
     * @return Appearance name.
     */
    QString appearance() const;

    /**
     * @brief Get the icon name for this annotation (for text annotations).
     * @return Icon name string.
     */
    QString iconName() const;

    /**
     * @brief Get the text annotation state (e.g., "Accepted", "Rejected").
     * @return State string.
     */
    QString state() const;

    /**
     * @brief Get the text annotation state model (e.g., "Review").
     * @return State model string.
     */
    QString stateModel() const;

    /**
     * @brief Get the ink paths for ink annotations.
     * @return List of lists of points representing paths.
     */
    QList<QList<QPointF>> inkPaths() const;

    /**
     * @brief Get the line coordinates for line annotations.
     * @return Pair of start/end points.
     */
    QPair<QPointF, QPointF> lineCoordinates() const;

    // --- Modification ---
    /**
     * @brief Update the annotation's internal Poppler object based on local changes.
     * This is needed before saving the document if the annotation was modified.
     */
    void syncToPopplerObject();

signals:
    /**
     * @brief Emitted when the annotation's properties change.
     */
    void propertiesChanged();

private:
    class Private;
    std::unique_ptr<Private> d;
};

} // namespace QuantilyxDoc

#endif // QUANTILYX_PDFANNOTATION_H