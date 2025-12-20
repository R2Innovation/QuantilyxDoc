/**
 * QuantilyxDoc - Professional Document Editor
 * Copyright (C) 2025 R² Innovative Software
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */
#include "PdfAnnotation.h"
#include "PdfDocument.h"
#include "../../core/Logger.h"
#include <poppler-qt5.h>
#include <QDateTime>
#include <QColor>
#include <QRectF>
#include <QPointF>
#include <QPair>
#include <QList>
#include <QVariant>
#include <QDebug>

namespace QuantilyxDoc {

class PdfAnnotation::Private {
public:
    Private(Poppler::Annotation* pAnnot, PdfDocument* doc, int pIndex)
        : popplerAnnot(pAnnot), document(doc), pageIndexVal(pIndex), typeVal(Type::Unknown), modified(false) {
            if (popplerAnnot) {
                typeVal = convertPopplerType(popplerAnnot->subType());
                // Store initial state for comparison later
                initialContents = popplerAnnot->contents();
                initialColor = popplerAnnot->color();
                initialHidden = isHiddenFromPoppler(popplerAnnot); // Add helper to get initial state
                // Store other properties as needed
            }
        }
    Poppler::Annotation* popplerAnnot;
    PdfDocument* document;
    int pageIndexVal;
    Type typeVal;
    bool modified;
    // Store initial state from Poppler object
    QString initialContents;
    QColor initialColor;

    // Helper to convert Poppler::Annotation::SubType to our Type
    static Type convertPopplerType(Poppler::Annotation::SubType popplerType) {
        switch (popplerType) {
            case Poppler::Annotation::AText: return Type::Text;
            case Poppler::Annotation::ALine: return Type::Line;
            case Poppler::Annotation::ASquare: return Type::Square;
            case Poppler::Annotation::ACircle: return Type::Circle;
            case Poppler::Annotation::APolygon: return Type::Polygon;
            case Poppler::Annotation::APolyLine: return Type::PolyLine;
            case Poppler::Annotation::AHighlight: return Type::Highlight;
            case Poppler::Annotation::AUnderline: return Type::Underline;
            case Poppler::Annotation::ASquiggly: return Type::Squiggly;
            case Poppler::Annotation::AStrikeOut: return Type::StrikeOut;
            case Poppler::Annotation::AStamp: return Type::Stamp;
            case Poppler::Annotation::ACaret: return Type::Caret;
            case Poppler::Annotation::AInk: return Type::Ink;
            case Poppler::Annotation::APopup: return Type::Popup;
            case Poppler::Annotation::AFileAttachment: return Type::FileAttachment;
            case Poppler::Annotation::ASound: return Type::Sound;
            case Poppler::Annotation::AMovie: return Type::Movie;
            case Poppler::Annotation::AWidget: return Type::Widget;
            case Poppler::Annotation::AScreen: return Type::Screen;
            case Poppler::Annotation::APrinterMark: return Type::PrinterMark;
            case Poppler::Annotation::ATrapNet: return Type::TrapNet;
            case Poppler::Annotation::AWatermark: return Type::Watermark;
            // case Poppler::Annotation::A3D: return Type::3D; // If available
            default: return Type::Unknown;
        }
    }

