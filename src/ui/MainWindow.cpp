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
#include "../core/ConfigManager.h"
#include "../core/Logger.h"
#include "../core/Document.h"
#include "DocumentView.h"

#include <QMenuBar>
#include <QToolBar>
#include <QStatusBar>
#include <QDockWidget>
#include <QTabWidget>
#include <QFileDialog>
#include <QMessageBox>
#include <QCloseEvent>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QSettings>
#include <QLabel>

namespace QuantilyxDoc {

// Private implementation
class MainWindow::Private
{
public:
    Private() 
        : tabWidget(nullptr)
        , statusLabel(nullptr)
        , currentDoc(nullptr)
    {}

    // UI Components
    QTabWidget* tabWidget;
    
    // Menus
    QMenu* fileMenu;
    QMenu* editMenu;
    QMenu* viewMenu;
    QMenu* insertMenu;
    QMenu* formatMenu;
    QMenu* toolsMenu;
    QMenu* documentMenu;
    QMenu* bookmarksMenu;
    QMenu* settingsMenu;
    QMenu* helpMenu;
    QMenu* recentFilesMenu;
    
    // Toolbars
    QToolBar* mainToolbar;
    QToolBar* editToolbar;
    QToolBar* viewToolbar;
    
    // Dock widgets
    QDockWidget* contentsDock;
    QDockWidget* thumbnailsDock;
    QDockWidget* bookmarksDock;
    QDockWidget* annotationsDock;
    QDockWidget* propertiesDock;
    
    // Status bar
    QLabel* statusLabel;
    QLabel* pageLabel;
    QLabel* zoomLabel;
    
    // Current document
    Document* currentDoc;
    
