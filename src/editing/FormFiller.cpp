/**
 * QuantilyxDoc - Professional Document Editor
 * Copyright (C) 2025 R² Innovative Software
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */
#include "FormFiller.h"
#include "../core/Document.h"
#include "../core/Logger.h"
#include <QPointer>
#include <QMutex>
#include <QMutexLocker>
#include <QTimer>
#include <QMetaObject>
#include <QRegularExpression>
#include <QDebug>

namespace QuantilyxDoc {

// Forward declaration of base FormField class if not included
// class FormField { ... };

class FormFiller::Private {
public:
    Private(FormFiller* q_ptr)
        : q(q_ptr), activeDocument(nullptr) {}

    FormFiller* q;
    QPointer<Document> activeDocument; // Use QPointer for safety
    mutable QMutex mutex; // Protect access to the fields map during concurrent access
    QHash<Document*, QHash<QString, QPointer<FormField>>> docToFields; // Map Doc -> (FieldName -> Field*)
    QHash<Document*, bool> docModifiedMap; // Track if a document's forms have been modified

    // Helper to add a field to the internal map
    void addToMap(Document* doc, const QString& fieldName, FormField* field) {
        QMutexLocker locker(&mutex);
        docToFields[doc][fieldName] = field;
        LOG_DEBUG("FormFiller: Added form field '" << fieldName << "' for doc: " << doc->filePath());
    }

