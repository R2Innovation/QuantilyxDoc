/**
 * QuantilyxDoc - Professional Document Editor
 * Copyright (C) 2025 R² Innovative Software
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#ifndef QUANTILYX_MAINWINDOW_H
#define QUANTILYX_MAINWINDOW_H

#include <QMainWindow>
#include <QString>
#include <memory>

namespace QuantilyxDoc {

// Forward declarations
class Document;
class DocumentView;

/**
 * @brief Main application window
 * 
 * Okular-style interface with dockable sidebars, customizable toolbars,
 * and tab-based document management.
 */
class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    /**
     * @brief Constructor
     * @param parent Parent widget
     */
    explicit MainWindow(QWidget* parent = nullptr);

    /**
     * @brief Destructor
     */
    ~MainWindow();

    /**
     * @brief Open a document
     * @param filePath Path to document file
     * @return true if opened successfully
     */
    bool openDocument(const QString& filePath);

    /**
     * @brief Create a new blank document
     * @return true if created successfully
     */
    bool newDocument();

    /**
     * @brief Close current document
     * @return true if closed successfully
     */
    bool closeDocument();

    /**
     * @brief Close all documents
     * @return true if all closed successfully
     */
    bool closeAllDocuments();

    /**
     * @brief Save current document
     * @return true if saved successfully
     */
    bool saveDocument();

    /**
     * @brief Save current document with new name
     * @return true if saved successfully
     */
    bool saveDocumentAs();

    /**
     * @brief Get current document
     * @return Pointer to current document or nullptr
     */
    Document* currentDocument() const;

    /**
     * @brief Get current document view
     * @return Pointer to current view or nullptr
     */
    DocumentView* currentView() const;

    /**
     * @brief Get number of open documents
     * @return Document count
     */
    int documentCount() const;

    /**
     * @brief Restore previous session
     * @return true if restored successfully
     */
    bool restoreSession();

    /**
     * @brief Save current session
     * @return true if saved successfully
     */
    bool saveSession();

public slots:
    /**
     * @brief Show about dialog
     */
    void showAboutDialog();

    /**
     * @brief Show preferences dialog
     */
    void showPreferences();

    /**
     * @brief Toggle fullscreen mode
     */
    void toggleFullscreen();

    /**
     * @brief Toggle sidebar visibility
     */
    void toggleSidebar();

    /**
     * @brief Toggle properties panel visibility
     */
    void togglePropertiesPanel();

    /**
     * @brief Switch to document by index
     * @param index Document tab index
     */
    void switchToDocument(int index);

protected:
    /**
     * @brief Handle close event
     * @param event Close event
     */
    void closeEvent(QCloseEvent* event) override;

    /**
     * @brief Handle drag enter event
     * @param event Drag enter event
     */
    void dragEnterEvent(QDragEnterEvent* event) override;

    /**
     * @brief Handle drop event
     * @param event Drop event
     */
    void dropEvent(QDropEvent* event) override;

private slots:
    /**
     * @brief Handle document modified state change
     */
    void onDocumentModified();

    /**
     * @brief Handle tab close request
     * @param index Tab index
     */
    void onTabCloseRequested(int index);

    /**
     * @brief Handle tab change
     * @param index New current tab index
     */
    void onCurrentTabChanged(int index);

    /**
     * @brief Update recent files menu
     */
    void updateRecentFilesMenu();

    /**
     * @brief Update window title
     */
    void updateWindowTitle();

    /**
     * @brief Update UI state
     */
    void updateUIState();

private:
    /**
     * @brief Create UI components
     */
    void createUI();

    /**
     * @brief Create menu bar
     */
    void createMenuBar();

    /**
     * @brief Create toolbars
     */
    void createToolbars();

    /**
     * @brief Create status bar
     */
    void createStatusBar();

    /**
     * @brief Create dock widgets
     */
    void createDockWidgets();

    /**
     * @brief Create central widget (tab widget)
     */
    void createCentralWidget();

    /**
     * @brief Setup connections
     */
    void setupConnections();

    /**
     * @brief Load window state
     */
    void loadWindowState();

    /**
     * @brief Save window state
     */
    void saveWindowState();

    /**
     * @brief Check if document has unsaved changes
     * @param doc Document to check
     * @return true if has unsaved changes
     */
    bool hasUnsavedChanges(Document* doc) const;

    /**
     * @brief Ask user to save changes
     * @param doc Document with unsaved changes
     * @return true if user chose to save or discard, false if cancelled
     */
    bool askToSaveChanges(Document* doc);

private:
    class Private;
    std::unique_ptr<Private> d;
};

} // namespace QuantilyxDoc

#endif // QUANTILYX_MAINWINDOW_H