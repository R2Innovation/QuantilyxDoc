/**
 * QuantilyxDoc - Professional Document Editor
 * Copyright (C) 2025 R² Innovative Software
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */
#include "PreferencesDialog.h"
#include "../core/Settings.h"
#include "../core/Logger.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTabWidget>
#include <QDialogButtonBox>
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
#include <QMessageBox>
#include <QDir>
#include <QStandardPaths>
#include <QDebug>

namespace QuantilyxDoc {

class PreferencesDialog::Private {
public:
    Private(PreferencesDialog* q_ptr) : q(q_ptr) {}

    PreferencesDialog* q;

    // Main UI elements
    QTabWidget* tabWidget;
    QDialogButtonBox* buttonBox;

    // Display Page widgets
    QCheckBox* highDpiCheckBox;
    QCheckBox* smoothScrollingCheckBox;
    QCheckBox* showPageShadowCheckBox;
    QSpinBox* pageSpacingSpinBox;
    QSpinBox* pageMarginSpinBox;
    QLabel* backgroundColorLabel;
    QPushButton* backgroundColorButton;
    QColor backgroundColorValue;
    QCheckBox* useCustomBackgroundColorCheckBox;

    // Editor Page widgets
    QCheckBox* autoIndentCheckBox;
    QSpinBox* tabWidthSpinBox;
    QCheckBox* showWhitespaceCheckBox;
    QCheckBox* autoSaveCheckBox;
    QSpinBox* autoSaveIntervalSpinBox;
    QCheckBox* spellCheckCheckBox;
    QCheckBox* liveSpellCheckCheckBox;

    // Security Page widgets
    QCheckBox* checkForUpdatesCheckBox;
    QCheckBox* sendUsageStatsCheckBox; // Should default to OFF for privacy
    QCheckBox* enableCrashReportingCheckBox;
    QCheckBox* warnOnRestrictionsCheckBox;
    QCheckBox* autoRemovePasswordsCheckBox; // Feature specific to QuantilyxDoc's philosophy

    // Advanced Page widgets
    QSpinBox* maxConcurrentRendersSpinBox;
    QSpinBox* maxConcurrentLoadsSpinBox;
    QSpinBox* pageCacheSizeSpinBox; // In MB
    QSpinBox* backupIntervalSpinBox; // In seconds
    QCheckBox* enableLazyLoadingCheckBox;
    QCheckBox* enableProgressiveRenderingCheckBox;

    // Profiles Page widgets
    QListWidget* profilesListWidget;
    QPushButton* newProfileButton;
    QPushButton* deleteProfileButton;
    QPushButton* renameProfileButton;
    QPushButton* importProfileButton;
    QPushButton* exportProfileButton;
    QPushButton* setDefaultProfileButton;

    // Temporary storage for values that might be applied later
    QColor tempBackgroundColor;
    QFont tempEditorFont;