    // Recent files
    QStringList recentFiles;
    int maxRecentFiles;
};

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
    , d(new Private())
{
    LOG_INFO("Creating main window...");
    
    // Set window properties
    setWindowTitle("QuantilyxDoc");
    setWindowIcon(QIcon(":/images/QuantilyxDoc.png"));
    resize(1280, 800);
    
    // Enable drag and drop
    setAcceptDrops(true);
    
    // Create UI
    createUI();
    
    // Setup connections
    setupConnections();
    
    // Load window state
    loadWindowState();
    
    // Register with application
    Application::instance()->setMainWindow(this);
    
    LOG_INFO("Main window created");
}

MainWindow::~MainWindow()
{
    LOG_INFO("Main window destroyed");
}

bool MainWindow::openDocument(const QString& filePath)
{
    LOG_INFO("Opening document: " << filePath);
    
    if (filePath.isEmpty()) {
        QString filter = tr("All Supported Files (*.pdf *.epub *.djvu *.cbz *.cbr);;") +
                        tr("PDF Files (*.pdf);;") +
                        tr("EPUB Files (*.epub);;") +
                        tr("DjVu Files (*.djvu);;") +
                        tr("Comic Books (*.cbz *.cbr);;") +
                        tr("All Files (*)");
        
        QString file = QFileDialog::getOpenFileName(this,
                                                    tr("Open Document"),
                                                    QString(),
                                                    filter);
        if (file.isEmpty()) {
            return false;
        }
        return openDocument(file);
    }
    
    // Check if document is already open
    for (int i = 0; i < d->tabWidget->count(); ++i) {
        DocumentView* view = qobject_cast<DocumentView*>(d->tabWidget->widget(i));
        if (view && view->document() && view->document()->filePath() == filePath) {
            d->tabWidget->setCurrentIndex(i);
            LOG_INFO("Document already open, switching to tab");
            return true;
        }
    }
    
    // TODO: Create document based on file type
    // For now, just log success
    LOG_INFO("Document opened successfully: " << filePath);
    
    // Add to recent files
    ConfigManager& config = ConfigManager::instance();
    d->recentFiles.removeAll(filePath);
    d->recentFiles.prepend(filePath);
    
    int maxRecent = config.getInt("General", "recent_files_count", 20);
    while (d->recentFiles.size() > maxRecent) {
        d->recentFiles.removeLast();
    }
    
    updateRecentFilesMenu();
    updateWindowTitle();
    
    return true;
}

bool MainWindow::newDocument()
{
    LOG_INFO("Creating new document");
    
    // TODO: Implement new document creation
    
    return true;
}

bool MainWindow::closeDocument()
{
    int index = d->tabWidget->currentIndex();
    if (index < 0) {
        return false;
    }
    
    DocumentView* view = qobject_cast<DocumentView*>(d->tabWidget->widget(index));
    if (!view || !view->document()) {
        return false;
    }
    
    Document* doc = view->document();
    
    // Check for unsaved changes
    if (hasUnsavedChanges(doc)) {
        if (!askToSaveChanges(doc)) {
            return false; // User cancelled
        }
    }
    
    // Remove tab
    d->tabWidget->removeTab(index);
    
    // Delete view and document
    delete view;
    // Document will be deleted by Application
    
    LOG_INFO("Document closed");
    return true;
}

bool MainWindow::closeAllDocuments()
{
    while (d->tabWidget->count() > 0) {
        if (!closeDocument()) {
            return false; // User cancelled
        }
    }
    return true;
}

bool MainWindow::saveDocument()
{
    if (!d->currentDoc) {
        return false;
    }
    
    if (d->currentDoc->filePath().isEmpty()) {
        return saveDocumentAs();
    }
    
    LOG_INFO("Saving document: " << d->currentDoc->filePath());
    
    // TODO: Implement document saving
    
    return true;
}

bool MainWindow::saveDocumentAs()
{
    if (!d->currentDoc) {
        return false;
    }
    
    QString filter = tr("PDF Files (*.pdf);;All Files (*)");
    QString filePath = QFileDialog::getSaveFileName(this,
                                                    tr("Save Document As"),
                                                    QString(),
                                                    filter);
    
    if (filePath.isEmpty()) {
        return false;
    }
    
    LOG_INFO("Saving document as: " << filePath);
    
    // TODO: Implement document saving with new path
    
    return true;
}

Document* MainWindow::currentDocument() const
{
    return d->currentDoc;
}

DocumentView* MainWindow::currentView() const
{
    return qobject_cast<DocumentView*>(d->tabWidget->currentWidget());
}

int MainWindow::documentCount() const
{
    return d->tabWidget->count();
}

bool MainWindow::restoreSession()
{
    LOG_INFO("Restoring session...");
    
    QSettings settings;
    settings.beginGroup("Session");
    
    QStringList files = settings.value("openFiles").toStringList();
    int currentIndex = settings.value("currentIndex", 0).toInt();
    
    settings.endGroup();
    
    for (const QString& file : files) {
        openDocument(file);
    }
    
    if (currentIndex >= 0 && currentIndex < d->tabWidget->count()) {
        d->tabWidget->setCurrentIndex(currentIndex);
    }
    
    LOG_INFO("Session restored");
    return true;
}

bool MainWindow::saveSession()
{
    LOG_INFO("Saving session...");
    
    QSettings settings;
    settings.beginGroup("Session");
    
    QStringList files;
    for (int i = 0; i < d->tabWidget->count(); ++i) {
        DocumentView* view = qobject_cast<DocumentView*>(d->tabWidget->widget(i));
        if (view && view->document()) {
            files.append(view->document()->filePath());
        }
    }
    
    settings.setValue("openFiles", files);
    settings.setValue("currentIndex", d->tabWidget->currentIndex());
    
    settings.endGroup();
    
    LOG_INFO("Session saved");
    return true;
}

void MainWindow::showAboutDialog()
{
    QString aboutText = tr(
        "<h2>QuantilyxDoc</h2>"
        "<p>Version %1</p>"
        "<p><b>R² Innovative Software</b></p>"
        "<p><i>\"Where innovation is the key to success\"</i></p>"
        "<p>Professional document editor for Linux</p>"
        "<p>Licensed under GPLv3</p>"
        "<p><a href='https://github.com/R-Square-Innovative-Software'>GitHub</a></p>"
    ).arg(Application::version());
    
    QMessageBox::about(this, tr("About QuantilyxDoc"), aboutText);
}

void MainWindow::showPreferences()
{
    LOG_INFO("Showing preferences dialog");
    
    // TODO: Implement preferences dialog
    
    QMessageBox::information(this, tr("Preferences"), 
                            tr("Preferences dialog will be implemented soon."));
}

void MainWindow::toggleFullscreen()
{
    if (isFullScreen()) {
        showNormal();
    } else {
        showFullScreen();
    }
}

void MainWindow::toggleSidebar()
{
    bool visible = d->contentsDock->isVisible();
    d->contentsDock->setVisible(!visible);
    d->thumbnailsDock->setVisible(!visible);
    d->bookmarksDock->setVisible(!visible);
}

void MainWindow::togglePropertiesPanel()
{
    bool visible = d->propertiesDock->isVisible();
    d->propertiesDock->setVisible(!visible);
}

void MainWindow::switchToDocument(int index)
{
    if (index >= 0 && index < d->tabWidget->count()) {
        d->tabWidget->setCurrentIndex(index);
    }
}

void MainWindow::closeEvent(QCloseEvent* event)
{
    LOG_INFO("Main window close event");
    
    // Check for unsaved changes
    for (int i = 0; i < d->tabWidget->count(); ++i) {
        DocumentView* view = qobject_cast<DocumentView*>(d->tabWidget->widget(i));
        if (view && view->document() && hasUnsavedChanges(view->document())) {
            d->tabWidget->setCurrentIndex(i);
            if (!askToSaveChanges(view->document())) {
                event->ignore();
                return;
            }
        }
    }
    
    // Save session if configured
    ConfigManager& config = ConfigManager::instance();
    bool saveSessionOnExit = config.getBool("Workspace", "save_workspace_on_exit", true);
    
    if (saveSessionOnExit) {
        saveSession();
    }
    
    // Save window state
    saveWindowState();
    
    event->accept();
    
    LOG_INFO("Main window closed");
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
        QList<QUrl> urls = mimeData->urls();
        for (const QUrl& url : urls) {
            if (url.isLocalFile()) {
                openDocument(url.toLocalFile());
            }
        }
        event->acceptProposedAction();
    }
}

