/**
 * QuantilyxDoc - Professional Document Editor
 * Copyright (C) 2025 R² Innovative Software
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */
#include "MainWindow.h"
#include "../core/Application.h"
#include "../core/DocumentFactory.h"
#include "../core/UndoStack.h"
#include "../core/RecentFiles.h"
#include "../core/Settings.h"
#include "../core/BackupManager.h"
#include "../core/PageCache.h"
#include "DocumentView.h"
#include "PreferencesDialog.h"
#include "AboutDialog.h"
#include "CommandPalette.h"
#include "QuickActionsPanel.h"
#include "ContentsWidget.h"
#include <QMenuBar>
#include <QToolBar>
#include <QStatusBar>
#include <QDockWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QLabel>
#include <QLineEdit>
#include <QSpinBox>
#include <QSlider>
#include <QComboBox>
#include <QAction>
#include <QActionGroup>
#include <QFileDialog>
#include <QMessageBox>
#include <QProgressDialog>
#include <QProgressBar>
#include <QTimer>
#include <QDir>
#include <QStandardPaths>
#include <QMimeData>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QApplication>
#include <QClipboard>
#include <QDebug>

namespace QuantilyxDoc {

class MainWindow::Private {
public:
    Private(MainWindow* q_ptr)
        : q(q_ptr), documentView(nullptr), currentDocument(nullptr) {}

    MainWindow* q;
    DocumentView* documentView;
    QPointer<Document> currentDocument; // Use QPointer for safety

    // UI Elements
    QMenuBar* menuBar;
    QToolBar* fileToolBar;
    QToolBar* editToolBar;
    QToolBar* viewToolBar;
    QStatusBar* statusBar;
    QDockWidget* contentsDock;
    QDockWidget* thumbnailsDock;
    QDockWidget* bookmarksDock;
    QDockWidget* propertiesDock;
    QDockWidget* annotationsDock;
    QDockWidget* commandPaletteDock;
    QDockWidget* quickActionsDock;
    ContentsWidget* contentsWidget; // Add this member

    // Actions
    QAction* newAction;
    QAction* openAction;
    QAction* saveAction;
    QAction* saveAsAction;
    QAction* closeAction;
    QAction* printAction;
    QAction* exitAction;

    QAction* undoAction;
    QAction* redoAction;
    QAction* cutAction;
    QAction* copyAction;
    QAction* pasteAction;
    QAction* findAction;

    QAction* zoomInAction;
    QAction* zoomOutAction;
    QAction* fitPageAction;
    QAction* fitWidthAction;
    QAction* actualSizeAction;
    QAction* fullScreenAction;
    QAction* presentationAction;

    QAction* preferencesAction;
    QAction* aboutAction;
    QAction* aboutQtAction;
    QAction* commandPaletteAction;
    QAction* quickActionsAction;

    // Status bar widgets
    QLabel* pageLabel;
    QSpinBox* pageSpinBox;
    QLabel* pageCountLabel;
    QLabel* zoomLabel;
    QSlider* zoomSlider;
    QLabel* statusLabel;
    QProgressBar* progressBar;

    // Settings keys for window state
    static const QString windowStateSetting;
    static const QString windowGeometrySetting;

