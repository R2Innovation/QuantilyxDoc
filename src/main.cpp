/**
 * QuantilyxDoc - Professional Document Editor
 * Copyright (C) 2025 R² Innovative Software
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * Motto: "Where innovation is the key to success"
 */

#include <QApplication>
#include <QCommandLineParser>
#include <QTranslator>
#include <QLibraryInfo>
#include <QMessageBox>
#include <QLocale>
#include <QStyleFactory>

#include "core/Application.h"
#include "core/Logger.h"
#include "core/ConfigManager.h"
#include "ui/SplashScreen.h"
#include "ui/MainWindow.h"
#include "utils/Version.h"

void setupApplication(QApplication& app);
void loadTranslations(QApplication& app, const QString& language);
void handleCommandLineArguments(const QCommandLineParser& parser);

int main(int argc, char *argv[])
{
    // Enable high DPI scaling
    QApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);
    
    QApplication app(argc, argv);
    
    // Set application information
    app.setApplicationName("QuantilyxDoc");
    app.setApplicationVersion(QUANTILYXDOC_VERSION_STRING);
    app.setOrganizationName("R² Innovative Software");
    app.setOrganizationDomain("github.com/R-Square-Innovative-Software");
    app.setApplicationDisplayName("QuantilyxDoc");
    
    // Setup application basics
    setupApplication(app);
    
    // Parse command line arguments
    QCommandLineParser parser;
    parser.setApplicationDescription(
        "QuantilyxDoc - Professional open-source document editor\n"
        "\"Where innovation is the key to success\""
    );
    parser.addHelpOption();
    parser.addVersionOption();
    
    // Add custom options
    QCommandLineOption configOption(
        QStringList() << "c" << "config",
        "Use custom configuration file",
        "config-file"
    );
    parser.addOption(configOption);
    
    QCommandLineOption profileOption(
        QStringList() << "p" << "profile",
        "Use specific profile (beginner, advanced, developer)",
        "profile"
    );
    parser.addOption(profileOption);
    
    QCommandLineOption noSplashOption(
        "no-splash",
        "Start without splash screen"
    );
    parser.addOption(noSplashOption);
    
    QCommandLineOption debugOption(
        "debug",
        "Enable debug logging"
    );
    parser.addOption(debugOption);
    
    QCommandLineOption importConfigOption(
        "import-config",
        "Import configuration from file",
        "config-file"
    );
    parser.addOption(importConfigOption);
    
    QCommandLineOption exportConfigOption(
        "export-config",
        "Export configuration to file",
        "config-file"
    );
    parser.addOption(exportConfigOption);
    
    // Add positional argument for files to open
    parser.addPositionalArgument("files", "Documents to open", "[files...]");
    
    parser.process(app);
    
    // Initialize logger
    QuantilyxDoc::Logger::instance().initialize(
        parser.isSet(debugOption) ? QuantilyxDoc::LogLevel::Debug : QuantilyxDoc::LogLevel::Info
    );
    
    LOG_INFO("QuantilyxDoc v" << QUANTILYXDOC_VERSION_STRING << " starting...");
    LOG_INFO("Developed by R² Innovative Software");
    
    // Initialize configuration
    QuantilyxDoc::ConfigManager& config = QuantilyxDoc::ConfigManager::instance();
    
    // Handle custom config file
    if (parser.isSet(configOption)) {
        QString configFile = parser.value(configOption);
        LOG_INFO("Loading custom configuration from: " << configFile);
        config.loadFromFile(configFile);
    } else {
        config.loadDefaults();
    }
    
    // Handle config import/export
    if (parser.isSet(importConfigOption)) {
        QString importFile = parser.value(importConfigOption);
        LOG_INFO("Importing configuration from: " << importFile);
        if (config.importFrom(importFile)) {
            LOG_INFO("Configuration imported successfully");
        } else {
            LOG_ERROR("Failed to import configuration");
        }
    }
    
    if (parser.isSet(exportConfigOption)) {
        QString exportFile = parser.value(exportConfigOption);
        LOG_INFO("Exporting configuration to: " << exportFile);
        if (config.exportTo(exportFile)) {
            LOG_INFO("Configuration exported successfully");
            return 0;
        } else {
            LOG_ERROR("Failed to export configuration");
            return 1;
        }
    }
    
    // Load translations
    QString language = config.getString("General", "language", "en");
    loadTranslations(app, language);
    
    // Check for single instance
    bool singleInstance = config.getBool("General", "single_instance", true);
    if (singleInstance) {
        if (QuantilyxDoc::Application::isAlreadyRunning()) {
            LOG_INFO("Application already running, sending files to existing instance");
            QStringList files = parser.positionalArguments();
            if (!files.isEmpty()) {
                QuantilyxDoc::Application::sendFilesToExistingInstance(files);
            }
            return 0;
        }
    }
    
    // Create application instance
    QuantilyxDoc::Application* quantilyxApp = QuantilyxDoc::Application::instance();
    
    // Show splash screen
    QuantilyxDoc::SplashScreen* splash = nullptr;
    bool showSplash = config.getBool("General", "show_splash", true) && 
                      !parser.isSet(noSplashOption);
    
    if (showSplash) {
        splash = new QuantilyxDoc::SplashScreen();
        splash->show();
        app.processEvents();
        
        // Simulate initialization steps with progress
        splash->showMessage("Initializing core systems...", 10);
        app.processEvents();
        QThread::msleep(300);
        
        splash->showMessage("Loading plugins...", 30);
        quantilyxApp->loadPlugins();
        app.processEvents();
        QThread::msleep(300);
        
        splash->showMessage("Initializing OCR engines...", 50);
        quantilyxApp->initializeOCR();
        app.processEvents();
        QThread::msleep(300);
        
        splash->showMessage("Loading translations...", 70);
        app.processEvents();
        QThread::msleep(300);
        
        splash->showMessage("Restoring session...", 90);
        app.processEvents();
        QThread::msleep(300);
    } else {
        // Initialize without splash
        quantilyxApp->loadPlugins();
        quantilyxApp->initializeOCR();
    }
    
    // Create main window
    QuantilyxDoc::MainWindow* mainWindow = new QuantilyxDoc::MainWindow();
    
    // Restore session
    bool restoreSession = config.getBool("General", "restore_session", true);
    if (restoreSession) {
        mainWindow->restoreSession();
    }
    
    // Open files from command line
    QStringList files = parser.positionalArguments();
    if (!files.isEmpty()) {
        for (const QString& file : files) {
            mainWindow->openDocument(file);
        }
    } else if (!restoreSession) {
        // Open blank document if configured
        bool openBlank = config.getBool("General", "open_blank_document", false);
        if (openBlank) {
            mainWindow->newDocument();
        }
    }
    
    // Show main window
    if (splash) {
        splash->showMessage("Ready!", 100);
        splash->finish(mainWindow);
        delete splash;
    }
    
    mainWindow->show();
    
    LOG_INFO("QuantilyxDoc initialized successfully");
    
    // Run application
    int result = app.exec();
    
    // Cleanup
    LOG_INFO("QuantilyxDoc shutting down...");
    delete mainWindow;
    delete quantilyxApp;
    
    LOG_INFO("Shutdown complete");
    
    return result;
}