void MainWindow::onDocumentModified()
{
    updateWindowTitle();
    updateUIState();
}

void MainWindow::onTabCloseRequested(int index)
{
    d->tabWidget->setCurrentIndex(index);
    closeDocument();
}

void MainWindow::onCurrentTabChanged(int index)
{
    DocumentView* view = qobject_cast<DocumentView*>(d->tabWidget->widget(index));
    d->currentDoc = view ? view->document() : nullptr;
    
    updateWindowTitle();
    updateUIState();
}

void MainWindow::updateRecentFilesMenu()
{
    d->recentFilesMenu->clear();
    
    for (const QString& file : d->recentFiles) {
        QAction* action = d->recentFilesMenu->addAction(QFileInfo(file).fileName());
        action->setData(file);
        action->setToolTip(file);
        
        connect(action, &QAction::triggered, this, [this, file]() {
            openDocument(file);
        });
    }
    
    if (d->recentFiles.isEmpty()) {
        QAction* action = d->recentFilesMenu->addAction(tr("No recent files"));
        action->setEnabled(false);
    }
}

void MainWindow::updateWindowTitle()
{
    QString title = "QuantilyxDoc";
    
    if (d->currentDoc) {
        QString fileName = QFileInfo(d->currentDoc->filePath()).fileName();
        if (fileName.isEmpty()) {
            fileName = tr("Untitled");
        }
        
        if (hasUnsavedChanges(d->currentDoc)) {
            fileName += " *";
        }
        
        title = QString("%1 - %2").arg(fileName, title);
    }
    
    setWindowTitle(title);
}