    // Helper to mark a document as modified
    void markDocumentAsModified(Document* doc) {
        QMutexLocker locker(&mutex);
        docModifiedMap[doc] = true;
        LOG_DEBUG("FormFiller: Marked document as modified (forms): " << doc->filePath());
        // Could emit a signal here if needed by Document/MainWindow
    }
};

// Static instance pointer
FormFiller* FormFiller::s_instance = nullptr;

FormFiller& FormFiller::instance()
{
    if (!s_instance) {
        s_instance = new FormFiller();
    }
    return *s_instance;
}

FormFiller::FormFiller(QObject* parent)
    : QObject(parent)
    , d(new Private(this))
{
    LOG_INFO("FormFiller created.");
}

FormFiller::~FormFiller()
{
    LOG_INFO("FormFiller destroyed.");
}

QList<FormFieldInfo> FormFiller::getAllFormFields(Document* document) const
{
    if (!document) {
        LOG_ERROR("FormFiller::getAllFormFields: Null document provided.");
        return {};
    }

    // This requires the Document or its pages to provide access to FormField objects.
    // For PDF (Poppler), this comes from Poppler::Document::formFields().
    // We need to iterate pages or use the document-level list and convert Poppler::FormField to our FormFieldInfo.
    QList<FormFieldInfo> results;

    // Example for PDF using Poppler (assuming PdfDocument exposes Poppler fields):
    // PdfDocument* pdfDoc = dynamic_cast<PdfDocument*>(document);
    // if (pdfDoc && pdfDoc->popplerDocument()) {
    //     auto popplerFields = pdfDoc->popplerDocument()->formFields();
    //     for (auto* popplerField : popplerFields) {
    //         if (popplerField) {
    //             FormFieldInfo info = convertPopplerFieldToInfo(popplerField, pdfDoc); // Hypothetical conversion helper
    //             results.append(info);
    //         }
    //     }
    // }

    LOG_WARN("FormFiller::getAllFormFields: Requires Document subclass (e.g., PdfDocument) to expose form fields. Returning empty list.");
    return results; // Placeholder
}

QList<FormFieldInfo> FormFiller::getFormFieldsForPage(Document* document, int pageIndex) const
{
    if (!document || pageIndex < 0 || pageIndex >= document->pageCount()) {
        LOG_ERROR("FormFiller::getFormFieldsForPage: Invalid document or page index.");
        return {};
    }

    // Similar to getAllFormFields, but filter by page index.
    // PdfPage* pdfPage = dynamic_cast<PdfPage*>(document->page(pageIndex));
    // if (pdfPage && pdfPage->popplerPage()) {
    //     // Get fields associated with this specific Poppler::Page.
    //     // Poppler::Page might not directly list its fields. The list often comes from Document::formFields().
    //     // We'd need to iterate the *document's* field list and check which page each field belongs to.
    //     // This requires knowing the page index of each Poppler::FormField.
    //     // Poppler::FormField might have a page() method or we get it from the Document::formFields() list ordering/context.
    //     auto allFields = getAllFormFields(document);
    //     QList<FormFieldInfo> pageFields;
    //     for (const auto& field : allFields) {
    //         if (field.pageIndex == pageIndex) {
    //             pageFields.append(field);
    //         }
    //     }
    //     return pageFields;
    // }

    LOG_WARN("FormFiller::getFormFieldsForPage: Requires Document/PdfPage to expose form fields per page. Returning empty list.");
    return {}; // Placeholder
}

FormFieldInfo FormFiller::getFormFieldByName(Document* document, const QString& fieldName) const
{
    if (!document || fieldName.isEmpty()) {
        LOG_ERROR("FormFiller::getFormFieldByName: Invalid document or field name.");
        return FormFieldInfo(); // Return default-constructed invalid info
    }

    // Iterate through all fields for the document to find the one with the matching name.
    auto allFields = getAllFormFields(document);
    auto it = std::find_if(allFields.begin(), allFields.end(),
                           [&fieldName](const FormFieldInfo& info) { return info.name == fieldName; });
    if (it != allFields.end()) {
        LOG_DEBUG("FormFiller: Found form field '" << fieldName << "' in doc: " << document->filePath());
        return *it;
    }

    LOG_DEBUG("FormFiller: Form field '" << fieldName << "' not found in doc: " << document->filePath());
    return FormFieldInfo(); // Return default-constructed invalid info
}

QVariant FormFiller::getFieldValue(Document* document, const QString& fieldName) const
{
    FormFieldInfo info = getFormFieldByName(document, fieldName);
    if (!info.name.isEmpty()) { // Check if field was found
        // The value comes from the underlying Poppler::FormField or similar object.
        // For example, for a text field: popplerTextField->text()
        // For a checkbox: popplerCheckBoxField->state()
        // For a combobox: popplerComboBoxField->currentValue() (returns index or text)
        // This requires the FormFieldInfo struct to contain a pointer to the underlying object or a way to retrieve its value.

        // For now, we'll assume FormFieldInfo has a 'value' member populated during retrieval.
        LOG_DEBUG("FormFiller: Got value for field '" << fieldName << "' in doc: " << document->filePath());
        return info.value;
    }
    LOG_WARN("FormFiller::getFieldValue: Field '" << fieldName << "' not found in doc: " << document->filePath());
    return QVariant(); // Return invalid variant if not found
}

bool FormFiller::setFieldValue(Document* document, const QString& fieldName, const QVariant& value)
{
    if (!document || fieldName.isEmpty()) {
        LOG_ERROR("FormFiller::setFieldValue: Invalid document or field name.");
        return false;
    }

    // --- CRITICAL: Poppler-Qt5 is READ-ONLY for Form Fields ---
    // Modifying the value of a Poppler::FormField directly is not possible using Poppler-Qt5.
    // We need to store the intended new value and apply it during document save using an external tool or writer library.

    // Find the field
    auto allFields = getAllFormFields(document);
    auto it = std::find_if(allFields.begin(), allFields.end(),
                           [&fieldName](const FormFieldInfo& info) { return info.name == fieldName; });

    if (it != allFields.end()) {
        // Validate the value against the field type if possible
        bool isValid = false;
        switch (it->type) {
            case FormFieldType::Text:
                isValid = value.canConvert<QString>();
                break;
            case FormFieldType::Button:
                if (it->buttonType == FormFieldButton::CheckBox || it->buttonType == FormFieldButton::Radio) {
                    isValid = value.canConvert<bool>();
                } else {
                    isValid = false; // Push button usually doesn't have a settable value
                }
                break;
            case FormFieldType::Choice:
                if (value.canConvert<QString>()) {
                    isValid = it->options.contains(value.toString()); // Check if string is a valid option
                } else if (value.canConvert<int>()) {
                    isValid = (value.toInt() >= 0 && value.toInt() < it->options.size()); // Check if int is a valid index
                }
                break;
            // Add checks for other types if needed
            default:
                isValid = true; // Assume valid for types not strictly validated here
        }

        if (!isValid) {
            LOG_ERROR("FormFiller::setFieldValue: Invalid value type for field '" << fieldName << "' (type: " << static_cast<int>(it->type) << "). Value: " << value.toString());
            return false;
        }

        // Store the intended new value within the document or in a separate map managed by the FormFiller.
        // This requires a mechanism to associate the new value with the field for the save process.
        // For example, the Document object could have a map: fieldName -> newValue.
        // document->setPendingFormFieldValue(fieldName, value); // Hypothetical method

        // Mark document as modified
        d->markDocumentAsModified(document);

        LOG_INFO("FormFiller: Set value for field '" << fieldName << "' in doc: " << document->filePath() << " (value: " << value.toString() << "). Stored for saving.");
        emit formFieldChanged(document, fieldName, value);
        return true;
    } else {
        LOG_WARN("FormFiller::setFieldValue: Field '" << fieldName << "' not found in doc: " << document->filePath());
        return false;
    }
}

bool FormFiller::resetForm(Document* document)
{
    if (!document) {
        LOG_ERROR("FormFiller::resetForm: Null document provided.");
        return false;
    }

    // Get all fields and set them back to their default values (if stored or determinable).
    // This requires knowing the *default* value for each field.
    // Poppler might expose default values (e.g., FormFieldText::defaultValue(), FormFieldChoice::defaultChoice()).
    // We'd iterate fields and call setFieldValue with the default.

    auto allFields = getAllFormFields(document);
    bool success = true;
    for (const auto& field : allFields) {
        QVariant defaultValue; // Get default from Poppler field object
        // Example: if (auto* popplerField = getPopplerFieldByName(field.name)) { ... get default ... }
        // For now, assume we have a way to get defaults.
        // bool fieldSuccess = setFieldValue(document, field.name, defaultValue);
        // success = success && fieldSuccess;
        Q_UNUSED(field);
        LOG_WARN("FormFiller::resetForm: Requires access to default values from underlying format (e.g., Poppler::FormField::defaultValue).");
    }

    if (success) {
        LOG_INFO("FormFiller: Reset form in doc: " << document->filePath());
    } else {
        LOG_ERROR("FormFiller::resetForm: Failed to reset all fields in doc: " << document->filePath());
    }
    return success;
}

bool FormFiller::resetFormField(Document* document, const QString& fieldName)
{
    if (!document || fieldName.isEmpty()) {
        LOG_ERROR("FormFiller::resetFormField: Invalid document or field name.");
        return false;
    }

    // Get the specific field and its default value
    // FormFieldInfo info = getFormFieldByName(document, fieldName);
    // if (!info.name.isEmpty()) {
    //     QVariant defaultValue; // Get default from Poppler field object
    //     // Example: if (auto* popplerField = getPopplerFieldByName(fieldName)) { ... get default ... }
    //     // For now, assume we have a way to get defaults.
    //     bool success = setFieldValue(document, fieldName, defaultValue);
    //     if (success) {
    //         LOG_INFO("FormFiller: Reset field '" << fieldName << "' in doc: " << document->filePath());
    //     } else {
    //         LOG_ERROR("FormFiller::resetFormField: Failed to reset field '" << fieldName << "' in doc: " << document->filePath());
    //     }
    //     return success;
    // }

    LOG_WARN("FormFiller::resetFormField: Requires access to default value from underlying format (e.g., Poppler::FormField::defaultValue).");
    return false; // Placeholder
}

bool FormFiller::validateForm(Document* document) const
{
    if (!document) {
        LOG_ERROR("FormFiller::validateForm: Null document provided.");
        return false;
    }

    // Iterate through all fields and check their validity based on type, constraints, and current value.
    // This could involve checking for required fields, data types (email, number), regex patterns, etc.
    // Many of these checks are defined within the PDF specification for AcroForm fields.
    // Poppler might expose some validation information, or we need to implement it based on field properties.

    auto allFields = getAllFormFields(document);
    bool allValid = true;
    for (const auto& field : allFields) {
        bool fieldValid = validateFormFieldInternal(document, field); // Hypothetical internal validation helper
        if (!fieldValid) {
             LOG_WARN("FormFiller::validateForm: Field '" << field.name << "' in doc " << document->filePath() << " is invalid.");
             allValid = false;
             emit validationError(document, field.name, "Field validation failed."); // Emit for UI
        }
    }

    LOG_DEBUG("FormFiller: Validated form in doc: " << document->filePath() << ". All valid: " << allValid);
    return allValid;
}

bool FormFiller::validateFormField(Document* document, const QString& fieldName) const
{
    if (!document || fieldName.isEmpty()) {
        LOG_ERROR("FormFiller::validateFormField: Invalid document or field name.");
        return false;
    }

    FormFieldInfo info = getFormFieldByName(document, fieldName);
    if (!info.name.isEmpty()) {
        bool valid = validateFormFieldInternal(document, info); // Hypothetical internal validation helper
        if (!valid) {
            LOG_WARN("FormFiller::validateFormField: Field '" << fieldName << "' in doc " << document->filePath() << " is invalid.");
            emit validationError(document, fieldName, "Field validation failed.");
        }
        return valid;
    }

    LOG_WARN("FormFiller::validateFormField: Field '" << fieldName << "' not found in doc: " << document->filePath());
    return false; // Field not found is considered invalid
}

bool FormFiller::submitForm(Document* document, const QString& submitUrl)
{
    if (!document) {
        LOG_ERROR("FormFiller::submitForm: Null document provided.");
        return false;
    }

    // Check if the document has a submit URL defined in its AcroForm dictionary
    // If submitUrl is empty, use the one from the document.
    // QString urlToSubmit = submitUrl.isEmpty() ? getSubmitUrlFromDocument(document) : submitUrl; // Hypothetical helper
    // if (urlToSubmit.isEmpty()) {
    //     LOG_ERROR("FormFiller::submitForm: No submit URL provided or found in document: " << document->filePath());
    //     return false;
    // }

    // Gather all field values
    // QVariantMap formData;
    // auto allFields = getAllFormFields(document);
    // for (const auto& field : allFields) {
    //     formData[field.name] = getFieldValue(document, field.name); // Use our getter
    // }

    // Submit data via HTTP POST (or GET) to the URL
    // QNetworkAccessManager nam; // Qt Network module
    // QNetworkRequest request(urlToSubmit);
    // request.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded"); // Or multipart/form-data
    // QUrlQuery query; // Qt Core
    // for (auto it = formData.constBegin(); it != formData.constEnd(); ++it) {
    //     query.addQueryItem(it.key(), it.value().toString()); // Simplified, might need more complex encoding
    // }
    // QNetworkReply* reply = nam.post(request, query.toString(QUrl::FullyEncoded).toUtf8());
    // // Handle reply asynchronously...
    // connect(reply, &QNetworkReply::finished, [reply, document]() {
    //     if (reply->error() == QNetworkReply::NoError) {
    //         LOG_INFO("FormFiller: Form submitted successfully for doc: " << document->filePath());
    //         emit formSubmitted(document, reply->url().toString());
    //     } else {
    //         LOG_ERROR("FormFiller: Form submission failed for doc: " << document->filePath() << ". Error: " << reply->errorString());
    //         emit formSubmissionFailed(document, reply->url().toString(), reply->errorString());
    //     }
    //     reply->deleteLater();
    // });

    LOG_WARN("FormFiller::submitForm: Requires implementation for HTTP submission and access to document's submit action.");
    return false; // Placeholder
}

bool FormFiller::hasFormFields(Document* document) const
{
    if (!document) {
        LOG_ERROR("FormFiller::hasFormFields: Null document provided.");
        return false;
    }

    // Check if the document format supports forms and if any are present.
    // For PDF: check popplerDoc->hasFormFields()
    // return document->hasFormFields(); // Hypothetical method on Document

    LOG_WARN("FormFiller::hasFormFields: Requires Document subclass to implement check (e.g., PdfDocument::hasFormFields()).");
    return false; // Placeholder
}

int FormFiller::formFieldCount(Document* document) const
{
    if (!document) {
        LOG_ERROR("FormFiller::formFieldCount: Null document provided.");
        return 0;
    }

    return getAllFormFields(document).size(); // Less efficient, but works
}

int FormFiller::formFieldCountForPage(Document* document, int pageIndex) const
{
    if (!document || pageIndex < 0 || pageIndex >= document->pageCount()) {
        LOG_ERROR("FormFiller::formFieldCountForPage: Invalid document or page index.");
        return 0;
    }

    return getFormFieldsForPage(document, pageIndex).size(); // Less efficient, but works
}

bool FormFiller::flattenForm(Document* document)
{
    if (!document) {
        LOG_ERROR("FormFiller::flattenForm: Null document provided.");
        return false;
    }

    // Flattening means converting form fields into static content on the page.
    // This removes the interactive nature of the fields.
    // This is typically a feature of PDF writing libraries.
    // Poppler-Qt5 cannot flatten forms directly.
    // Requires external tool (e.g., QPDF) or PDF writer library integration.

    LOG_WARN("FormFiller::flattenForm: Requires PDF writing library or external tool (e.g., QPDF) to flatten form fields into static content.");
    return false; // Not possible with Poppler-Qt5 alone
}

void FormFiller::setActiveDocument(Document* document)
{
    QMutexLocker locker(&d->mutex);
    d->activeDocument = document; // Use QPointer
    LOG_DEBUG("FormFiller: Set active document to: " << (document ? document->filePath() : "nullptr"));
}

Document* FormFiller::activeDocument() const
{
    QMutexLocker locker(&d->mutex);
    return d->activeDocument.data(); // Returns nullptr if document was deleted
}

// --- Private Helpers (or could be public if needed elsewhere) ---

bool FormFiller::validateFormFieldInternal(Document* document, const FormFieldInfo& fieldInfo) const
{
    Q_UNUSED(document); // Might be needed for complex cross-field validation
    // Perform validation based on field properties like isRequired, dataType, regex, etc.
    // This requires the FormFieldInfo to contain validation rules extracted from the PDF.
    // Example checks:
    if (fieldInfo.isRequired && fieldInfo.value.toString().isEmpty()) {
        LOG_DEBUG("FormFiller: validateFormFieldInternal: Field '" << fieldInfo.name << "' is required but empty.");
        return false;
    }
    if (fieldInfo.type == FormFieldType::Text) {
        // Example: Check email regex if field is marked as email type
        // QRegularExpression emailRegex(R"(\b[A-Za-z0-9._%+-]+@[A-Za-z0-9.-]+\.[A-Z|a-z]{2,}\b)");
        // if (fieldInfo.subtype == "Email" && !emailRegex.match(fieldInfo.value.toString()).hasMatch()) {
        //     return false;
        // }
    }
    // Add more checks based on field properties (regex, data type, etc.)
    return true; // Placeholder: assume valid if not obviously invalid
}

} // namespace QuantilyxDoc