void setupApplication(QApplication& app)
{
    // Set application style
    QString style = QuantilyxDoc::ConfigManager::instance()
        .getString("Appearance", "style", "Fusion");
    
    if (QStyleFactory::keys().contains(style)) {
        app.setStyle(QStyleFactory::create(style));
    }
    
    // Setup font
    QString fontFamily = QuantilyxDoc::ConfigManager::instance()
        .getString("Appearance", "ui_font", "Sans Serif");
    int fontSize = QuantilyxDoc::ConfigManager::instance()
        .getInt("Appearance", "ui_font_size", 10);
    
    QFont appFont(fontFamily, fontSize);
    app.setFont(appFont);
}

void loadTranslations(QApplication& app, const QString& language)
{
    LOG_INFO("Loading translations for language: " << language);
    
    // Load Qt translations
    QTranslator* qtTranslator = new QTranslator(&app);
    if (qtTranslator->load("qt_" + language, 
                           QLibraryInfo::location(QLibraryInfo::TranslationsPath))) {
        app.installTranslator(qtTranslator);
        LOG_INFO("Qt translations loaded");
    }
    
    // Load application translations
    QTranslator* appTranslator = new QTranslator(&app);
    QString translationPath = QApplication::applicationDirPath() + 
                             "/../share/quantilyxdoc/translations";
    
    if (appTranslator->load("quantilyxdoc_" + language, translationPath)) {
        app.installTranslator(appTranslator);
        LOG_INFO("Application translations loaded");
    } else {
        LOG_WARNING("Failed to load application translations for: " << language);
    }
}