/**
 * QuantilyxDoc - Professional Document Editor
 * Copyright (C) 2025 R² Innovative Software
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */
#ifndef QUANTILYX_FORMFILLER_H
#define QUANTILYX_FORMFILLER_H

#include <QObject>
#include <QList>
#include <QMap>
#include <QVariant>
#include <QHash>
#include <memory>

namespace QuantilyxDoc {

class Document; // Forward declaration
class FormField; // Forward declaration - assuming a base class exists or will be defined

/**
 * @brief Enum describing the type of form field.
 */
enum class FormFieldType {
    PushButton,
    CheckBox,
    RadioButton,
    TextField,
    ComboBox,
    ListBox,
    Signature,
    Unknown
};

/**
 * @brief Structure holding information about a form field.
 */
struct FormFieldInfo {
    QString name;                 // Name of the field
    QString alternateName;        // Alternate name (if present)
    QString mappingName;          // Mapping name (if present)
    FormFieldType type;           // Type of the field
    QString value;                // Current value (for text, list selection index, etc.)
    QVariantMap options;          // Options for choices (e.g., list items for ComboBox/Listbox, checked state for Radio/Checkbox)
    bool isReadOnly;              // Whether the field is read-only
    bool isRequired;              // Whether the field is required
    bool isVisible;               // Whether the field is visible
    QRectF bounds;                // Bounding rectangle on the page
    int pageIndex;                // Page index the field is on
    QString exportValue;          // Value to be exported if different from display value (e.g., for radio buttons)
    // Add more properties as needed per type
};

/**
 * @brief Manages the filling and interaction with form fields within documents.
 * 
 * Provides methods to retrieve form field information, set/get values,
 * check validity, and submit forms (if supported by the format).
 */
class FormFiller : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief Constructor.
     * @param parent Parent object.
     */
    explicit FormFiller(QObject* parent = nullptr);

    /**
     * @brief Destructor.
     */
    ~FormFiller() override;

    /**
     * @brief Get singleton instance.
     * @return Reference to the global FormFiller instance.
     */
    static FormFiller& instance();

    /**
     * @brief Get a list of all form fields in the document.
     * @param document The document to query.
     * @return List of FormFieldInfo structures.
     */
    QList<FormFieldInfo> getAllFormFields(Document* document) const;

    /**
     * @brief Get a list of all form fields on a specific page.
     * @param document The document containing the page.
     * @param pageIndex The index of the page.
     * @return List of FormFieldInfo structures.
     */
    QList<FormFieldInfo> getFormFieldsForPage(Document* document, int pageIndex) const;

    /**
     * @brief Get information about a specific form field by its name.
     * @param document The document containing the field.
     * @param fieldName The name of the field.
     * @return FormFieldInfo structure, or an invalid one if not found.
     */
    FormFieldInfo getFormFieldByName(Document* document, const QString& fieldName) const;

    /**
     * @brief Get the value of a specific form field.
     * @param document The document containing the field.
     * @param fieldName The name of the field.
     * @return The value as a QVariant (QString for text, bool for checkbox, int for list index, etc.).
     */
    QVariant getFieldValue(Document* document, const QString& fieldName) const;

    /**
     * @brief Set the value of a specific form field.
     * @param document The document containing the field.
     * @param fieldName The name of the field.
     * @param value The new value.
     * @return True if setting the value was successful.
     */
    bool setFieldValue(Document* document, const QString& fieldName, const QVariant& value);

    /**
     * @brief Reset all form fields in the document to their default values.
     * @param document The document to reset.
     * @return True if reset was successful.
     */
    bool resetForm(Document* document);

    /**
     * @brief Reset a specific form field to its default value.
     * @param document The document containing the field.
     * @param fieldName The name of the field.
     * @return True if reset was successful.
     */
    bool resetFormField(Document* document, const QString& fieldName);

    /**
     * @brief Validate the values of all form fields in the document.
     * @param document The document to validate.
     * @return True if all fields are valid according to their constraints.
     */
    bool validateForm(Document* document) const;

    /**
     * @brief Validate the value of a specific form field.
     * @param document The document containing the field.
     * @param fieldName The name of the field.
     * @return True if the field's value is valid.
     */
    bool validateFormField(Document* document, const QString& fieldName) const;

    /**
     * @brief Submit the form data (if the document format supports it).
     * This might involve sending data to a server or saving to a specific format.
     * @param document The document containing the form.
     * @param submitUrl Optional URL to submit the data to (for web-based forms embedded in PDF).
     * @return True if submission was successful.
     */
    bool submitForm(Document* document, const QString& submitUrl = QString());

    /**
     * @brief Check if the document contains any form fields.
     * @param document The document to check.
     * @return True if the document has form fields.
     */
    bool hasFormFields(Document* document) const;

    /**
     * @brief Get the number of form fields in the document.
     * @param document The document.
     * @return Count of form fields.
     */
    int formFieldCount(Document* document) const;

    /**
     * @brief Get the number of form fields on a specific page.
     * @param document The document containing the page.
     * @param pageIndex The index of the page.
     * @return Count of form fields on the page.
     */
    int formFieldCountForPage(Document* document, int pageIndex) const;

    /**
     * @brief Flatten the form fields (make them non-interactive, part of the page content).
     * This is often done before printing or saving a final version.
     * @param document The document to flatten.
     * @return True if flattening was successful.
     */
    bool flattenForm(Document* document);

    /**
     * @brief Set the active document for the filler.
     * @param document The active document.
     */
    void setActiveDocument(Document* document);

    /**
     * @brief Get the currently active document for the filler.
     * @return Pointer to the active document, or nullptr.
     */
    Document* activeDocument() const;

signals:
    /**
     * @brief Emitted when a form field's value changes.
     * @param document Pointer to the document containing the field.
     * @param fieldName Name of the field that changed.
     * @param newValue The new value of the field.
     */
    void formFieldChanged(QuantilyxDoc::Document* document, const QString& fieldName, const QVariant& newValue);

    /**
     * @brief Emitted when the list of form fields in a document changes (e.g., added/removed via scripting or editing).
     * @param document Pointer to the document.
     */
    void formFieldsChanged(QuantilyxDoc::Document* document);

    /**
     * @brief Emitted when a form validation error occurs.
     * @param document Pointer to the document.
     * @param fieldName Name of the field with the error.
     * @param errorMessage Description of the error.
     */
    void validationError(QuantilyxDoc::Document* document, const QString& fieldName, const QString& errorMessage);

    /**
     * @brief Emitted when a form is submitted successfully.
     * @param document Pointer to the document.
     * @param submitUrl URL the form was submitted to (if applicable).
     */
    void formSubmitted(QuantilyxDoc::Document* document, const QString& submitUrl);

    /**
     * @brief Emitted when a form submission fails.
     * @param document Pointer to the document.
     * @param submitUrl URL the form was submitted to (if applicable).
     * @param error Error message.
     */
    void formSubmissionFailed(QuantilyxDoc::Document* document, const QString& submitUrl, const QString& error);

private:
    class Private;
    std::unique_ptr<Private> d;

    // Helper to convert internal field representation to FormFieldInfo
    FormFieldInfo fieldToInfo(void* internalFieldRepresentation, int pageIndex) const; // Placeholder for internal type
    // Helper to update the document's modification state after a form change
    void markDocumentAsModified(Document* document);
};

} // namespace QuantilyxDoc

#endif // QUANTILYX_FORMFILLER_H