void MainWindow::updateUIState()
{
    bool hasDoc = (d->currentDoc != nullptr);
    
    // TODO: Enable/disable actions based on document state
}

// UI Creation Methods

void MainWindow::createUI()
{
    createCentralWidget();
    createMenuBar();
    createToolbars();
    createDockWidgets();
    createStatusBar();
}

void MainWindow::createMenuBar()
{
    QMenuBar* menuBar = this->menuBar();
    
    // File Menu
    d->fileMenu = menuBar->addMenu(tr("&File"));
    
    QAction* newAction = d->fileMenu->addAction(tr("&New"));
    newAction->setShortcut(QKeySequence::New);
    connect(newAction, &QAction::triggered, this, &MainWindow::newDocument);
    
    QAction* openAction = d->fileMenu->addAction(tr("&Open..."));
    openAction->setShortcut(QKeySequence::Open);
    connect(openAction, &QAction::triggered, this, [this]() { openDocument(QString()); });
    
    d->recentFilesMenu = d->fileMenu->addMenu(tr("Open &Recent"));
    updateRecentFilesMenu();
    
    d->fileMenu->addSeparator();
    
    QAction* saveAction = d->fileMenu->addAction(tr("&Save"));
    saveAction->setShortcut(QKeySequence::Save);
    connect(saveAction, &QAction::triggered, this, &MainWindow::saveDocument);
    
    QAction* saveAsAction = d->fileMenu->addAction(tr("Save &As..."));
    saveAsAction->setShortcut(QKeySequence::SaveAs);
    connect(saveAsAction, &QAction::triggered, this, &MainWindow::saveDocumentAs);
    
    d->fileMenu->addSeparator();
    
    QAction* closeAction = d->fileMenu->addAction(tr("&Close"));
    closeAction->setShortcut(QKeySequence::Close);
    connect(closeAction, &QAction::triggered, this, &MainWindow::closeDocument);
    
    QAction* quitAction = d->fileMenu->addAction(tr("&Quit"));
    quitAction->setShortcut(QKeySequence::Quit);
    connect(quitAction, &QAction::triggered, this, &QWidget::close);
    
    // Edit Menu
    d->editMenu = menuBar->addMenu(tr("&Edit"));
    d->editMenu->addAction(tr("&Undo"))->setShortcut(QKeySequence::Undo);
    d->editMenu->addAction(tr("&Redo"))->setShortcut(QKeySequence::Redo);
    d->editMenu->addSeparator();
    d->editMenu->addAction(tr("Cu&t"))->setShortcut(QKeySequence::Cut);
    d->editMenu->addAction(tr("&Copy"))->setShortcut(QKeySequence::Copy);
    d->editMenu->addAction(tr("&Paste"))->setShortcut(QKeySequence::Paste);
    d->editMenu->addSeparator();
    d->editMenu->addAction(tr("&Find..."))->setShortcut(QKeySequence::Find);
    d->editMenu->addAction(tr("Find &Next"))->setShortcut(QKeySequence::FindNext);
    
    // View Menu
    d->viewMenu = menuBar->addMenu(tr("&View"));
    
    QAction* fullscreenAction = d->viewMenu->addAction(tr("&Fullscreen"));
    fullscreenAction->setShortcut(QKeySequence("F11"));
    fullscreenAction->setCheckable(true);
    connect(fullscreenAction, &QAction::triggered, this, &MainWindow::toggleFullscreen);
    
    d->viewMenu->addSeparator();
    
    QAction* sidebarAction = d->viewMenu->addAction(tr("Show &Sidebar"));
    sidebarAction->setCheckable(true);
    sidebarAction->setChecked(true);
    connect(sidebarAction, &QAction::triggered, this, &MainWindow::toggleSidebar);
    
    QAction* propertiesAction = d->viewMenu->addAction(tr("Show &Properties"));
    propertiesAction->setCheckable(true);
    propertiesAction->setChecked(true);
    connect(propertiesAction, &QAction::triggered, this, &MainWindow::togglePropertiesPanel);
    
    // Insert Menu
    d->insertMenu = menuBar->addMenu(tr("&Insert"));
    
    // Format Menu
    d->formatMenu = menuBar->addMenu(tr("F&ormat"));
    
    // Tools Menu
    d->toolsMenu = menuBar->addMenu(tr("&Tools"));
    
    // Document Menu
    d->documentMenu = menuBar->addMenu(tr("&Document"));
    
    // Bookmarks Menu
    d->bookmarksMenu = menuBar->addMenu(tr("&Bookmarks"));
    
    // Settings Menu
    d->settingsMenu = menuBar->addMenu(tr("&Settings"));
    
    QAction* preferencesAction = d->settingsMenu->addAction(tr("&Preferences..."));
    preferencesAction->setShortcut(QKeySequence::Preferences);
    connect(preferencesAction, &QAction::triggered, this, &MainWindow::showPreferences);
    
    // Help Menu
    d->helpMenu = menuBar->addMenu(tr("&Help"));
    
    d->helpMenu->addAction(tr("&User Manual"))->setShortcut(QKeySequence::HelpContents);
    d->helpMenu->addAction(tr("&Keyboard Shortcuts"));
    d->helpMenu->addSeparator();
    
    QAction* aboutAction = d->helpMenu->addAction(tr("&About QuantilyxDoc"));
    connect(aboutAction, &QAction::triggered, this, &MainWindow::showAboutDialog);
    
    d->helpMenu->addAction(tr("About &Qt"))->connect(d->helpMenu, &QMenu::triggered, 
                                                     qApp, &QApplication::aboutQt);
}

