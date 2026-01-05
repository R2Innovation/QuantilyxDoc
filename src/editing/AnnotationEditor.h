/**
 * QuantilyxDoc - Professional Document Editor
 * Copyright (C) 2025 R² Innovative Software
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */
#ifndef QUANTILYX_ANNOTATIONEDITOR_H
#define QUANTILYX_ANNOTATIONEDITOR_H

#include <QObject>
#include <QList>
#include <QPointF>
#include <QRectF>
#include <QColor>
#include <QPen>
#include <QBrush>
#include <memory>
#include <QHash>

namespace QuantilyxDoc {

class Document; // Forward declaration
class Page;     // Forward declaration
class Annotation; // Forward declaration - assuming a base class exists or will be defined

/**
 * @brief Enum describing the type of annotation being edited.
 */
enum class AnnotationType {
    Text,           // Sticky note
    Highlight,      // Text highlight
    Underline,
    Squiggly,
    StrikeOut,
    Line,
    Square,         // Highlight box
    Circle,
    Polygon,
    PolyLine,
    Ink,            // Freehand drawing
    Stamp,          // Rubber stamp
    Link,           // Hyperlink annotation
    FileAttachment, // Attached file
    Sound,          // Sound clip
    Movie,          // Movie clip
    FreeText,       // Text box annotation
    Caret,          // Insertion caret
    Popup,          // Popup window for another annotation
    Widget,         // Form widget (not a visual annotation per se)
    Screen,         // Screen for multimedia
    PrinterMark,    // Printer's mark
    TrapNet,        // Trap network color
    Watermark,      // Watermark
    Unknown
};

/**
 * @brief Structure holding properties of an annotation being created/modified.
 */
struct AnnotationProperties {
    AnnotationType type = AnnotationType::Unknown;
    QString contents;             // Text content for text/sticky notes
    QColor color = Qt::yellow;    // Color of the annotation
    qreal opacity = 1.0;          // Opacity (0.0 - 1.0)
    QPen pen;                     // Pen for outlines (width, style, color)
    QBrush brush;                 // Brush for fills (color, style)
    QRectF bounds;                // Bounding rectangle on the page
    QPointF position;             // Position (for point-based annotations like stamps)
    QList<QPointF> inkPoints;     // Points for ink annotations
    QString linkDestination;      // Destination for link annotations
    bool isHidden = false;        // Visibility flag
    bool isPrintable = true;      // Print flag
    // Add more properties as needed per type
};

/**
 * @brief Manages the creation, modification, and deletion of document annotations.
 * 
 * Provides methods to add new annotations to a page, modify existing ones,
 * and delete them. Integrates with the core Annotation system and UI tools.
 * This class handles the *editing* logic, while the format-specific rendering/storage
 * is handled by the underlying Document/Page/Annotation classes (e.g., PdfAnnotation).
 */
class AnnotationEditor : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief Constructor.
     * @param parent Parent object.
     */
    explicit AnnotationEditor(QObject* parent = nullptr);

    /**
     * @brief Destructor.
     */
    ~AnnotationEditor() override;

    /**
     * @brief Get singleton instance.
     * @return Reference to the global AnnotationEditor instance.
     */
    static AnnotationEditor& instance();

    /**
     * @brief Add a new annotation to a specific page.
     * @param document The document containing the page.
     * @param pageIndex The index of the page.
     * @param properties The initial properties of the annotation.
     * @return Pointer to the newly created Annotation object, or nullptr on failure.
     *         The Annotation object's lifecycle might be managed by the Document/Page,
     *         or by this editor, depending on design. QPointer is recommended if managed elsewhere.
     */
    QPointer<Annotation> addAnnotation(Document* document, int pageIndex, const AnnotationProperties& properties);

    /**
     * @brief Modify an existing annotation.
     * @param annotation The annotation object to modify.
     * @param newProperties The new properties to apply.
     * @return True if modification was successful.
     */
    bool modifyAnnotation(Annotation* annotation, const AnnotationProperties& newProperties);

    /**
     * @brief Delete an existing annotation.
     * @param annotation The annotation object to delete.
     * @return True if deletion was successful.
     */
    bool deleteAnnotation(Annotation* annotation);

    /**
     * @brief Delete all annotations on a specific page.
     * @param document The document containing the page.
     * @param pageIndex The index of the page.
     * @return The number of annotations deleted.
     */
    int deleteAllAnnotationsOnPage(Document* document, int pageIndex);