    // Helper to create a settings group box
    QGroupBox* createGroupBox(const QString& title, QWidget* contentWidget);
};

QGroupBox* PreferencesDialog::Private::createGroupBox(const QString& title, QWidget* contentWidget)
{
    QGroupBox* groupBox = new QGroupBox(title);
    QVBoxLayout* layout = new QVBoxLayout(groupBox);
    layout->addWidget(contentWidget);
    return groupBox;
}

PreferencesDialog::PreferencesDialog(QWidget* parent)
    : QDialog(parent)
    , d(new Private(this))
{
    setWindowTitle(tr("Preferences"));
    setModal(true);
    resize(600, 500);

    // Main layout
    QVBoxLayout* mainLayout = new QVBoxLayout(this);

    // Tab widget for different preference categories
    d->tabWidget = new QTabWidget(this);

    // Add pages
    d->tabWidget->addTab(createDisplayPage(), tr("Display"));
    d->tabWidget->addTab(createEditorPage(), tr("Editor"));
    d->tabWidget->addTab(createSecurityPage(), tr("Security"));
    d->tabWidget->addTab(createAdvancedPage(), tr("Advanced"));
    d->tabWidget->addTab(createProfilesPage(), tr("Profiles"));

    mainLayout->addWidget(d->tabWidget);

    // Button box
    d->buttonBox = new QDialogButtonBox(this);
    d->buttonBox->setOrientation(Qt::Horizontal);
    d->buttonBox->setStandardButtons(QDialogButtonBox::Ok | QDialogButtonBox::Apply | QDialogButtonBox::Cancel | QDialogButtonBox::RestoreDefaults);
    mainLayout->addWidget(d->buttonBox);

    // Connect signals
    connect(d->buttonBox, &QDialogButtonBox::accepted, this, &PreferencesDialog::accept);
    connect(d->buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(d->buttonBox->button(QDialogButtonBox::Apply), &QPushButton::clicked, this, &PreferencesDialog::applyChanges);
    connect(d->buttonBox->button(QDialogButtonBox::RestoreDefaults), &QPushButton::clicked, [this]() {
        // Reset settings to defaults registered in Settings class
        Settings::instance().resetAllToDefaults();
        loadSettings(); // Reload UI from defaults
    });

    connect(d->tabWidget, &QTabWidget::currentChanged, this, &PreferencesDialog::onTabChanged);

    // Load initial settings into UI
    loadSettings();

    LOG_INFO("PreferencesDialog initialized.");
}

PreferencesDialog::~PreferencesDialog()
{
    LOG_INFO("PreferencesDialog destroyed.");
}

void PreferencesDialog::accept()
{
    validateSettings();
    saveSettings();
    QDialog::accept();
}

void PreferencesDialog::reject()
{
    // On reject, do not save changes. The Settings object holds the original values.
    // If temporary values were modified (e.g., colors/fonts in UI), they are discarded.
    QDialog::reject();
}

void PreferencesDialog::applyChanges()
{
    validateSettings();
    saveSettings();
    // Keep dialog open
}

void PreferencesDialog::onTabChanged(int index)
{
    Q_UNUSED(index);
    // Could be used to load specific data for a tab only when it's shown
    LOG_DEBUG("Preferences dialog tab changed to index: " << index);
}

void PreferencesDialog::openColorDialog()
{
    QColor newColor = QColorDialog::getColor(d->tempBackgroundColor, this, tr("Choose Background Color"));
    if (newColor.isValid()) {
        d->tempBackgroundColor = newColor;
        // Update button appearance or label
        QPixmap pixmap(16, 16);
        pixmap.fill(newColor);
        d->backgroundColorButton->setIcon(pixmap);
    }
}

void PreferencesDialog::openFontDialog()
{
    bool ok;
    QFont newFont = QFontDialog::getFont(&ok, d->tempEditorFont, this, tr("Choose Editor Font"));
    if (ok) {
        d->tempEditorFont = newFont;
        // Update label or example text
        LOG_DEBUG("Selected font: " << newFont.family() << ", " << newFont.pointSize());
    }
}

QWidget* PreferencesDialog::createDisplayPage()
{
    QWidget* page = new QWidget();
    QVBoxLayout* layout = new QVBoxLayout(page);

    // High DPI
    d->highDpiCheckBox = new QCheckBox(tr("Use High DPI Pixmaps"), page);
    layout->addWidget(d->highDpiCheckBox);

    // Smooth Scrolling
    d->smoothScrollingCheckBox = new QCheckBox(tr("Enable Smooth Scrolling"), page);
    layout->addWidget(d->smoothScrollingCheckBox);

    // Show Page Shadow
    d->showPageShadowCheckBox = new QCheckBox(tr("Show Page Shadow"), page);
    layout->addWidget(d->showPageShadowCheckBox);

    // Page Spacing
    QHBoxLayout* spacingLayout = new QHBoxLayout();
    spacingLayout->addWidget(new QLabel(tr("Page Spacing (px):")));
    d->pageSpacingSpinBox = new QSpinBox(page);
    d->pageSpacingSpinBox->setRange(0, 50);
    spacingLayout->addWidget(d->pageSpacingSpinBox);
    layout->addLayout(spacingLayout);

    // Page Margin
    QHBoxLayout* marginLayout = new QHBoxLayout();
    marginLayout->addWidget(new QLabel(tr("Page Margin (px):")));
    d->pageMarginSpinBox = new QSpinBox(page);
    d->pageMarginSpinBox->setRange(0, 50);
    marginLayout->addWidget(d->pageMarginSpinBox);
    layout->addLayout(marginLayout);

    // Background Color
    QHBoxLayout* bgColorLayout = new QHBoxLayout();
    d->useCustomBackgroundColorCheckBox = new QCheckBox(tr("Use Custom Background Color"), page);
    bgColorLayout->addWidget(d->useCustomBackgroundColorCheckBox);
    bgColorLayout->addStretch(); // Push button to the right
    d->backgroundColorButton = new QPushButton(tr("..."), page);
    d->backgroundColorButton->setFixedSize(30, 20);
    bgColorLayout->addWidget(d->backgroundColorButton);
    layout->addLayout(bgColorLayout);

    connect(d->backgroundColorButton, &QPushButton::clicked, this, &PreferencesDialog::openColorDialog);

    layout->addStretch(); // Push everything up

    return page;
}

QWidget* PreferencesDialog::createEditorPage()
{
    QWidget* page = new QWidget();
    QVBoxLayout* layout = new QVBoxLayout(page);

    // Auto Indent
    d->autoIndentCheckBox = new QCheckBox(tr("Enable Auto Indent"), page);
    layout->addWidget(d->autoIndentCheckBox);

    // Tab Width
    QHBoxLayout* tabWidthLayout = new QHBoxLayout();
    tabWidthLayout->addWidget(new QLabel(tr("Tab Width:")));
    d->tabWidthSpinBox = new QSpinBox(page);
    d->tabWidthSpinBox->setRange(1, 16);
    tabWidthLayout->addWidget(d->tabWidthSpinBox);
    layout->addLayout(tabWidthLayout);

    // Show Whitespace
    d->showWhitespaceCheckBox = new QCheckBox(tr("Show Whitespace Characters"), page);
    layout->addWidget(d->showWhitespaceCheckBox);

    // Auto Save
    d->autoSaveCheckBox = new QCheckBox(tr("Enable Auto Save"), page);
    layout->addWidget(d->autoSaveCheckBox);

    // Auto Save Interval
    QHBoxLayout* autoSaveLayout = new QHBoxLayout();
    autoSaveLayout->addWidget(new QLabel(tr("Auto Save Interval (seconds):")));
    d->autoSaveIntervalSpinBox = new QSpinBox(page);
    d->autoSaveIntervalSpinBox->setRange(30, 3600); // 30 seconds to 1 hour
    autoSaveLayout->addWidget(d->autoSaveIntervalSpinBox);
    layout->addLayout(autoSaveLayout);

    // Spell Check
    d->spellCheckCheckBox = new QCheckBox(tr("Enable Spell Check"), page);
    layout->addWidget(d->spellCheckCheckBox);

    // Live Spell Check
    d->liveSpellCheckCheckBox = new QCheckBox(tr("Live Spell Check (check while typing)"), page);
    layout->addWidget(d->liveSpellCheckCheckBox);

    layout->addStretch(); // Push everything up

    return page;
}

QWidget* PreferencesDialog::createSecurityPage()
{
    QWidget* page = new QWidget();
    QVBoxLayout* layout = new QVBoxLayout(page);

    // Check for Updates
    d->checkForUpdatesCheckBox = new QCheckBox(tr("Automatically check for updates"), page);
    layout->addWidget(d->checkForUpdatesCheckBox);

    // Send Usage Stats (OFF by default for privacy)
    d->sendUsageStatsCheckBox = new QCheckBox(tr("Send anonymous usage statistics"), page);
    d->sendUsageStatsCheckBox->setChecked(false); // Default OFF
    layout->addWidget(d->sendUsageStatsCheckBox);

    // Enable Crash Reporting
    d->enableCrashReportingCheckBox = new QCheckBox(tr("Enable crash reporting (helps improve the app)"), page);
    layout->addWidget(d->enableCrashReportingCheckBox);

    // Warn on Restrictions
    d->warnOnRestrictionsCheckBox = new QCheckBox(tr("Warn when opening documents with restrictions"), page);
    layout->addWidget(d->warnOnRestrictionsCheckBox);

    // Auto Remove Passwords (QuantilyxDoc specific)
    d->autoRemovePasswordsCheckBox = new QCheckBox(tr("Auto-remove passwords from documents (Liberation Feature)"), page);
    layout->addWidget(d->autoRemovePasswordsCheckBox);

    layout->addStretch(); // Push everything up

    return page;
}

QWidget* PreferencesDialog::createAdvancedPage()
{
    QWidget* page = new QWidget();
    QVBoxLayout* layout = new QVBoxLayout(page);

    // Max Concurrent Renders
    QHBoxLayout* renderLayout = new QHBoxLayout();
    renderLayout->addWidget(new QLabel(tr("Max Concurrent Page Renders:")));
    d->maxConcurrentRendersSpinBox = new QSpinBox(page);
    d->maxConcurrentRendersSpinBox->setRange(1, 16);
    renderLayout->addWidget(d->maxConcurrentRendersSpinBox);
    layout->addLayout(renderLayout);

    // Max Concurrent Loads
    QHBoxLayout* loadLayout = new QHBoxLayout();
    loadLayout->addWidget(new QLabel(tr("Max Concurrent Resource Loads:")));
    d->maxConcurrentLoadsSpinBox = new QSpinBox(page);
    d->maxConcurrentLoadsSpinBox->setRange(1, 16);
    loadLayout->addWidget(d->maxConcurrentLoadsSpinBox);
    layout->addLayout(loadLayout);

    // Page Cache Size
    QHBoxLayout* cacheLayout = new QHBoxLayout();
    cacheLayout->addWidget(new QLabel(tr("Page Cache Size (MB):")));
    d->pageCacheSizeSpinBox = new QSpinBox(page);
    d->pageCacheSizeSpinBox->setRange(10, 1000); // 10MB to 1GB
    cacheLayout->addWidget(d->pageCacheSizeSpinBox);
    layout->addLayout(cacheLayout);

    // Backup Interval
    QHBoxLayout* backupLayout = new QHBoxLayout();
    backupLayout->addWidget(new QLabel(tr("Backup Interval (seconds):")));
    d->backupIntervalSpinBox = new QSpinBox(page);
    d->backupIntervalSpinBox->setRange(60, 3600); // 1 minute to 1 hour
    backupLayout->addWidget(d->backupIntervalSpinBox);
    layout->addLayout(backupLayout);

    // Enable Lazy Loading
    d->enableLazyLoadingCheckBox = new QCheckBox(tr("Enable Lazy Loading for Large Documents"), page);
    layout->addWidget(d->enableLazyLoadingCheckBox);

    // Enable Progressive Rendering
    d->enableProgressiveRenderingCheckBox = new QCheckBox(tr("Enable Progressive Rendering"), page);
    layout->addWidget(d->enableProgressiveRenderingCheckBox);

    layout->addStretch(); // Push everything up

    return page;
}

QWidget* PreferencesDialog::createProfilesPage()
{
    QWidget* page = new QWidget();
    QVBoxLayout* layout = new QVBoxLayout(page);

    // Profile list
    d->profilesListWidget = new QListWidget(page);
    layout->addWidget(new QLabel(tr("Profiles:")));
    layout->addWidget(d->profilesListWidget);

    // Profile management buttons
    QHBoxLayout* buttonLayout = new QHBoxLayout();
    d->newProfileButton = new QPushButton(tr("New"), page);
    d->deleteProfileButton = new QPushButton(tr("Delete"), page);
    d->renameProfileButton = new QPushButton(tr("Rename"), page);
    d->importProfileButton = new QPushButton(tr("Import"), page);
    d->exportProfileButton = new QPushButton(tr("Export"), page);
    d->setDefaultProfileButton = new QPushButton(tr("Set as Default"), page);

    buttonLayout->addWidget(d->newProfileButton);
    buttonLayout->addWidget(d->deleteProfileButton);
    buttonLayout->addWidget(d->renameProfileButton);
    buttonLayout->addWidget(d->importProfileButton);
    buttonLayout->addWidget(d->exportProfileButton);
    buttonLayout->addWidget(d->setDefaultProfileButton);
    buttonLayout->addStretch(); // Push buttons to the left

    layout->addLayout(buttonLayout);

    layout->addStretch(); // Push list and buttons up

    return page;
}

void PreferencesDialog::loadSettings()
{
    Settings& settings = Settings::instance();

    // Display
    d->highDpiCheckBox->setChecked(settings.value<bool>("Display/UseHighDpiPixmaps", true));
    d->smoothScrollingCheckBox->setChecked(settings.value<bool>("Display/SmoothScrolling", true));
    d->showPageShadowCheckBox->setChecked(settings.value<bool>("Display/ShowPageShadow", true));
    d->pageSpacingSpinBox->setValue(settings.value<int>("Display/PageSpacing", 10));
    d->pageMarginSpinBox->setValue(settings.value<int>("Display/PageMargin", 5));
    d->useCustomBackgroundColorCheckBox->setChecked(settings.value<bool>("Display/UseCustomBackgroundColor", false));
    d->backgroundColorValue = settings.value<QColor>("Display/BackgroundColor", Qt::white);
    d->tempBackgroundColor = d->backgroundColorValue; // Initialize temp value
    QPixmap pixmap(16, 16);
    pixmap.fill(d->backgroundColorValue);
    d->backgroundColorButton->setIcon(pixmap);

    // Editor
    d->autoIndentCheckBox->setChecked(settings.value<bool>("Editor/AutoIndent", true));
    d->tabWidthSpinBox->setValue(settings.value<int>("Editor/TabWidth", 4));
    d->showWhitespaceCheckBox->setChecked(settings.value<bool>("Editor/ShowWhitespace", false));
    d->autoSaveCheckBox->setChecked(settings.value<bool>("Editor/AutoSave", true));
    d->autoSaveIntervalSpinBox->setValue(settings.value<int>("Editor/AutoSaveInterval", 300)); // 5 minutes
    d->spellCheckCheckBox->setChecked(settings.value<bool>("Editor/SpellCheck", false));
    d->liveSpellCheckCheckBox->setChecked(settings.value<bool>("Editor/LiveSpellCheck", false));

    // Security
    d->checkForUpdatesCheckBox->setChecked(settings.value<bool>("Security/CheckForUpdates", true));
    d->sendUsageStatsCheckBox->setChecked(settings.value<bool>("Security/SendUsageStats", false)); // Default OFF
    d->enableCrashReportingCheckBox->setChecked(settings.value<bool>("Security/EnableCrashReporting", true));
    d->warnOnRestrictionsCheckBox->setChecked(settings.value<bool>("Security/WarnOnRestrictions", true));
    d->autoRemovePasswordsCheckBox->setChecked(settings.value<bool>("Security/AutoRemovePasswords", false)); // Feature might be off by default initially

    // Advanced
    d->maxConcurrentRendersSpinBox->setValue(settings.value<int>("Advanced/MaxConcurrentRenders", 4));
    d->maxConcurrentLoadsSpinBox->setValue(settings.value<int>("Advanced/MaxConcurrentLoads", 4));
    d->pageCacheSizeSpinBox->setValue(settings.value<int>("Advanced/PageCacheSizeMB", 50));
    d->backupIntervalSpinBox->setValue(settings.value<int>("Advanced/BackupIntervalSeconds", 300)); // 5 minutes
    d->enableLazyLoadingCheckBox->setChecked(settings.value<bool>("Advanced/EnableLazyLoading", true));
    d->enableProgressiveRenderingCheckBox->setChecked(settings.value<bool>("Advanced/EnableProgressiveRendering", true));

    LOG_DEBUG("Loaded settings into PreferencesDialog UI.");
}

void PreferencesDialog::saveSettings()
{
    Settings& settings = Settings::instance();

    // Display
    settings.setValue("Display/UseHighDpiPixmaps", d->highDpiCheckBox->isChecked());
    settings.setValue("Display/SmoothScrolling", d->smoothScrollingCheckBox->isChecked());
    settings.setValue("Display/ShowPageShadow", d->showPageShadowCheckBox->isChecked());
    settings.setValue("Display/PageSpacing", d->pageSpacingSpinBox->value());
    settings.setValue("Display/PageMargin", d->pageMarginSpinBox->value());
    settings.setValue("Display/UseCustomBackgroundColor", d->useCustomBackgroundColorCheckBox->isChecked());
    settings.setValue("Display/BackgroundColor", d->tempBackgroundColor); // Use temp value that was potentially modified by color dialog

    // Editor
    settings.setValue("Editor/AutoIndent", d->autoIndentCheckBox->isChecked());
    settings.setValue("Editor/TabWidth", d->tabWidthSpinBox->value());
    settings.setValue("Editor/ShowWhitespace", d->showWhitespaceCheckBox->isChecked());
    settings.setValue("Editor/AutoSave", d->autoSaveCheckBox->isChecked());
    settings.setValue("Editor/AutoSaveInterval", d->autoSaveIntervalSpinBox->value());
    settings.setValue("Editor/SpellCheck", d->spellCheckCheckBox->isChecked());
    settings.setValue("Editor/LiveSpellCheck", d->liveSpellCheckCheckBox->isChecked());

    // Security
    settings.setValue("Security/CheckForUpdates", d->checkForUpdatesCheckBox->isChecked());
    settings.setValue("Security/SendUsageStats", d->sendUsageStatsCheckBox->isChecked());
    settings.setValue("Security/EnableCrashReporting", d->enableCrashReportingCheckBox->isChecked());
    settings.setValue("Security/WarnOnRestrictions", d->warnOnRestrictionsCheckBox->isChecked());
    settings.setValue("Security/AutoRemovePasswords", d->autoRemovePasswordsCheckBox->isChecked());

    // Advanced
    settings.setValue("Advanced/MaxConcurrentRenders", d->maxConcurrentRendersSpinBox->value());
    settings.setValue("Advanced/MaxConcurrentLoads", d->maxConcurrentLoadsSpinBox->value());
    settings.setValue("Advanced/PageCacheSizeMB", d->pageCacheSizeSpinBox->value());
    settings.setValue("Advanced/BackupIntervalSeconds", d->backupIntervalSpinBox->value());
    settings.setValue("Advanced/EnableLazyLoading", d->enableLazyLoadingCheckBox->isChecked());
    settings.setValue("Advanced/EnableProgressiveRendering", d->enableProgressiveRenderingCheckBox->isChecked());

    settings.save(); // Force write to persistent storage
    LOG_INFO("Saved settings from PreferencesDialog.");
}

void PreferencesDialog::validateSettings()
{
    // Perform any validation logic here before saving.
    // For example, check if backup interval is reasonable, cache size isn't too large, etc.
    // Show error messages if validation fails and return false, or just log warnings.

    bool valid = true;
    QString errorMsg;

    // Example validation: Cache size should not be ridiculously small or large
    int cacheSize = d->pageCacheSizeSpinBox->value();
    if (cacheSize < 10) {
        errorMsg += tr("Page Cache Size is very small (< 10MB), performance might be affected.\n");
        // Consider setting a minimum or showing warning
    }
    if (cacheSize > 10000) { // 10GB
        errorMsg += tr("Page Cache Size is very large (> 10GB), this might consume too much RAM.\n");
        // Consider setting a maximum or showing warning
    }

    if (!errorMsg.isEmpty()) {
        QMessageBox::warning(this, tr("Settings Validation"), errorMsg);
        LOG_WARN("Settings validation warnings: " << errorMsg);
    }

    LOG_DEBUG("Settings validation completed.");
}

} // namespace QuantilyxDoc