void MainWindow::createToolbars()
{
    // Main Toolbar
    d->mainToolbar = addToolBar(tr("Main Toolbar"));
    d->mainToolbar->setObjectName("MainToolbar");
    d->mainToolbar->setMovable(true);
    
    d->mainToolbar->addAction(QIcon::fromTheme("document-new"), tr("New"));
    d->mainToolbar->addAction(QIcon::fromTheme("document-open"), tr("Open"));
    d->mainToolbar->addAction(QIcon::fromTheme("document-save"), tr("Save"));
    d->mainToolbar->addSeparator();
    d->mainToolbar->addAction(QIcon::fromTheme("edit-undo"), tr("Undo"));
    d->mainToolbar->addAction(QIcon::fromTheme("edit-redo"), tr("Redo"));
    d->mainToolbar->addSeparator();
    d->mainToolbar->addAction(QIcon::fromTheme("zoom-in"), tr("Zoom In"));
    d->mainToolbar->addAction(QIcon::fromTheme("zoom-out"), tr("Zoom Out"));
    d->mainToolbar->addAction(QIcon::fromTheme("zoom-fit-best"), tr("Fit Width"));
    
    // Edit Toolbar
    d->editToolbar = addToolBar(tr("Edit Toolbar"));
    d->editToolbar->setObjectName("EditToolbar");
    
    // View Toolbar
    d->viewToolbar = addToolBar(tr("View Toolbar"));
    d->viewToolbar->setObjectName("ViewToolbar");
}

void MainWindow::createStatusBar()
{
    QStatusBar* status = statusBar();
    
    // Status label (left side)
    d->statusLabel = new QLabel(tr("Ready"));
    status->addWidget(d->statusLabel, 1);
    
    // Page label
    d->pageLabel = new QLabel(tr("Page 1 of 1"));
    status->addPermanentWidget(d->pageLabel);
    
    // Zoom label
    d->zoomLabel = new QLabel(tr("100%"));
    status->addPermanentWidget(d->zoomLabel);
}

