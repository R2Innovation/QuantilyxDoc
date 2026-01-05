/**
 * QuantilyxDoc - Professional Document Editor
 * Copyright (C) 2025 R² Innovative Software
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */
#include "Application.h"
#include "Logger.h"
#include "ConfigManager.h"
#include "Settings.h"
#include "RecentFiles.h"
#include "BackupManager.h"
#include "CrashHandler.h"
#include "ProfileManager.h"
#include "MetadataDatabase.h"
#include "search/FullTextIndex.h"
#include "automation/MacroRecorder.h"
#include "automation/ScriptingEngine.h"
#include "security/PasswordRemover.h"
#include "security/RestrictionBypass.h"
#include "ocr/OcrEngine.h"
#include "ui/SplashScreen.h"
#include "ui/MainWindow.h"
#include <QDir>
#include <QStandardPaths>
#include <QFileInfo>
#include <QCommandLineParser>
#include <QTimer>
#include <QMessageBox>
#include <QDesktopServices>
#include <QUrl>
#include <QMetaType> // For registering custom types if needed
#include <QDebug>
#include <QElapsedTimer> // For timing initialization steps

namespace QuantilyxDoc {

// Static instance pointer
Application* Application::s_instance = nullptr;

Application& Application::instance()
{
    if (!s_instance) {
        qCritical("Application::instance: Application object not created yet!");
        // Should not happen if main.cpp creates Application first.
        // Maybe throw an exception or return a dummy instance? For now, assert.
        Q_ASSERT(s_instance);
    }
    return *s_instance;
}

Application::Application(int& argc, char** argv)
    : QApplication(argc, argv)
    , d(new Private(this))
{
    s_instance = this; // Set the static instance pointer early

    setApplicationName("QuantilyxDoc");
    setApplicationVersion("0.1.0-alpha"); // Placeholder version
    setOrganizationName("R² Innovative Software");
    setOrganizationDomain("r2innovative.software"); // Placeholder domain
    // setWindowIcon(QIcon(":/icons/app_icon.png")); // Load from resources

    LOG_INFO("QuantilyxDoc Application starting (Version " << applicationVersion() << ").");

    // Register custom types if any (e.g., for signals/slots across threads)
    // qRegisterMetaType<MyCustomType>("MyCustomType");

    LOG_INFO("Application object created.");
}

Application::~Application()
{
    LOG_INFO("Application object destruction started.");

    // --- Shutdown Sequence ---
    // 1. Save application state (settings, recent files, profiles, metadata DB, FTS index)
    Settings::instance().save();
    RecentFiles::instance().save();
    // ProfileManager::instance().saveCurrentProfile(); // Assuming this method exists
    // MetadataDatabase::instance().commit(); // Assuming this flushes changes to disk
    // FullTextIndex::instance().commit(); // Assuming this flushes changes to disk

    // 2. Stop background services/processes (OCR, MacroRecorder, ScriptingEngine if running scripts, etc.)
    // OcrEngine::instance().shutdown(); // Assuming a shutdown method exists
    // MacroRecorder::instance().stopRecording(); // If currently recording
    // ScriptingEngine::instance().shutdown(); // If running scripts

    // 3. Close all documents gracefully
    // QList<Document*> docsToClose = openDocuments(); // Hypothetical method to get list
    // for (Document* doc : docsToClose) {
    //     if (doc->isModified()) {
    //         // Prompt user to save/discard changes
    //         // This might require interaction with MainWindow or a headless prompt mechanism.
    //         // For now, assume save/discard is handled by MainWindow's closeEvent.
    //         // We'll just remove them from the manager, which might trigger save prompts in MainWindow.
    //         // DocumentManager::instance().closeDocument(doc); // Hypothetical manager
    //     }
    //     // Document's destructor should handle cleanup if not managed elsewhere.
    // }

    // 4. Uninstall crash handler
    CrashHandler::instance().uninstall();

    LOG_INFO("Application object destruction finished.");
}

bool Application::initialize()
{
    LOG_INFO("Starting application initialization...");

    bool initSuccess = true;
    QString initError;

    // 1. Initialize Logger first (so other initializations can log)
    if (!Logger::instance().initialize()) {
        initError = "Failed to initialize Logger.";
        initSuccess = false;
        goto finalize_init; // Jump to cleanup/error handling
    }

    // 2. Initialize ConfigManager
    if (initSuccess) {
        if (!ConfigManager::instance().initialize()) {
            initError = "Failed to initialize ConfigManager.";
            initSuccess = false;
            goto finalize_init;
        }
    }

    // 3. Initialize Settings
    if (initSuccess) {
        Settings::instance().load(); // Load settings from file
        // Any critical settings validation could happen here.
        // If a critical setting is missing or invalid, set a default or fail.
    }

    // 4. Initialize Crash Handler
    if (initSuccess) {
        if (!CrashHandler::instance().install()) {
            LOG_WARN("Could not install crash handler.");
            // Don't fail init for this, but log it.
        }
    }

    // 5. Initialize Profile Manager
    if (initSuccess) {
        if (!ProfileManager::instance().initialize()) {
            initError = "Failed to initialize ProfileManager.";
            initSuccess = false;
            goto finalize_init;
        }
    }

    // 6. Initialize Recent Files
    if (initSuccess) {
        RecentFiles::instance().load();
    }

    // 7. Initialize Backup Manager (settings dependent)
    if (initSuccess) {
        // BackupManager::instance().setEnabled(Settings::instance().value<bool>("General/EnableAutoBackup", true));
        // BackupManager::instance().setBackupDirectory(...);
        // BackupManager::instance().initializeTimers(); // If needed
    }

    // 8. Initialize Metadata Database
    if (initSuccess) {
        QString dbPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/metadata.db";
        QDir().mkpath(QFileInfo(dbPath).absolutePath()); // Ensure directory exists
        if (!MetadataDatabase::instance().initialize(dbPath)) {
            initError = "Failed to initialize MetadataDatabase.";
            initSuccess = false;
            goto finalize_init;
        }
    }

    // 9. Initialize Full-Text Index
    if (initSuccess) {
        QString indexPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/fts_index";
        QDir().mkpath(indexPath);
        if (!FullTextIndex::instance().initialize(indexPath)) {
            initError = "Failed to initialize FullTextIndex.";
            initSuccess = false;
            goto finalize_init;
        }
    }

    // 10. Initialize Password Remover (finds tools like QPDF)
    if (initSuccess) {
        // PasswordRemover::instance().findExternalTool(); // Or initialize in its constructor if it does this automatically
    }

    // 11. Initialize Restriction Bypass (finds tools like QPDF)
    if (initSuccess) {
        // RestrictionBypass::instance().findExternalTool(); // Or initialize in its constructor
    }

    // 12. Initialize OCR Engine
    if (initSuccess) {
        QString lang = Settings::instance().value<QString>("Ocr/Language", "eng");
        QString dataPath = Settings::instance().value<QString>("Ocr/TessDataPath", QString()); // Could be empty, uses default
        if (!OcrEngine::instance().initialize(lang, dataPath)) {
            // OCR is not critical for startup, warn and continue
            LOG_WARN("Failed to initialize OCR Engine. OCR features will be unavailable.");
            // initSuccess = true; // Keep initSuccess as true to allow the app to run without OCR
        }
    }

    // 13. Initialize Macro Recorder
    if (initSuccess) {
        // MacroRecorder::instance(); // Singleton created/accessed, might initialize in constructor
    }

    // 14. Initialize Scripting Engine (optional at startup)
    if (initSuccess) {
        // QString scriptLanguage = Settings::instance().value<QString>("Scripting/Language", "python");
        // if (!ScriptingEngine::instance().initialize(scriptLanguage)) {
        //     LOG_WARN("Failed to initialize Scripting Engine. Scripting features will be unavailable.");
        //     // initSuccess = true; // Keep initSuccess as true to allow the app to run without scripting
        // }
    }

finalize_init:

    if (!initSuccess) {
        LOG_CRITICAL("Application initialization failed: " << initError);
        // Could show a critical error dialog here before quitting, but QApplication isn't fully set up yet.
        // A better place might be in main() after QApplication is created but before MainWindow.
        // For now, log and return the failure status.
        // QMessageBox::critical(nullptr, "Initialization Error", initError);
        // exit(EXIT_FAILURE); // Or return false from main
    } else {
        LOG_INFO("Application initialization completed successfully.");
    }

    d->initialized = initSuccess;
    emit initializationComplete(initSuccess);
    return initSuccess;
}

bool Application::isInitialized() const
{
    return d->initialized;
}

void Application::showSplashScreen()
{
    if (!d->splashScreen) {
        d->splashScreen = new SplashScreen();
    }
    d->splashScreen->show();
    processEvents(); // Ensure splash is painted
    LOG_DEBUG("Splash screen shown.");
}

void Application::hideSplashScreen()
{
    if (d->splashScreen) {
        d->splashScreen->hide();
        LOG_DEBUG("Splash screen hidden.");
    }
}

void Application::showMainWindow()
{
    if (!d->mainWindow) {
        LOG_DEBUG("Creating MainWindow...");
        d->mainWindow = new MainWindow();
    }
    if (d->splashScreen) {
        d->splashScreen->finish(d->mainWindow); // Hide splash when main window is shown
    } else {
        d->mainWindow->show(); // Show main window directly if no splash
    }
    LOG_DEBUG("MainWindow shown.");
}

MainWindow* Application::mainWindow() const
{
    return d->mainWindow.data(); // Use QPointer
}

void Application::openFileFromCommandLine(const QString& filePath)
{
    if (d->mainWindow) {
        d->mainWindow->openDocument(filePath); // Assuming MainWindow has this method
    } else {
        LOG_WARN("Application::openFileFromCommandLine: MainWindow not ready yet.");
        // Could store the file path and open it once the main window is created.
        d->pendingFileToOpen = filePath;
    }
}

QStringList Application::commandLineFiles() const
{
    return d->filesFromCommandLine;
}

void Application::parseCommandLine()
{
    // This logic is typically in main.cpp, but could be moved here if Application owns it.
    // For now, assume main.cpp passes the file list to openFileFromCommandLine or stores it.
    // This is a simplification. A full implementation would involve QCommandLineParser.
    // For now, we'll just log that this step happens.
    LOG_DEBUG("Application: Command line parsing would happen here.");
    // Example:
    // QCommandLineParser parser;
    // parser.addPositionalArgument("files", "Files to open.", "[files...]");
    // parser.process(*this); // 'this' is QApplication
    // d->filesFromCommandLine = parser.positionalArguments();
}

void Application::handleStartupTasks()
{
    // Perform tasks that happen after the UI is ready but before full user interaction.
    // Examples:
    // - Check for updates (if enabled in settings)
    // - Load last opened documents (if enabled in settings)
    // - Initialize background services that don't require immediate UI feedback

    LOG_DEBUG("Application: Handling startup tasks...");

    // Example: Check for updates
    bool checkUpdates = Settings::instance().value<bool>("General/CheckForUpdates", true);
    if (checkUpdates) {
        QTimer::singleShot(5000, this, &Application::checkForUpdates); // Check after 5 seconds
    }

    // Example: Load last session (last opened files)
    bool loadLastSession = Settings::instance().value<bool>("General/LoadLastSession", true);
    if (loadLastSession && !d->pendingFileToOpen.isEmpty()) {
        // MainWindow would handle loading the list of files
        // mainWindow()->loadSession(RecentFiles::instance().lastSessionFiles());
        openFileFromCommandLine(d->pendingFileToOpen); // Open the first one specified on command line
    }

    LOG_DEBUG("Application: Finished startup tasks.");
}

void Application::checkForUpdates()
{
    LOG_DEBUG("Application: Checking for updates... (Stub implementation)");
    // This would involve network requests, comparing versions, prompting user, etc.
    // For now, just log.
    // Could emit a signal updateCheckFinished(bool hasUpdate, QString newVersion, QString downloadUrl);
}

} // namespace QuantilyxDoc