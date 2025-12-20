/**
 * QuantilyxDoc - Professional Document Editor
 * Copyright (C) 2025 R² Innovative Software
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */
#include "PdfFormField.h"
#include "PdfDocument.h"
#include "../../core/Logger.h"
#include <poppler-qt5.h>
#include <QRectF>
#include <QVariant>
#include <QColor>
#include <QDebug>

namespace QuantilyxDoc {

class PdfFormField::Private {
public:
    Private(Poppler::FormField* pField, PdfDocument* doc)
        : popplerField(pField), document(doc), typeVal(Type::Unknown), buttonTypeVal(ButtonType::UnknownButton), choiceTypeVal(ChoiceType::UnknownChoice) {
            if (popplerField) {
                typeVal = convertPopplerType(popplerField->type());
                if (typeVal == Type::Button) {
                    // Determine specific button type
                    // Poppler::FormFieldButton might be the dynamic_cast target
                    Poppler::FormFieldButton* buttonField = dynamic_cast<Poppler::FormFieldButton*>(popplerField);
                    if (buttonField) {
                        switch (buttonField->buttonType()) {
                            case Poppler::FormFieldButton::PushButton: buttonTypeVal = ButtonType::Push; break;
                            case Poppler::FormFieldButton::CheckBox: buttonTypeVal = ButtonType::CheckBox; break;
                            case Poppler::FormFieldButton::Radio: buttonTypeVal = ButtonType::Radio; break;
                            default: buttonTypeVal = ButtonType::UnknownButton; break;
                        }
                    }
                } else if (typeVal == Type::Choice) {
                    // Determine specific choice type
                    Poppler::FormFieldChoice* choiceField = dynamic_cast<Poppler::FormFieldChoice*>(popplerField);
                    if (choiceField) {
                        switch (choiceField->choiceType()) {
                            case Poppler::FormFieldChoice::ComboBox: choiceTypeVal = ChoiceType::ComboBox; break;
                            case Poppler::FormFieldChoice::ListBox: choiceTypeVal = ChoiceType::ListBox; break;
                            default: choiceTypeVal = ChoiceType::UnknownChoice; break;
                        }
                    }
                }
            }
        }

    Poppler::FormField* popplerField;
    PdfDocument* document;
    Type typeVal;
    ButtonType buttonTypeVal;
    ChoiceType choiceTypeVal;
    bool modified; // Track if the field's value has been modified

    // Store initial state from Poppler object
    QString initialTextValue;
    bool initialCheckedState = false; // Or appropriate default
    int initialSelectedIndex = -1; // For choice fields
    // Add other initial state members as needed

    // Helper to convert Poppler::FormField::FormType to our Type
    static Type convertPopplerType(Poppler::FormField::FormType popplerType) {
        switch (popplerType) {
            case Poppler::FormField::FormButton: return Type::Button;
            case Poppler::FormField::FormText: return Type::Text;
            case Poppler::FormField::FormChoice: return Type::Choice;
            case Poppler::FormField::FormSignature: return Type::Signature;
            default: return Type::Unknown;
        }
    }

    // Helper to get page index from Poppler field (requires accessing page properties, often not directly on the field object itself)
    // This might require storing the page index when the field is created within PdfDocument/PdfPage.
    // Let's assume PdfDocument passes the page index or it's derivable.
    // For now, we'll store it during construction if possible or use a lookup.
    int getPageIndex() const {
        // This is tricky. Poppler::FormField doesn't directly store its page index.
        // It's usually associated with a Poppler::Page when retrieved via popplerDoc->formFields().
        // PdfDocument likely needs to determine and pass this during PdfFormField creation.
        // For this stub, assume it's stored in a member variable initialized at construction.
        // This requires changes in PdfDocument::Private::loadFormFields or wherever PdfFormField is created.
        LOG_WARN("PdfFormField::getPageIndex: Requires page index to be passed/stored during construction.");
        return -1; // Placeholder
    }