void MainWindow::createDockWidgets()
{
    // Contents Dock (left)
    d->contentsDock = new QDockWidget(tr("Contents"), this);
    d->contentsDock->setObjectName("ContentsDock");
    d->contentsDock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    addDockWidget(Qt::LeftDockWidgetArea, d->contentsDock);
    
    // Thumbnails Dock (left)
    d->thumbnailsDock = new QDockWidget(tr("Thumbnails"), this);
    d->thumbnailsDock->setObjectName("ThumbnailsDock");
    d->thumbnailsDock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    addDockWidget(Qt::LeftDockWidgetArea, d->thumbnailsDock);
    
    // Bookmarks Dock (left)
    d->bookmarksDock = new QDockWidget(tr("Bookmarks"), this);
    d->bookmarksDock->setObjectName("BookmarksDock");
    d->bookmarksDock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    addDockWidget(Qt::LeftDockWidgetArea, d->bookmarksDock);
    
    // Tabify left docks
    tabifyDockWidget(d->contentsDock, d->thumbnailsDock);
    tabifyDockWidget(d->thumbnailsDock, d->bookmarksDock);
    d->contentsDock->raise(); // Show contents by default
    
    // Annotations Dock (left)
    d->annotationsDock = new QDockWidget(tr("Annotations"), this);
    d->annotationsDock->setObjectName("AnnotationsDock");
    d->annotationsDock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    addDockWidget(Qt::LeftDockWidgetArea, d->annotationsDock);
    
    // Properties Dock (right)
    d->propertiesDock = new QDockWidget(tr("Properties"), this);
    d->propertiesDock->setObjectName("PropertiesDock");
    d->propertiesDock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    addDockWidget(Qt::RightDockWidgetArea, d->propertiesDock);
}

void MainWindow::createCentralWidget()
{
    d->tabWidget = new QTabWidget(this);
    d->tabWidget->setTabsClosable(true);
    d->tabWidget->setMovable(true);
    d->tabWidget->setDocumentMode(true);
    
    setCentralWidget(d->tabWidget);
    
    connect(d->tabWidget, &QTabWidget::tabCloseRequested, 
            this, &MainWindow::onTabCloseRequested);
    connect(d->tabWidget, &QTabWidget::currentChanged, 
            this, &MainWindow::onCurrentTabChanged);
}

void MainWindow::setupConnections()
{
    // TODO: Setup additional connections
}

void MainWindow::loadWindowState()
{
    QSettings settings;
    settings.beginGroup("MainWindow");
    
    // Restore geometry
    restoreGeometry(settings.value("geometry").toByteArray());
    
    // Restore window state (dock positions, toolbar positions)
    restoreState(settings.value("windowState").toByteArray());
    
    settings.endGroup();
    
    LOG_INFO("Window state loaded");
}

void MainWindow::saveWindowState()
{
    QSettings settings;
    settings.beginGroup("MainWindow");
    
    // Save geometry
    settings.setValue("geometry", saveGeometry());
    
    // Save window state
    settings.setValue("windowState", saveState());
    
    settings.endGroup();
    
    LOG_INFO("Window state saved");
}

bool MainWindow::hasUnsavedChanges(Document* doc) const
{
    if (!doc) {
        return false;
    }
    
    // TODO: Check if document has been modified
    return false;
}

bool MainWindow::askToSaveChanges(Document* doc)
{
    if (!doc) {
        return true;
    }
    
    QString fileName = QFileInfo(doc->filePath()).fileName();
    if (fileName.isEmpty()) {
        fileName = tr("Untitled");
    }
    
    QMessageBox::StandardButton result = QMessageBox::question(
        this,
        tr("Save Changes?"),
        tr("The document \"%1\" has been modified.\n"
           "Do you want to save your changes?").arg(fileName),
        QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel,
        QMessageBox::Save
    );
    
    switch (result) {
        case QMessageBox::Save:
            return saveDocument();
        case QMessageBox::Discard:
            return true;
        case QMessageBox::Cancel:
        default:
            return false;
    }
}

} // namespace QuantilyxDoc