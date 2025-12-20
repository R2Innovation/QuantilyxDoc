/**
 * QuantilyxDoc - Professional Document Editor
 * Copyright (C) 2025 R² Innovative Software
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */
#ifndef QUANTILYX_PDFFORMFIELD_H
#define QUANTILYX_PDFFORMFIELD_H

#include <QObject>
#include <poppler-qt5.h>
#include <memory>
#include <QRectF>
#include <QVariant>

namespace QuantilyxDoc {

class PdfDocument;

/**
 * @brief Represents a form field within a PDF document.
 * 
 * Wraps a Poppler::FormField object and provides QuantilyxDoc-specific
 * properties and methods for interacting with the field's value, state, etc.
 * Allows for reading and *potentially* modifying field values (though modification
 * might require external tools/libraries for saving, similar to annotations).
 */
class PdfFormField : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief Type of PDF form field.
     * Mirrors Poppler::FormField::FormType or defines a compatible set.
     */
    enum class Type {
        Unknown,
        Button,     // Checkbox, Radio Button, Push Button
        Text,       // Text Input
        Choice,     // ComboBox, ListBox
        Signature   // Digital Signature Field
    };

    /**
     * @brief Specific type for button fields.
     */
    enum class ButtonType {
        UnknownButton,
        Push,       // Push button (not stateful)
        CheckBox,   // Check box
        Radio       // Radio button
    };

    /**
     * @brief Specific type for choice fields.
     */
    enum class ChoiceType {
        UnknownChoice,
        ComboBox,   // Drop-down list
        ListBox     // List box
    };

    /**
     * @brief Constructor wrapping an existing Poppler form field.
     * @param popplerField The underlying Poppler form field object.
     * @param document The parent PdfDocument this field belongs to.
     * @param parent Parent object.
     */
    explicit PdfFormField(Poppler::FormField* popplerField, PdfDocument* document = nullptr, QObject* parent = nullptr);

    /**
     * @brief Destructor.
     */
    ~PdfFormField() override;

    /**
     * @brief Get the type of this form field.
     * @return The form field type.
     */
    Type type() const;

    /**
     * @brief Get the specific button type (if this field is a button).
     * @return The button type.
     */
    ButtonType buttonType() const;

    /**
     * @brief Get the specific choice type (if this field is a choice).
     * @return The choice type.
     */
    ChoiceType choiceType() const;

    /**
     * @brief Get the name of this form field.
     * @return Field name.
     */
    QString name() const;

    /**
     * @brief Get the fully qualified name of this form field (including parent names).
     * @return Fully qualified field name.
     */
    QString fullyQualifiedName() const;

    /**
     * @brief Get the ID of this form field (if available).
     * @return Field ID string.
     */
    QString id() const;

    /**
     * @brief Get the page index this field is located on.
     * @return Page index (0-based).
     */
    int pageIndex() const;

    /**
     * @brief Get the bounding rectangle of this field in PDF coordinates.
     * @return Rectangle in PDF coordinate system.
     */
    QRectF bounds() const;

    /**
     * @brief Get the value of this form field.
     * @return Field value (e.g., string for text, bool for checkbox, int for selected index in list).
     */
    QVariant value() const;

    /**
     * @brief Get the text content of this form field (for text fields).
     * @return Text content.
     */
    QString text() const;

    /**
     * @brief Get the list of options for choice fields.
     * @return List of option strings.
     */
    QStringList choiceOptions() const;

    /**
     * @brief Get the currently selected index for choice fields.
     * @return Selected index, or -1 if none.
     */
    int selectedChoiceIndex() const;

    /**
     * @brief Get the currently selected text for choice fields.
     * @return Selected text, or empty string.
     */
    QString selectedChoiceText() const;

    /**
     * @brief Check if this field is checked (for checkboxes/radio buttons).
     * @return True if checked.
     */
    bool isChecked() const;

    /**
     * @brief Check if this field is enabled.
     * @return True if enabled.
     */
    bool isEnabled() const;

    /**
     * @brief Check if this field is read-only.
     * @return True if read-only.
     */
    bool isReadOnly() const;

    /**
     * @brief Check if this field is required.
     * @return True if required.
     */
    bool isRequired() const;

    /**
     * @brief Check if this field is visible.
     * @return True if visible.
     */
    bool isVisible() const;

    /**
     * @brief Get the tooltip text for this field.
     * @return Tooltip string.
     */
    QString toolTip() const;

    /**
     * @brief Get the status text for this field (e.g., validation error).
     * @return Status string.
     */
    QString statusText() const;

    /**
     * @brief Get the text color of this field.
     * @return Text color.
     */
    QColor textColor() const;

    /**
     * @brief Get the background color of this field.
     * @return Background color.
     */
    QColor backgroundColor() const;

    /**
     * @brief Get the font name used by this field.
     * @return Font name string.
     */
    QString fontName() const;

    /**
     * @brief Get the font size used by this field.
     * @return Font size in points.
     */
    qreal fontSize() const;

    // --- Modification ---
    /**
     * @brief Set the value of this form field.
     * @param value New value to set.
     * @return True if setting the value was successful (might be limited by field type/properties).
     */
    bool setValue(const QVariant& value);

    /**
     * @brief Set the text content of this form field (for text fields).
     * @param text New text content.
     * @return True if setting the text was successful.
     */
    bool setText(const QString& text);

    /**
     * @brief Set the selected index for choice fields.
     * @param index Index to select.
     * @return True if setting the index was successful.
     */
    bool setSelectedChoiceIndex(int index);

    /**
     * @brief Set the selected text for choice fields.
     * @param text Text to select (must match an option).
     * @return True if setting the text was successful.
     */
    bool setSelectedChoiceText(const QString& text);

    /**
     * @brief Set the checked state of this field (for checkboxes/radio buttons).
     * @param checked New checked state.
     * @return True if setting the state was successful.
     */
    bool setChecked(bool checked);

    /**
     * @brief Update the field's internal Poppler object based on local changes.
     * This is needed before saving the document if the field was modified.
     * NOTE: Poppler-Qt5 is read-only. This function might serialize changes
     *       for later application by an external writing tool.
     */
    void syncToPopplerObject();

    /**
     * @brief Get the underlying Poppler form field object.
     * @return Poppler form field object or nullptr.
     */
    Poppler::FormField* popplerFormField() const;

    /**
     * @brief Get the parent PdfDocument this field belongs to.
     * @return Parent document pointer.
     */
    PdfDocument* document() const;

signals:
    /**
     * @brief Emitted when the field's value changes.
     */
    void valueChanged();

    /**
     * @brief Emitted when the field's properties (enabled, readonly, etc.) change.
     */
    void propertiesChanged();

private:
    class Private;
    std::unique_ptr<Private> d;
};

} // namespace QuantilyxDoc

#endif // QUANTILYX_PDFFORMFIELD_H