    /**
     * @brief Delete all annotations in a specific document.
     * @param document The document to clear annotations from.
     * @return The number of annotations deleted.
     */
    int deleteAllAnnotationsInDocument(Document* document);

    /**
     * @brief Get all annotations on a specific page.
     * @param document The document containing the page.
     * @param pageIndex The index of the page.
     * @return List of Annotation pointers.
     */
    QList<QPointer<Annotation>> annotationsForPage(Document* document, int pageIndex) const;

    /**
     * @brief Get all annotations in a specific document.
     * @param document The document.
     * @return List of Annotation pointers.
     */
    QList<QPointer<Annotation>> annotationsForDocument(Document* document) const;

    /**
     * @brief Find annotations intersecting a specific rectangle on a page.
     * @param document The document containing the page.
     * @param pageIndex The index of the page.
     * @param rect The rectangle in page coordinates to search within.
     * @return List of Annotation pointers whose bounds intersect the rectangle.
     */
    QList<QPointer<Annotation>> findAnnotationsInRect(Document* document, int pageIndex, const QRectF& rect) const;

    /**
     * @brief Get the properties of an existing annotation.
     * @param annotation The annotation object.
     * @return AnnotationProperties struct containing the current properties.
     */
    AnnotationProperties getAnnotationProperties(Annotation* annotation) const;

    /**
     * @brief Set the active document for the editor.
     * This might be used by UI tools to know which document to operate on.
     * @param document The active document.
     */
    void setActiveDocument(Document* document);

    /**
     * @brief Get the currently active document for the editor.
     * @return Pointer to the active document, or nullptr.
     */
    Document* activeDocument() const;

    /**
     * @brief Check if the editor is currently modifying an annotation.
     * @return True if an edit operation is in progress.
     */
    bool isEditing() const;

    /**
     * @brief Start an editing operation on an annotation.
     * This could involve grabbing handles, showing properties, etc.
     * @param annotation The annotation to start editing.
     * @return True if the edit operation started successfully.
     */
    bool startEditing(Annotation* annotation);

    /**
     * @brief Finish the current editing operation.
     * Commits changes to the annotation.
     */
    void finishEditing();

    /**
     * @brief Cancel the current editing operation.
     * Discards any changes made during the edit.
     */
    void cancelEditing();

    /**
     * @brief Get the list of supported annotation types for creation/modification.
     * @return List of AnnotationType enums.
     */
    QList<AnnotationType> supportedAnnotationTypes() const;

signals:
    /**
     * @brief Emitted when an annotation is added.
     * @param annotation Pointer to the added annotation.
     * @param document Pointer to the document it was added to.
     * @param pageIndex Index of the page it was added to.
     */
    void annotationAdded(QuantilyxDoc::Annotation* annotation, QuantilyxDoc::Document* document, int pageIndex);

    /**
     * @brief Emitted when an annotation is modified.
     * @param annotation Pointer to the modified annotation.
     */
    void annotationModified(QuantilyxDoc::Annotation* annotation);

    /**
     * @brief Emitted when an annotation is deleted.
     * @param annotation Pointer to the deleted annotation.
     * @param document Pointer to the document it was on.
     * @param pageIndex Index of the page it was on.
     */
    void annotationDeleted(QuantilyxDoc::Annotation* annotation, QuantilyxDoc::Document* document, int pageIndex);

    /**
     * @brief Emitted when the list of annotations for a document changes.
     * @param document Pointer to the document.
     */
    void annotationsChanged(QuantilyxDoc::Document* document);

    /**
     * @brief Emitted when an annotation edit operation starts.
     * @param annotation Pointer to the annotation being edited.
     */
    void editStarted(QuantilyxDoc::Annotation* annotation);

    /**
     * @brief Emitted when an annotation edit operation finishes (committed).
     * @param annotation Pointer to the edited annotation.
     */
    void editFinished(QuantilyxDoc::Annotation* annotation);

    /**
     * @brief Emitted when an annotation edit operation is canceled.
     * @param annotation Pointer to the annotation whose edit was canceled.
     */
    void editCanceled(QuantilyxDoc::Annotation* annotation);

private:
    class Private;
    std::unique_ptr<Private> d;

    // Helper to validate annotation properties before adding/modifying
    bool validateProperties(const AnnotationProperties& props) const;

    // Helper to update the document's modification state after an annotation change
    void markDocumentAsModified(Document* document);
};

} // namespace QuantilyxDoc

#endif // QUANTILYX_ANNOTATIONEDITOR_H