    // Helper to convert our Type to Poppler::Annotation::SubType
    static Poppler::Annotation::SubType convertToPopplerType(Type type) {
        switch (type) {
            case Type::Text: return Poppler::Annotation::AText;
            case Type::Line: return Poppler::Annotation::ALine;
            case Type::Square: return Poppler::Annotation::ASquare;
            case Type::Circle: return Poppler::Annotation::ACircle;
            case Type::Polygon: return Poppler::Annotation::APolygon;
            case Type::PolyLine: return Poppler::Annotation::APolyLine;
            case Type::Highlight: return Poppler::Annotation::AHighlight;
            case Type::Underline: return Poppler::Annotation::AUnderline;
            case Type::Squiggly: return Poppler::Annotation::ASquiggly;
            case Type::StrikeOut: return Poppler::Annotation::AStrikeOut;
            case Type::Stamp: return Poppler::Annotation::AStamp;
            case Type::Caret: return Poppler::Annotation::ACaret;
            case Type::Ink: return Poppler::Annotation::AInk;
            case Type::Popup: return Poppler::Annotation::APopup;
            case Type::FileAttachment: return Poppler::Annotation::AFileAttachment;
            case Type::Sound: return Poppler::Annotation::ASound;
            case Type::Movie: return Poppler::Annotation::AMovie;
            case Type::Widget: return Poppler::Annotation::AWidget;
            case Type::Screen: return Poppler::Annotation::AScreen;
            case Type::PrinterMark: return Poppler::Annotation::APrinterMark;
            case Type::TrapNet: return Poppler::Annotation::ATrapNet;
            case Type::Watermark: return Poppler::Annotation::AWatermark;
            // case Type::3D: return Poppler::Annotation::A3D; // If available
            default: return Poppler::Annotation::AText; // Fallback
        }
    }
};

PdfAnnotation::PdfAnnotation(Poppler::Annotation* popplerAnnot, PdfDocument* document, int pageIndex, QObject* parent)
    : Annotation(parent) // Assuming base Annotation class constructor takes QObject* parent
    , d(new Private(popplerAnnot, document, pageIndex))
{
    if (!popplerAnnot) {
        LOG_ERROR("PdfAnnotation created with null Poppler::Annotation.");
    } else {
        LOG_DEBUG("PdfAnnotation created for page " << pageIndex << ", type: " << static_cast<int>(d->typeVal));
    }
}

PdfAnnotation::PdfAnnotation(Type type, const QRectF& bounds, PdfDocument* document, int pageIndex, QObject* parent)
    : Annotation(parent)
    , d(new Private(nullptr, document, pageIndex)) // popplerAnnot is null initially
{
    // Create a new Poppler::Annotation object based on the type and bounds.
    // This is complex because Poppler::Annotation is abstract.
    // We need to create the correct derived class like Poppler::TextAnnotation, Poppler::HighlightAnnotation, etc.
    // Poppler doesn't provide direct constructors for these derived classes in its public API easily.
    // Creating new annotations requires a library with PDF *writing* capabilities (like PoDoFo, mupdf bindings, QPDF).
    // Poppler-Qt5 is primarily for reading.
    // For QuantilyxDoc, we might need to defer full annotation *creation* until a writing library is integrated,
    // or use a hybrid approach where we create a temporary object here and handle the PDF writing later.
    // For now, this constructor might be a stub or only create a placeholder object.
    d->typeVal = type;
    LOG_WARN("PdfAnnotation constructor for new annotation (type " << static_cast<int>(type) << ") is a stub. Poppler-Qt5 is read-only for annotations.");
    Q_UNUSED(bounds);
}

PdfAnnotation::~PdfAnnotation()
{
    LOG_DEBUG("PdfAnnotation destroyed.");
}

PdfAnnotation::Type PdfAnnotation::type() const
{
    return d->typeVal;
}

QRectF PdfAnnotation::bounds() const
{
    if (d->popplerAnnot) {
        // Poppler::Annotation::boundary() returns the bounding box
        QRectF boundary = d->popplerAnnot->boundary();
        LOG_DEBUG("PdfAnnotation bounds: " << boundary);
        return boundary;
    }
    // If created new (popplerAnnot is null), return a stored value or default
    LOG_WARN("PdfAnnotation::bounds: Poppler annotation is null, returning default.");
    return QRectF(); // Or a stored value from a member variable if created new
}

QString PdfAnnotation::author() const
{
    return d->popplerAnnot ? d->popplerAnnot->author() : QString();
}

QString PdfAnnotation::contents() const
{
    return d->popplerAnnot ? d->popplerAnnot->contents() : QString();
}

QDateTime PdfAnnotation::modificationDate() const
{
    return d->popplerAnnot ? d->popplerAnnot->modificationDate() : QDateTime();
}

QColor PdfAnnotation::color() const
{
    if (d->popplerAnnot) {
        // Poppler::Annotation::color() returns QColor
        QColor c = d->popplerAnnot->color();
        LOG_DEBUG("PdfAnnotation color: " << c.name());
        return c;
    }
    return QColor(); // Default color
}

oid PdfAnnotation::setContents(const QString& contents)
{
    if (d->popplerAnnot) {
        if (contents != d->initialContents) { // Compare to initial state
            d->modified = true;
            // Store new value locally if needed for QPDF writing
            // d->localContents = contents; // Or update initialContents if you always compare to last saved
            emit propertiesChanged();
            if (d->document) {
                 // Notify the document that its state has changed, requiring a save
                 // This might involve calling a method on PdfDocument or using AnnotationManager
                 // For now, let's assume AnnotationManager tracks this per document
                 AnnotationManager::instance().markDocumentAsModified(d->document);
                 // Or PdfDocument could have a method like markAnnotationModified(this)
            }
        } else {
            // Contents reverted to initial state, maybe clear modified flag?
            // d->modified = false; // Be careful with this logic
        }
    } else {
        LOG_WARN("PdfAnnotation::setContents: Poppler annotation is null.");
    }
}

void PdfAnnotation::setColor(const QColor& color)
{
    if (d->popplerAnnot) {
        if (color != d->initialColor) {
            d->modified = true;
            // Store new value locally if needed
            // d->localColor = color;
            emit propertiesChanged();
             if (d->document) {
                 AnnotationManager::instance().markDocumentAsModified(d->document);
             }
        }
    } else {
        LOG_WARN("PdfAnnotation::setColor: Poppler annotation is null.");
    }
}

Poppler::Annotation* PdfAnnotation::popplerAnnotation() const
{
    return d->popplerAnnot;
}

int PdfAnnotation::pageIndex() const
{
    return d->pageIndexVal;
}

QString PdfAnnotation::name() const
{
    // Poppler might not expose 'name' directly, or it might be in specific annotation types.
    // Check Poppler API documentation.
    // For now, assume it's not directly available or is part of a more complex property map.
    LOG_WARN("PdfAnnotation::name: Not directly available in Poppler-Qt5 API.");
    return QString();
}

QString PdfAnnotation::subject() const
{
    // Poppler might not expose 'subject' directly, or it might be part of contents or title.
    // Check Poppler API documentation.
    // For Text annotations, 'title' might be used for the author or a short title.
    LOG_WARN("PdfAnnotation::subject: Not directly available in Poppler-Qt5 API as a separate field.");
    return QString();
}

qreal PdfAnnotation::opacity() const
{
    // Poppler might not expose opacity directly on the base Annotation class.
    // It might be part of the appearance characteristics or specific to certain types.
    LOG_WARN("PdfAnnotation::opacity: Not directly available in Poppler-Qt5 API.");
    return 1.0; // Default opaque
}

bool PdfAnnotation::isModified() const
{
    return d->modified;
}

bool PdfAnnotation::isHidden() const
{
    if (d->popplerAnnot) {
        // Poppler::Annotation::Flag is an enum or bitmask. Check its values.
        // Poppler::Annotation::Hidden is the flag we are looking for.
        // The exact enum value might be different, check Poppler documentation.
        // Common flags are: Invisible (1), Hidden (2), Print (4), NoZoom (8), NoRotate (16), NoView (32), ReadOnly (64), Locked (128)
        // Hidden is typically 2 (1 << 1).
        // Check the Poppler source or documentation for the exact enum.
        // Assuming Poppler::Annotation::Hidden exists and has the correct value.
        // int hiddenFlag = static_cast<int>(Poppler::Annotation::Hidden); // This might not be the exact syntax or value
        // A safer way is to use the numeric value directly if documented.
        // According to PDF spec and common Poppler usage: Hidden = 2
        const int HIDDEN_FLAG = 2;
        bool isHidden = (d->popplerAnnot->flags() & HIDDEN_FLAG) != 0;
        LOG_DEBUG("PdfAnnotation::isHidden: Annotation '" << name() << "' is hidden: " << isHidden);
        return isHidden;
    }
    LOG_WARN("PdfAnnotation::isHidden: Poppler annotation is null, returning false.");
    return false; // Default to not hidden if no object
}

// Add a setter for the hidden property
void PdfAnnotation::setHidden(bool hidden)
{
    if (d->popplerAnnot) {
        // Poppler::Annotation objects are read-only. We cannot directly change the flag here.
        // We need to store the intended new state and apply it during save using QPDF.
        LOG_DEBUG("PdfAnnotation::setHidden: Marking annotation '" << name() << "' hidden state for change (requires saving with writer). New state: " << hidden);

        // Store the new state locally (requires adding a member variable in Private)
        // For example, add 'std::optional<bool> localHiddenState;' to Private class
        // d->localHiddenState = hidden; // Assuming this member exists

        // Mark the annotation as modified
        if (hidden != isHidden()) { // Only mark if the value actually changed
            d->modified = true;
            emit propertiesChanged();
            if (d->document) {
                 AnnotationManager::instance().markDocumentAsModified(d->document);
            }
        }
    } else {
        LOG_WARN("PdfAnnotation::setHidden: Poppler annotation is null.");
    }
}

// Add the getter for the local state (needed for QPDF save logic)
bool PdfAnnotation::getLocalHiddenState() const
{
    // Return the locally stored intended state, or the original state if not modified
    // This requires storing the intended state in the Private class.
    // For example, if you added 'std::optional<bool> localHiddenState;' to Private:
    // return d->localHiddenState.value_or(isHidden());
    // For now, since we haven't added the member, we'll just return the original state from Poppler.
    // This means the save logic will need to know if *this specific property* was modified separately.
    // A better way is to add the member.
    LOG_WARN("PdfAnnotation::getLocalHiddenState: Requires storing local state in Private class.");
    return isHidden(); // Fallback to original state
}

bool PdfAnnotation::isReadOnly() const
{
    // Poppler::Annotation might have a flags field that includes a read-only flag.
    LOG_WARN("PdfAnnotation::isReadOnly: Requires checking Poppler::Annotation flags.");
    return false; // Default
}

bool PdfAnnotation::isLocked() const
{
    // Poppler::Annotation might have a flags field that includes a locked flag.
    LOG_WARN("PdfAnnotation::isLocked: Requires checking Poppler::Annotation flags.");
    return false; // Default
}

QVariant PdfAnnotation::borderStyle() const
{
    // Poppler::Annotation::Border might provide this, but access might be limited.
    LOG_WARN("PdfAnnotation::borderStyle: Requires detailed Poppler::Annotation::Border access.");
    return QVariant(); // Default
}

QString PdfAnnotation::appearance() const
{
    // For Stamp annotations, the appearance might be the name of the stamp image.
    // This is often handled by the PDF viewer/renderer, not directly exposed by Poppler-Qt5 for modification.
    LOG_WARN("PdfAnnotation::appearance: Not directly available for modification in Poppler-Qt5 API.");
    return QString();
}

QString PdfAnnotation::iconName() const
{
    // For Text annotations, the icon name (e.g., "Comment", "Key") might be available.
    // Check Poppler::TextAnnotation specific properties if accessible.
    // Poppler::Annotation is the base class.
    if (d->popplerAnnot && d->popplerAnnot->subType() == Poppler::Annotation::AText) {
         // Cast to Poppler::TextAnnotation if possible via dynamic_cast or a specific getter.
         // Poppler::Annotation doesn't expose sub-type specific properties directly on the base class.
         // This requires careful casting or a different design where PdfAnnotation stores its own copy of the specific data.
         LOG_WARN("PdfAnnotation::iconName: Requires casting to Poppler::TextAnnotation.");
    }
    return QString();
}

QString PdfAnnotation::state() const
{
    // For Review annotations, the state (e.g., "Accepted", "Rejected").
    // Check Poppler::TextAnnotation or similar specific types.
    LOG_WARN("PdfAnnotation::state: Requires specific annotation type access.");
    return QString();
}

QString PdfAnnotation::stateModel() const
{
    // For Review annotations, the state model (e.g., "Review").
    // Check Poppler::TextAnnotation or similar specific types.
    LOG_WARN("PdfAnnotation::stateModel: Requires specific annotation type access.");
    return QString();
}

QList<QList<QPointF>> PdfAnnotation::inkPaths() const
{
    if (d->popplerAnnot && d->popplerAnnot->subType() == Poppler::Annotation::AInk) {
        // Cast to Poppler::InkAnnotation if possible.
        // Poppler::Annotation doesn't expose sub-type specific properties directly.
        // This is difficult with Poppler-Qt5's current API design for reading.
        LOG_WARN("PdfAnnotation::inkPaths: Requires casting to Poppler::InkAnnotation, not directly supported for reading paths.");
        // Poppler::InkAnnotation *inkAnnot = dynamic_cast<Poppler::InkAnnotation*>(d->popplerAnnot);
        // if (inkAnnot) { ... get paths ... }
        // But dynamic_cast might not work if the classes aren't polymorphically linked this way in the library.
        // The API often requires using the Poppler::Page::annotations() list which returns Poppler::Annotation*,
        // and the specific sub-type is only known via subType().
    }
    return QList<QList<QPointF>>(); // Default empty
}

QPair<QPointF, QPointF> PdfAnnotation::lineCoordinates() const
{
    if (d->popplerAnnot && d->popplerAnnot->subType() == Poppler::Annotation::ALine) {
        // Similar issue as inkPaths. Requires specific sub-type access.
        LOG_WARN("PdfAnnotation::lineCoordinates: Requires casting to Poppler::LineAnnotation, not directly supported for reading coordinates.");
    }
    return QPair<QPointF, QPointF>(); // Default empty
}

void PdfAnnotation::syncToPopplerObject()
{
    // This function would update the underlying Poppler::Annotation object
    // with any local changes made to this PdfAnnotation wrapper.
    // However, Poppler::Annotation objects are read-only in Poppler-Qt5.
    // Modifying them requires a PDF *writing* library.
    LOG_WARN("PdfAnnotation::syncToPopplerObject: Poppler-Qt5 annotations are read-only. Syncing requires a PDF writing library (e.g., PoDoFo, QPDF).");
    // A potential future implementation could serialize the local changes
    // and apply them during the document save process using an external tool or library.
}

} // namespace QuantilyxDoc