    // Helper to create menus
    void createMenus();
    // Helper to create toolbars
    void createToolBars();
    // Helper to create dock widgets
    void createDockWidgets();
    // Helper to create status bar
    void createStatusBar();
    // Helper to create actions
    void createActions();
    // Helper to update UI based on current document state
    void updateUiForDocument(Document* doc);
    // Helper to update status bar
    void updateStatusBar();
    // Helper to connect signals
    void connectSignals();
};

// Define static const strings
const QString MainWindow::Private::windowStateSetting = "MainWindow/State";
const QString MainWindow::Private::windowGeometrySetting = "MainWindow/Geometry";

void MainWindow::Private::createActions() {
    // File Actions
    newAction = new QAction(tr("&New"), q);
    newAction->setShortcuts(QKeySequence::New);
    newAction->setStatusTip(tr("Create a new document"));
    // connect(newAction, &QAction::triggered, q, &MainWindow::newDocument);

    openAction = new QAction(tr("&Open..."), q);
    openAction->setShortcuts(QKeySequence::Open);
    openAction->setStatusTip(tr("Open an existing document"));
    connect(openAction, &QAction::triggered, q, &MainWindow::openDocumentDialog);

    saveAction = new QAction(tr("&Save"), q);
    saveAction->setShortcuts(QKeySequence::Save);
    saveAction->setStatusTip(tr("Save the document"));
    connect(saveAction, &QAction::triggered, [this]() {
        if (currentDocument) {
            // currentDocument->save(); // Assuming Document has a save method
            LOG_INFO("Save action triggered for: " << currentDocument->filePath());
        }
    });

    saveAsAction = new QAction(tr("Save &As..."), q);
    saveAsAction->setShortcuts(QKeySequence::SaveAs);
    saveAsAction->setStatusTip(tr("Save the document under a new name"));
    // connect(saveAsAction, &QAction::triggered, q, &MainWindow::saveAsDocument);

    closeAction = new QAction(tr("&Close"), q);
    closeAction->setShortcuts(QKeySequence::Close);
    closeAction->setStatusTip(tr("Close the current document"));
    connect(closeAction, &QAction::triggered, [this]() {
        if (currentDocument) {
            // Application::instance().unregisterDocument(currentDocument);
            // currentDocument->close(); // Assuming Document has a close method
            updateUiForDocument(nullptr); // Clear UI
        }
    });

    printAction = new QAction(tr("&Print..."), q);
    printAction->setShortcuts(QKeySequence::Print);
    printAction->setStatusTip(tr("Print the document"));
    // connect(printAction, &QAction::triggered, q, &MainWindow::printDocument);

    exitAction = new QAction(tr("E&xit"), q);
    exitAction->setShortcuts(QKeySequence::Quit);
    exitAction->setStatusTip(tr("Exit the application"));
    connect(exitAction, &QAction::triggered, q, &QMainWindow::close);

    // Edit Actions
    undoAction = new QAction(tr("&Undo"), q);
    undoAction->setShortcuts(QKeySequence::Undo);
    undoAction->setStatusTip(tr("Undo the last action"));
    connect(undoAction, &QAction::triggered, &UndoStack::instance(), &UndoStack::undo);

    redoAction = new QAction(tr("&Redo"), q);
    redoAction->setShortcuts(QKeySequence::Redo);
    redoAction->setStatusTip(tr("Redo the last undone action"));
    connect(redoAction, &QAction::triggered, &UndoStack::instance(), &UndoStack::redo);

    cutAction = new QAction(tr("Cu&t"), q);
    cutAction->setShortcuts(QKeySequence::Cut);
    cutAction->setStatusTip(tr("Cut the selected content"));

    copyAction = new QAction(tr("&Copy"), q);
    copyAction->setShortcuts(QKeySequence::Copy);
    copyAction->setStatusTip(tr("Copy the selected content"));

    pasteAction = new QAction(tr("&Paste"), q);
    pasteAction->setShortcuts(QKeySequence::Paste);
    pasteAction->setStatusTip(tr("Paste content from clipboard"));

    findAction = new QAction(tr("&Find..."), q);
    findAction->setShortcuts(QKeySequence::Find);
    findAction->setStatusTip(tr("Find text in the document"));
    connect(findAction, &QAction::triggered, q, &MainWindow::findText);

    // View Actions
    zoomInAction = new QAction(tr("Zoom &In"), q);
    zoomInAction->setShortcuts(QKeySequence::ZoomIn);
    zoomInAction->setStatusTip(tr("Increase the zoom level"));
    connect(zoomInAction, &QAction::triggered, q, &MainWindow::zoomIn);

    zoomOutAction = new QAction(tr("Zoom &Out"), q);
    zoomOutAction->setShortcuts(QKeySequence::ZoomOut);
    zoomOutAction->setStatusTip(tr("Decrease the zoom level"));
    connect(zoomOutAction, &QAction::triggered, q, &MainWindow::zoomOut);

    fitPageAction = new QAction(tr("Fit &Page"), q);
    fitPageAction->setShortcut(QKeySequence(tr("Ctrl+0"))); // Common shortcut
    fitPageAction->setStatusTip(tr("Fit the entire page to the window"));
    connect(fitPageAction, &QAction::triggered, q, &MainWindow::fitPage);

    fitWidthAction = new QAction(tr("Fit &Width"), q);
    fitWidthAction->setShortcut(QKeySequence(tr("Ctrl+1")));
    fitWidthAction->setStatusTip(tr("Fit the page width to the window"));
    connect(fitWidthAction, &QAction::triggered, q, &MainWindow::fitWidth);

    actualSizeAction = new QAction(tr("&Actual Size"), q);
    actualSizeAction->setShortcut(QKeySequence(tr("Ctrl+2")));
    actualSizeAction->setStatusTip(tr("Show the page at its actual size (100%)"));
    connect(actualSizeAction, &QAction::triggered, q, &MainWindow::actualSize);

    fullScreenAction = new QAction(tr("&Full Screen"), q);
    fullScreenAction->setShortcuts(QKeySequence::FullScreen);
    fullScreenAction->setStatusTip(tr("Toggle full screen mode"));
    fullScreenAction->setCheckable(true);
    connect(fullScreenAction, &QAction::triggered, q, &MainWindow::toggleFullScreen);

    presentationAction = new QAction(tr("&Presentation"), q);
    presentationAction->setShortcut(QKeySequence(tr("F5")));
    presentationAction->setStatusTip(tr("Start presentation mode"));
    connect(presentationAction, &QAction::triggered, q, &MainWindow::startPresentation);

    // Settings & Help Actions
    preferencesAction = new QAction(tr("&Preferences..."), q);
    preferencesAction->setMenuRole(QAction::PreferencesRole);
    preferencesAction->setStatusTip(tr("Modify application settings"));
    connect(preferencesAction, &QAction::triggered, q, &MainWindow::showPreferences);

    aboutAction = new QAction(tr("&About QuantilyxDoc"), q);
    aboutAction->setMenuRole(QAction::AboutRole);
    aboutAction->setStatusTip(tr("Show information about QuantilyxDoc"));
    connect(aboutAction, &QAction::triggered, q, &MainWindow::showAbout);

    aboutQtAction = new QAction(tr("About &Qt"), q);
    aboutQtAction->setMenuRole(QAction::AboutQtRole);
    aboutQtAction->setStatusTip(tr("Show information about Qt"));
    connect(aboutQtAction, &QAction::triggered, q, &QApplication::aboutQt);

    commandPaletteAction = new QAction(tr("Command Palette"), q);
    commandPaletteAction->setShortcut(QKeySequence(tr("Ctrl+K")));
    commandPaletteAction->setStatusTip(tr("Open the command palette"));
    connect(commandPaletteAction, &QAction::triggered, q, &MainWindow::showCommandPalette);

    quickActionsAction = new QAction(tr("Quick Actions"), q);
    quickActionsAction->setShortcut(QKeySequence(tr("Ctrl+J")));
    quickActionsAction->setStatusTip(tr("Show quick actions panel"));
    connect(quickActionsAction, &QAction::triggered, q, &MainWindow::showQuickActions);
}

void MainWindow::Private::createMenus() {
    // File Menu
    QMenu* fileMenu = menuBar->addMenu(tr("&File"));
    fileMenu->addAction(newAction);
    fileMenu->addAction(openAction);
    fileMenu->addSeparator();
    fileMenu->addAction(saveAction);
    fileMenu->addAction(saveAsAction);
    fileMenu->addSeparator();
    fileMenu->addAction(closeAction);
    fileMenu->addSeparator();
    fileMenu->addAction(printAction);
    fileMenu->addSeparator();
    fileMenu->addAction(exitAction);

    // Edit Menu
    QMenu* editMenu = menuBar->addMenu(tr("&Edit"));
    editMenu->addAction(undoAction);
    editMenu->addAction(redoAction);
    editMenu->addSeparator();
    editMenu->addAction(cutAction);
    editMenu->addAction(copyAction);
    editMenu->addAction(pasteAction);
    editMenu->addSeparator();
    editMenu->addAction(findAction);

    // View Menu
    QMenu* viewMenu = menuBar->addMenu(tr("&View"));
    viewMenu->addAction(zoomInAction);
    viewMenu->addAction(zoomOutAction);
    viewMenu->addSeparator();
    viewMenu->addAction(fitPageAction);
    viewMenu->addAction(fitWidthAction);
    viewMenu->addAction(actualSizeAction);
    viewMenu->addSeparator();
    viewMenu->addAction(fullScreenAction);
    viewMenu->addAction(presentationAction);
    viewMenu->addSeparator();
    // Add dock widget toggle actions
    viewMenu->addAction(contentsDock->toggleViewAction());
    viewMenu->addAction(thumbnailsDock->toggleViewAction());
    viewMenu->addAction(bookmarksDock->toggleViewAction());
    viewMenu->addAction(propertiesDock->toggleViewAction());
    viewMenu->addAction(annotationsDock->toggleViewAction());
    viewMenu->addAction(commandPaletteDock->toggleViewAction());
    viewMenu->addAction(quickActionsDock->toggleViewAction());

    // Settings Menu
    QMenu* settingsMenu = menuBar->addMenu(tr("&Settings"));
    settingsMenu->addAction(preferencesAction);

    // Help Menu
    QMenu* helpMenu = menuBar->addMenu(tr("&Help"));
    helpMenu->addAction(aboutAction);
    helpMenu->addAction(aboutQtAction);
}

void MainWindow::Private::createToolBars() {
    // File Toolbar
    fileToolBar = addToolBar(tr("File"));
    fileToolBar->addAction(newAction);
    fileToolBar->addAction(openAction);
    fileToolBar->addAction(saveAction);
    fileToolBar->addAction(printAction);

    // Edit Toolbar
    editToolBar = addToolBar(tr("Edit"));
    editToolBar->addAction(undoAction);
    editToolBar->addAction(redoAction);
    editToolBar->addAction(cutAction);
    editToolBar->addAction(copyAction);
    editToolBar->addAction(pasteAction);
    editToolBar->addAction(findAction);

    // View Toolbar
    viewToolBar = addToolBar(tr("View"));
    viewToolBar->addAction(zoomInAction);
    viewToolBar->addAction(zoomOutAction);
    viewToolBar->addAction(fitPageAction);
    viewToolBar->addAction(fitWidthAction);
    viewToolBar->addAction(actualSizeAction);
    viewToolBar->addAction(fullScreenAction);
    viewToolBar->addAction(presentationAction);
}

void MainWindow::Private::createDockWidgets() {
    // Contents Dock
    contentsDock = new QDockWidget(tr("Contents"), q);
    contentsDock->setObjectName("ContentsDock");
    contentsDock->setWidget(new ContentsWidget(q)); // Assuming ContentsWidget exists
    contentsDock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    q->addDockWidget(Qt::LeftDockWidgetArea, contentsDock);

    // Thumbnails Dock
    thumbnailsDock = new QDockWidget(tr("Thumbnails"), q);
    thumbnailsDock->setObjectName("ThumbnailsDock");
    thumbnailsDock->setWidget(new ThumbnailsWidget(q)); // Assuming ThumbnailsWidget exists
    thumbnailsDock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    q->addDockWidget(Qt::LeftDockWidgetArea, thumbnailsDock);

    // Bookmarks Dock
    bookmarksDock = new QDockWidget(tr("Bookmarks"), q);
    bookmarksDock->setObjectName("BookmarksDock");
    bookmarksDock->setWidget(new BookmarksWidget(q)); // Assuming BookmarksWidget exists
    bookmarksDock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    q->addDockWidget(Qt::RightDockWidgetArea, bookmarksDock);

    // Properties Dock
    propertiesDock = new QDockWidget(tr("Properties"), q);
    propertiesDock->setObjectName("PropertiesDock");
    propertiesDock->setWidget(new PropertiesWidget(q)); // Assuming PropertiesWidget exists
    propertiesDock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    q->addDockWidget(Qt::RightDockWidgetArea, propertiesDock);

    // Annotations Dock
    annotationsDock = new QDockWidget(tr("Annotations"), q);
    annotationsDock->setObjectName("AnnotationsDock");
    annotationsDock->setWidget(new AnnotationsWidget(q)); // Assuming AnnotationsWidget exists
    annotationsDock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    q->addDockWidget(Qt::RightDockWidgetArea, annotationsDock);

    // Command Palette Dock
    commandPaletteDock = new QDockWidget(tr("Command Palette"), q);
    commandPaletteDock->setObjectName("CommandPaletteDock");
    commandPaletteDock->setWidget(new CommandPalette(q)); // Assuming CommandPalette widget exists
    commandPaletteDock->setAllowedAreas(Qt::AllDockWidgetAreas);
    commandPaletteDock->hide(); // Hidden by default
    q->addDockWidget(Qt::BottomDockWidgetArea, commandPaletteDock);

    // Quick Actions Dock
    quickActionsDock = new QDockWidget(tr("Quick Actions"), q);
    quickActionsDock->setObjectName("QuickActionsDock");
    quickActionsDock->setWidget(new QuickActionsPanel(q)); // Assuming QuickActionsPanel widget exists
    quickActionsDock->setAllowedAreas(Qt::AllDockWidgetAreas);
    quickActionsDock->hide(); // Hidden by default
    q->addDockWidget(Qt::BottomDockWidgetArea, quickActionsDock);
}

void MainWindow::Private::createStatusBar() {
    pageLabel = new QLabel(tr("Page:"));
    pageSpinBox = new QSpinBox();
    pageSpinBox->setRange(1, 1); // Will be updated by document
    pageSpinBox->setValue(1);
    pageCountLabel = new QLabel(tr("/ 1")); // Will be updated by document
    zoomLabel = new QLabel(tr("Zoom:"));
    zoomSlider = new QSlider(Qt::Horizontal);
    zoomSlider->setRange(10, 500); // 10% to 500%
    zoomSlider->setValue(100);
    statusLabel = new QLabel(tr("Ready"));
    progressBar = new QProgressBar();
    progressBar->setVisible(false); // Hidden unless an operation is in progress

    statusBar->addPermanentWidget(pageLabel);
    statusBar->addPermanentWidget(pageSpinBox);
    statusBar->addPermanentWidget(pageCountLabel);
    statusBar->addPermanentWidget(zoomLabel);
    statusBar->addPermanentWidget(zoomSlider, 1); // Stretch factor
    statusBar->addPermanentWidget(statusLabel);
    statusBar->addWidget(progressBar);

    // Connect spinbox and slider changes to document view
    connect(pageSpinBox, QOverload<int>::of(&QSpinBox::valueChanged),
            [this](int page) {
                if (documentView && currentDocument) {
                    documentView->goToPage(page - 1); // 0-based index
                }
            });
    connect(zoomSlider, &QSlider::valueChanged,
            [this](int zoom) {
                if (documentView) {
                    documentView->setZoomLevel(zoom / 100.0); // Convert to factor
                }
            });
}

void MainWindow::Private::updateUiForDocument(Document* doc) {
    currentDocument = doc; // Use QPointer

    if (doc) {
        q->setWindowTitle(doc->title() + " - QuantilyxDoc");
        pageSpinBox->setRange(1, doc->pageCount());
        pageSpinBox->setValue(doc->currentPageIndex() + 1); // 0-based to 1-based
        pageCountLabel->setText(tr("/ %1").arg(doc->pageCount()));
        statusLabel->setText(tr("Loaded: %1").arg(doc->filePath()));

        // Update ContentsWidget
        contentsWidget->setDocument(doc); // Pass the document to the ContentsWidget

        // Connect document signals to update UI
        connect(doc, &Document::currentPageChanged, q, [this](int index) {
            pageSpinBox->setValue(index + 1); // Update spinbox when page changes
        });
        // Update undo/redo actions based on UndoStack
        undoAction->setEnabled(UndoStack::instance().canUndo());
        redoAction->setEnabled(UndoStack::instance().canRedo());
    } else {
        q->setWindowTitle(tr("QuantilyxDoc"));
        pageSpinBox->setRange(1, 1);
        pageSpinBox->setValue(1);
        pageCountLabel->setText(tr("/ 1"));
        statusLabel->setText(tr("Ready"));

        // Clear ContentsWidget
        contentsWidget->setDocument(nullptr); // Clear when no document

        undoAction->setEnabled(false);
        redoAction->setEnabled(false);
    }
}

void MainWindow::Private::updateStatusBar() {
    // This can be called periodically or when document/view state changes
    if (currentDocument && documentView) {
        // Update page number if needed (though currentPageChanged signal should handle it)
        // Update zoom if needed
        // Update status text based on current operation
    }
}

void MainWindow::Private::connectSignals() {
    // Connect UndoStack signals to update UI actions
    connect(&UndoStack::instance(), &UndoStack::canUndoChanged,
            undoAction, &QAction::setEnabled);
    connect(&UndoStack::instance(), &UndoStack::canRedoChanged,
            redoAction, &QAction::setEnabled);
    // Connect RecentFiles to update menu
    // connect(&RecentFiles::instance(), &RecentFiles::recentFilesChanged, q, &MainWindow::updateRecentFilesMenu);
    connect(contentsWidget, &ContentsWidget::navigateRequested, q, [this](const QVariant& destination) {
        // The destination could be a map like {"type": "page", "page": 5} or a string
        // For now, let's handle the simple page type.
        if (destination.type() == QVariant::Map) {
            QVariantMap destMap = destination.toMap();
            QString type = destMap.value("type", "").toString();
            if (type == "page") {
                int pageIndex = destMap.value("page", -1).toInt();
                if (pageIndex >= 0 && documentView && currentDocument && pageIndex < currentDocument->pageCount()) {
                    documentView->goToPage(pageIndex);
                    LOG_INFO("Navigated to page " << pageIndex << " from ContentsWidget.");
                } else {
                    LOG_WARN("ContentsWidget destination requested invalid page: " << pageIndex);
                }
            } else {
                LOG_WARN("ContentsWidget destination type not handled: " << type);
            }
        } else {
            // Handle if destination is a raw string (e.g., Poppler's destination format)
            QString destStr = destination.toString();
            LOG_WARN("ContentsWidget destination is a raw string, not a map. Format handling needed: " << destStr);
            // This would require parsing the string format (e.g., "/Page 5 /XYZ 100 700 null")
            // and converting it to a page number and coordinates for DocumentView.
            // This is complex and depends on the PDF's internal destination format.
            // For now, we focus on the simpler map format generated by our PdfDocument::tableOfContents.
        }
    });

}

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
    , d(new Private(this))
{
    d->menuBar = menuBar();
    d->statusBar = statusBar();

    d->createActions();
    d->createMenus();
    d->createToolBars();
    d->createDockWidgets();
    d->createStatusBar();
    d->connectSignals();

    // Central widget - Document View
    d->documentView = new DocumentView(this);
    setCentralWidget(d->documentView);

    // Restore window geometry and state from settings
    Settings& settings = Settings::instance();
    restoreGeometry(settings.value< QByteArray >("MainWindow/Geometry", QByteArray()));
    restoreState(settings.value< QByteArray >("MainWindow/State", QByteArray()));

    // Update UI for no initial document
    d->updateUiForDocument(nullptr);

    setAcceptDrops(true); // Enable drag and drop
    LOG_INFO("MainWindow initialized.");
}

MainWindow::~MainWindow()
{
    // Save window geometry and state to settings
    Settings& settings = Settings::instance();
    settings.setValue("MainWindow/Geometry", saveGeometry());
    settings.setValue("MainWindow/State", saveState());
    LOG_INFO("MainWindow destroyed. State saved.");
}

bool MainWindow::openDocument(const QString& filePath)
{
    // Use DocumentFactory to create the appropriate Document instance
    Document* doc = DocumentFactory::instance().createDocument(filePath);
    if (doc) {
        // Register document with Application
        Application::instance().registerDocument(doc);
        // Set the document in the view
        d->documentView->setDocument(doc);
        // Update UI
        d->updateUiForDocument(doc);
        // Add to recent files
        RecentFiles::instance().addFile(filePath);
        LOG_INFO("Opened document: " << filePath);
        return true;
    } else {
        QMessageBox::critical(this, tr("Error"), tr("Failed to open document: %1").arg(filePath));
        LOG_ERROR("Failed to open document: " << filePath);
        return false;
    }
}

bool MainWindow::newDocument()
{
    // Placeholder for creating a new, blank document
    // This requires a specific implementation in DocumentFactory or a dedicated NewDocumentDialog
    LOG_WARN("New document functionality is not fully implemented yet.");
    QMessageBox::information(this, tr("Info"), tr("Creating new documents is not yet supported."));
    return false;
}

void MainWindow::closeEvent(QCloseEvent* event)
{
    // Check if any documents are modified and prompt to save
    // For now, assume user can close without saving
    // Save window state
    Settings& settings = Settings::instance();
    settings.setValue("MainWindow/Geometry", saveGeometry());
    settings.setValue("MainWindow/State", saveState());
    settings.save(); // Ensure settings are written
    // Unregister all documents from Application
    // Application::instance().unregisterAllDocuments(); // Assuming such a method exists or handled by Document destructor
    QMainWindow::closeEvent(event);
    LOG_INFO("MainWindow close event handled.");
}

void MainWindow::dragEnterEvent(QDragEnterEvent* event)
{
    if (event->mimeData()->hasUrls()) {
        event->acceptProposedAction();
    }
}

void MainWindow::dropEvent(QDropEvent* event)
{
    const QMimeData* mimeData = event->mimeData();
    if (mimeData->hasUrls()) {
        QList<QUrl> urlList = mimeData->urls();
        if (!urlList.isEmpty()) {
            QString filePath = urlList.first().toLocalFile();
            if (!filePath.isEmpty()) {
                openDocument(filePath);
                event->acceptProposedAction();
                return;
            }
        }
    }
    event->ignore();
}

void MainWindow::openDocumentDialog()
{
    QString filter = DocumentFactory::instance().fileDialogFilter();
    QString filePath = QFileDialog::getOpenFileName(
        this,
        tr("Open Document"),
        QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation),
        filter
    );
    if (!filePath.isEmpty()) {
        openDocument(filePath);
    }
}