    // Store the page index determined externally (e.g., in PdfDocument)
    int pageIndexVal = -1;
};

PdfFormField::PdfFormField(Poppler::FormField* popplerField, PdfDocument* document, QObject* parent)
    : QObject(parent)
    , d(new Private(popplerField, document))
{
    if (!popplerField) {
        LOG_ERROR("PdfFormField created with null Poppler::FormField.");
    } else {
        LOG_DEBUG("PdfFormField created for name: " << popplerField->name() << ", type: " << static_cast<int>(d->typeVal));
        // Determine page index - this is the challenging part.
        // When PdfDocument loads fields, it knows the page. It should pass this index here.
        // d->pageIndexVal = ...; // Set by PdfDocument
    }
}

PdfFormField::~PdfFormField()
{
    LOG_DEBUG("PdfFormField '" << (d->popplerField ? d->popplerField->name() : "unnamed") << "' destroyed.");
}

PdfFormField::Type PdfFormField::type() const
{
    return d->typeVal;
}

PdfFormField::ButtonType PdfFormField::buttonType() const
{
    return d->buttonTypeVal;
}

PdfFormField::ChoiceType PdfFormField::choiceType() const
{
    return d->choiceTypeVal;
}

QString PdfFormField::name() const
{
    return d->popplerField ? d->popplerField->name() : QString();
}

QString PdfFormField::fullyQualifiedName() const
{
    // Poppler might not directly provide the fully qualified name (FQN).
    // FQN includes parent field names separated by dots (e.g., "Form1.FieldA").
    // This requires traversing the PDF's form field hierarchy, which Poppler-Qt5
    // might not expose easily in its FormField object.
    LOG_WARN("PdfFormField::fullyQualifiedName: Not directly available in Poppler-Qt5 API.");
    return name(); // Fallback to simple name
}

QString PdfFormField::id() const
{
    // PDF Form Fields might have an ID, but Poppler-Qt5 doesn't seem to expose it directly.
    // Name is often used as a unique identifier within the form.
    LOG_WARN("PdfFormField::id: Not directly available in Poppler-Qt5 API.");
    return QString(); // Or maybe return name() as a pseudo-ID?
}

int PdfFormField::pageIndex() const
{
    // As discussed in Private, getting the page index from the Poppler::FormField itself is difficult.
    // It must be determined when the field is loaded and stored.
    // This requires changes in PdfDocument's field loading logic to pass/store the index.
    return d->pageIndexVal; // Return the stored value
}

QRectF PdfFormField::bounds() const
{
    if (d->popplerField) {
        // Poppler::FormField::rect() returns the bounding box
        QRectF rect = d->popplerField->rect();
        LOG_DEBUG("PdfFormField bounds: " << rect);
        return rect;
    }
    return QRectF(); // Default empty rect
}

QVariant PdfFormField::value() const
{
    if (!d->popplerField) return QVariant();

    // The value type depends on the field type
    switch (d->typeVal) {
        case Type::Text: {
            Poppler::FormFieldText* textField = dynamic_cast<Poppler::FormFieldText*>(d->popplerField);
            if (textField) {
                return textField->text(); // Returns QString
            }
            break;
        }
        case Type::Button: {
            Poppler::FormFieldButton* buttonField = dynamic_cast<Poppler::FormFieldButton*>(d->popplerField);
            if (buttonField) {
                if (d->buttonTypeVal == ButtonType::CheckBox || d->buttonTypeVal == ButtonType::Radio) {
                    return buttonField->state(); // Returns bool for checked state
                } else if (d->buttonTypeVal == ButtonType::Push) {
                    // Push buttons don't have a persistent value like checked state
                    return QString(); // Or maybe an action identifier?
                }
            }
            break;
        }
        case Type::Choice: {
            Poppler::FormFieldChoice* choiceField = dynamic_cast<Poppler::FormFieldChoice*>(d->popplerField);
            if (choiceField) {
                if (d->choiceTypeVal == ChoiceType::ComboBox || d->choiceTypeVal == ChoiceType::ListBox) {
                    // Return selected indices as a list, or just the first selected one as an int?
                    // Poppler::FormFieldChoice::currentValue() might return a string or index?
                    // Let's get the selected indexes.
                    QList<int> indices = choiceField->currentValues(); // Hypothetical, check Poppler API
                    // Or Poppler::FormFieldChoice::value() might return the text of the selected item(s)
                    QString selectedText = choiceField->value(); // Check what this actually returns
                    if (!indices.isEmpty()) {
                        if (indices.size() == 1) {
                            return indices.first(); // Return first index if single selection
                        } else {
                            return indices; // Return list if multiple selection allowed (unlikely for basic combo/list)
                        }
                    } else {
                        // Fallback if currentValues() doesn't exist or is empty
                        // Try to find index from value() string against options()
                        QStringList options = choiceField->options();
                        int index = options.indexOf(selectedText);
                        if (index != -1) return index;
                    }
                    return selectedText; // Fallback to text value
                }
            }
            break;
        }
        case Type::Signature:
        case Type::Unknown:
        default:
            break;
    }
    return QVariant(); // Default invalid variant
}

QString PdfFormField::text() const
{
    // Alias for value() if it's a text field
    QVariant val = value();
    return val.canConvert<QString>() ? val.toString() : QString();
}

QStringList PdfFormField::choiceOptions() const
{
    if (d->typeVal == Type::Choice && d->popplerField) {
        Poppler::FormFieldChoice* choiceField = dynamic_cast<Poppler::FormFieldChoice*>(d->popplerField);
        if (choiceField) {
            return choiceField->options(); // Returns QStringList of option texts
        }
    }
    return QStringList(); // Default empty list
}

int PdfFormField::selectedChoiceIndex() const
{
    QVariant val = value();
    if (val.type() == QVariant::Int) {
        return val.toInt();
    } else if (val.type() == QVariant::StringList) {
        // If value returned a list of selected texts, find their indices
        QStringList selectedTexts = val.toStringList();
        QStringList allOptions = choiceOptions();
        QList<int> indices;
        for (const QString& text : selectedTexts) {
            int index = allOptions.indexOf(text);
            if (index != -1) indices.append(index);
        }
        // Return first index or handle multiple - let's return first or -1
        return indices.isEmpty() ? -1 : indices.first();
    }
    return -1; // Default not found
}

QString PdfFormField::selectedChoiceText() const
{
    int index = selectedChoiceIndex();
    if (index != -1) {
        QStringList options = choiceOptions();
        if (index < options.size()) {
            return options[index];
        }
    }
    return QString(); // Default empty
}

bool PdfFormField::isChecked() const
{
    if (d->typeVal == Type::Button && d->popplerField) {
        Poppler::FormFieldButton* buttonField = dynamic_cast<Poppler::FormFieldButton*>(d->popplerField);
        if (buttonField && (d->buttonTypeVal == ButtonType::CheckBox || d->buttonTypeVal == ButtonType::Radio)) {
            return buttonField->state();
        }
    }
    return false; // Default unchecked
}

bool PdfFormField::isEnabled() const
{
    return d->popplerField ? d->popplerField->isEnabled() : false;
}

bool PdfFormField::isReadOnly() const
{
    return d->popplerField ? d->popplerField->isReadOnly() : true; // Default to read-only if no field
}

bool PdfFormField::isRequired() const
{
    // Poppler::FormField might have an isRequired() method or a flags check.
    // Check Poppler API.
    // For now, assume not directly available or requires flag checking.
    LOG_WARN("PdfFormField::isRequired: Requires checking Poppler::FormField flags.");
    return false; // Default
}

bool PdfFormField::isVisible() const
{
    // Poppler::FormField might have an isVisible() method or a flags check.
    LOG_WARN("PdfFormField::isVisible: Requires checking Poppler::FormField flags.");
    return true; // Default visible
}

QString PdfFormField::toolTip() const
{
    // Poppler::FormField might have a method for tooltip/alternate text.
    LOG_WARN("PdfFormField::toolTip: Not directly available in Poppler-Qt5 API.");
    return QString();
}

QString PdfFormField::statusText() const
{
    // Status text is often related to validation errors, not directly stored in the field itself.
    LOG_WARN("PdfFormField::statusText: Not directly available in Poppler-Qt5 API.");
    return QString();
}

QColor PdfFormField::textColor() const
{
    // Poppler::FormField might have a method for text color.
    // Check Poppler API for appearance characteristics.
    LOG_WARN("PdfFormField::textColor: Not directly available in Poppler-Qt5 API.");
    return QColor(); // Default black
}

QColor PdfFormField::backgroundColor() const
{
    // Poppler::FormField might have a method for background color.
    LOG_WARN("PdfFormField::backgroundColor: Not directly available in Poppler-Qt5 API.");
    return QColor(); // Default transparent/white
}

QString PdfFormField::fontName() const
{
    // Poppler::FormField might have a method for font name.
    LOG_WARN("PdfFormField::fontName: Not directly available in Poppler-Qt5 API.");
    return QString(); // Default
}

qreal PdfFormField::fontSize() const
{
    // Poppler::FormField might have a method for font size.
    LOG_WARN("PdfFormField::fontSize: Not directly available in Poppler-Qt5 API.");
    return 0.0; // Default
}

bool PdfFormField::setValue(const QVariant& value)
{
    if (!d->popplerField) return false;

    // Setting the value depends on the field type.
    // Poppler::FormField objects are read-only in Poppler-Qt5.
    // We can store the *intended* new value locally, but applying it requires
    // a PDF writing library.
    bool success = false;
    switch (d->typeVal) {
        case Type::Text: {
            if (value.canConvert<QString>()) {
                // Store locally, mark as modified
                // d->localTextValue = value.toString(); // Assuming such a member exists
                // d->isModified = true;
                success = true;
                LOG_DEBUG("Marked PdfFormField '" << name() << "' text value for change (requires saving with writer). New value: " << value.toString());
            }
            break;
        }
        case Type::Button: {
            if (value.canConvert<bool>() && (d->buttonTypeVal == ButtonType::CheckBox || d->buttonTypeVal == ButtonType::Radio)) {
                // Store locally, mark as modified
                // d->localCheckedState = value.toBool();
                // d->isModified = true;
                success = true;
                LOG_DEBUG("Marked PdfFormField '" << name() << "' checked state for change (requires saving with writer). New state: " << value.toBool());
            }
            break;
        }
        case Type::Choice: {
            if (value.canConvert<int>()) {
                int index = value.toInt();
                QStringList options = choiceOptions();
                if (index >= 0 && index < options.size()) {
                    // Store locally, mark as modified
                    // d->localSelectedIndex = index;
                    // d->isModified = true;
                    success = true;
                    LOG_DEBUG("Marked PdfFormField '" << name() << "' selected index for change (requires saving with writer). New index: " << index);
                }
            } else if (value.canConvert<QString>()) {
                QString text = value.toString();
                QStringList options = choiceOptions();
                int index = options.indexOf(text);
                if (index != -1) {
                    // Store locally, mark as modified
                    // d->localSelectedIndex = index;
                    // d->isModified = true;
                    success = true;
                    LOG_DEBUG("Marked PdfFormField '" << name() << "' selected text for change (requires saving with writer). New text: " << text);
                }
            }
            break;
        }
        case Type::Signature:
        case Type::Unknown:
        default:
            break;
    }

    if (success) {
        emit valueChanged();
    }
    return success;
}

bool PdfFormField::setText(const QString& text)
{
    if (d->popplerField && d->typeVal == Type::Text) {
        if (text != d->initialTextValue) {
            d->modified = true;
            // Store new value locally if needed for QPDF writing
            // d->localTextValue = text;
            emit valueChanged();
            if (d->document) {
                 AnnotationManager::instance().markDocumentAsModified(d->document); // Or a specific form field modification signal/flag
                 // PdfDocument might need its own markFormFieldModified method
                 // d->document->markFormFieldModified(this); // Hypothetical
            }
            return true;
        }
    }
    return false;
}


bool PdfFormField::setSelectedChoiceIndex(int index)
{
    return setValue(index); // Delegates to setValue for Choice fields
}

bool PdfFormField::setSelectedChoiceText(const QString& text)
{
    return setValue(text); // Delegates to setValue for Choice fields
}

bool PdfFormField::setChecked(bool checked)
{
     if (d->popplerField && d->typeVal == Type::Button && (d->buttonTypeVal == ButtonType::CheckBox || d->buttonTypeVal == ButtonType::Radio)) {
        if (checked != d->initialCheckedState) {
            d->modified = true;
            // Store new value locally if needed
            // d->localCheckedState = checked;
            emit valueChanged();
            if (d->document) {
                 AnnotationManager::instance().markDocumentAsModified(d->document);
            }
            return true;
        }
    }
    return false;
}

// Add a getter for the modified state
bool PdfFormField::isModified() const
{
    return d->modified;
}

void PdfFormField::syncToPopplerObject()
{
    // As with PdfAnnotation, Poppler::FormField objects are read-only.
    // This function would serialize the locally stored intended changes
    // for application during document save using an external writing tool.
    LOG_WARN("PdfFormField::syncToPopplerObject: Poppler-Qt5 FormFields are read-only. Syncing requires a PDF writing library.");
}

Poppler::FormField* PdfFormField::popplerFormField() const
{
    return d->popplerField;
}

PdfDocument* PdfFormField::document() const
{
    return d->document;
}

} // namespace QuantilyxDoc