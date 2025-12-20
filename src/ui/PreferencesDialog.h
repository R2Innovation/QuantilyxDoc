/**
 * QuantilyxDoc - Professional Document Editor
 * Copyright (C) 2025 R² Innovative Software
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */
#ifndef QUANTILYX_PREFERENCESDIALOG_H
#define QUANTILYX_PREFERENCESDIALOG_H

#include <QDialog>
#include <QTabWidget>
#include <QDialogButtonBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QScrollArea>
#include <QCheckBox>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QLineEdit>
#include <QComboBox>
#include <QLabel>
#include <QPushButton>
#include <QGroupBox>
#include <QColorDialog>
#include <QFontDialog>
#include <QSpinBox>
#include <QListWidget>
#include <QStackedWidget>
#include <memory>

namespace QuantilyxDoc {

/**
 * @brief Dialog for modifying application preferences.
 *
 * Provides a tabbed interface to configure various aspects of the application,
 * such as display settings, editor behavior, security options, and advanced settings.
 * It interacts with the central Settings system.
 */
class PreferencesDialog : public QDialog
{
    Q_OBJECT

public:
    /**
     * @brief Constructor.
     * @param parent Parent widget.
     */
    explicit PreferencesDialog(QWidget* parent = nullptr);

    /**
     * @brief Destructor.
     */
    ~PreferencesDialog() override;

private slots:
    /**
     * @brief Apply changes without closing the dialog.
     */
    void applyChanges();

    /**
     * @brief Slot called when a tab is changed.
     * @param index Index of the new tab.
     */
    void onTabChanged(int index);

    /**
     * @brief Slot to open a color dialog for a specific setting.
     */
    void openColorDialog();

    /**
     * @brief Slot to open a font dialog for a specific setting.
     */
    void openFontDialog();

private:
    class Private;
    std::unique_ptr<Private> d;

    // --- UI Pages ---
    QWidget* createDisplayPage();
    QWidget* createEditorPage();
    QWidget* createSecurityPage();
    QWidget* createAdvancedPage();
    QWidget* createProfilesPage();

    // --- Helper methods ---
    void loadSettings();  // Load settings from Settings into UI
    void saveSettings();  // Save UI values to Settings
    void validateSettings(); // Validate UI values before saving
};

} // namespace QuantilyxDoc

#endif // QUANTILYX_PREFERENCESDIALOG_H