void MainWindow::findText()
{
    // Placeholder for find dialog
    LOG_WARN("Find dialog is not fully implemented yet.");
    QMessageBox::information(this, tr("Info"), tr("The Find dialog is not yet implemented."));
}

void MainWindow::zoomIn()
{
    if (d->documentView) {
        d->documentView->zoomIn();
    }
}

void MainWindow::zoomOut()
{
    if (d->documentView) {
        d->documentView->zoomOut();
    }
}

void MainWindow::fitPage()
{
    if (d->documentView) {
        d->documentView->setZoomMode(DocumentView::FitPage);
    }
}

void MainWindow::fitWidth()
{
    if (d->documentView) {
        d->documentView->setZoomMode(DocumentView::FitWidth);
    }
}

void MainWindow::actualSize()
{
    if (d->documentView) {
        d->documentView->setZoomLevel(1.0);
    }
}

void MainWindow::toggleFullScreen()
{
    if (isFullScreen()) {
        showNormal();
        d->fullScreenAction->setChecked(false);
    } else {
        showFullScreen();
        d->fullScreenAction->setChecked(true);
    }
}

void MainWindow::startPresentation()
{
    // Placeholder for presentation mode
    LOG_WARN("Presentation mode is not fully implemented yet.");
    QMessageBox::information(this, tr("Info"), tr("Presentation mode is not yet implemented."));
}

void MainWindow::showPreferences()
{
    PreferencesDialog dialog(this);
    dialog.exec();
}

void MainWindow::showAbout()
{
    AboutDialog dialog(this);
    dialog.exec();
}

void MainWindow::showCommandPalette()
{
    if (d->commandPaletteDock) {
        d->commandPaletteDock->setVisible(!d->commandPaletteDock->isVisible());
    }
}

void MainWindow::showQuickActions()
{
    if (d->quickActionsDock) {
        d->quickActionsDock->setVisible(!d->quickActionsDock->isVisible());
    }
}

} // namespace QuantilyxDoc