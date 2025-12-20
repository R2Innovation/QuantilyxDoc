/**
 * QuantilyxDoc - Professional Document Editor
 * Copyright (C) 2025 R² Innovative Software
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */
#include "Toolbar.h"
#include "../core/Settings.h"
#include "../core/UndoStack.h"
#include "../core/RecentFiles.h"
#include "../core/Logger.h"
#include <QAction>
#include <QActionGroup>
#include <QToolButton>
#include <QComboBox>
#include <QSpinBox>
#include <QLabel>
#include <QApplication>
#include <QMainWindow> // For casting parent if needed
#include <QMenu>
#include <QDebug>

namespace QuantilyxDoc {

class Toolbar::Private {
public:
    Private(Toolbar* q_ptr) : q(q_ptr) {}

    Toolbar* q;

    // Standard actions managed by the toolbar
    QHash<QString, QAction*> standardActions; // Map ID -> QAction*

    // Configuration keys
    static const QString configKey;

    // Helper to create a standard action and add it to the map
    QAction* createAndRegisterAction(const QString& id, const QString& text, const QString& icon, const QString& tooltip, const QKeySequence& shortcut = QKeySequence());
};

const QString Toolbar::Private::configKey = "Toolbar/StandardLayout";

QAction* Toolbar::Private::createAndRegisterAction(const QString& id, const QString& text, const QString& icon, const QString& tooltip, const QKeySequence& shortcut)
{
    QAction* action = new QAction(text, q);
    if (!icon.isEmpty()) {
        // action->setIcon(QIcon::fromTheme(icon)); // Use themed icon if available
        // For now, we'll just use text or a placeholder
        // action->setIcon(QIcon(":/icons/" + icon + ".png")); // Or use resource
    }
    action->setToolTip(tooltip);
    if (!shortcut.isEmpty()) {
        action->setShortcut(shortcut);
    }
    action->setObjectName(id); // Use ID as object name for identification
    standardActions.insert(id, action);
    return action;
}

Toolbar::Toolbar(QWidget* parent)
    : QToolBar(parent)
    , d(new Private(this))
{
    setObjectName("StandardToolbar"); // Set object name for QMainWindow to save/restore state
    setMovable(true); // Allow user to move the toolbar
    setFloatable(true); // Allow user to float the toolbar
    setAllowedAreas(Qt::TopToolBarArea | Qt::BottomToolBarArea); // Restrict to top/bottom

    createStandardActions();

    // Load initial configuration
    loadConfiguration();

    LOG_INFO("Toolbar initialized.");
}

Toolbar::~Toolbar()
{
    // Configuration is saved by MainWindow when it closes
    LOG_INFO("Toolbar destroyed.");
}

void Toolbar::createStandardActions()
{
    // File Actions
    d->createAndRegisterAction("file.new", tr("New"), "document-new", tr("Create a new document"), QKeySequence::New);
    d->createAndRegisterAction("file.open", tr("Open"), "document-open", tr("Open an existing document"), QKeySequence::Open);
    d->createAndRegisterAction("file.save", tr("Save"), "document-save", tr("Save the document"), QKeySequence::Save);
    d->createAndRegisterAction("file.print", tr("Print"), "document-print", tr("Print the document"), QKeySequence::Print);

    // Edit Actions
    d->createAndRegisterAction("edit.undo", tr("Undo"), "edit-undo", tr("Undo the last action"), QKeySequence::Undo);
    d->createAndRegisterAction("edit.redo", tr("Redo"), "edit-redo", tr("Redo the last undone action"), QKeySequence::Redo);
    d->createAndRegisterAction("edit.cut", tr("Cut"), "edit-cut", tr("Cut the selected content"), QKeySequence::Cut);
    d->createAndRegisterAction("edit.copy", tr("Copy"), "edit-copy", tr("Copy the selected content"), QKeySequence::Copy);
    d->createAndRegisterAction("edit.paste", tr("Paste"), "edit-paste", tr("Paste content from clipboard"), QKeySequence::Paste);
    d->createAndRegisterAction("edit.find", tr("Find"), "edit-find", tr("Find text in the document"), QKeySequence::Find);

    // View Actions
    d->createAndRegisterAction("view.zoom_in", tr("Zoom In"), "zoom-in", tr("Increase the zoom level"), QKeySequence::ZoomIn);
    d->createAndRegisterAction("view.zoom_out", tr("Zoom Out"), "zoom-out", tr("Decrease the zoom level"), QKeySequence::ZoomOut);
    d->createAndRegisterAction("view.fit_page", tr("Fit Page"), "zoom-fit-best", tr("Fit the entire page to the window"), QKeySequence(tr("Ctrl+0")));

    // Connect standard actions to their respective systems
    // Undo/Redo
    auto undoAction = d->standardActions.value("edit.undo");
    auto redoAction = d->standardActions.value("edit.redo");
    if (undoAction) {
        connect(&UndoStack::instance(), &UndoStack::canUndoChanged, undoAction, &QAction::setEnabled);
        connect(undoAction, &QAction::triggered, &UndoStack::instance(), &UndoStack::undo);
    }
    if (redoAction) {
        connect(&UndoStack::instance(), &UndoStack::canRedoChanged, redoAction, &QAction::setEnabled);
        connect(redoAction, &QAction::triggered, &UndoStack::instance(), &UndoStack::redo);
    }

    LOG_DEBUG("Created " << d->standardActions.size() << " standard toolbar actions.");
}

QStringList Toolbar::defaultActionLayout() const
{
    // Define the default order of actions on the toolbar
    return QStringList() << "file.new" << "file.open" << "file.save" << "file.print"
                         << "separator"
                         << "edit.undo" << "edit.redo" << "edit.cut" << "edit.copy" << "edit.paste" << "edit.find"
                         << "separator"
                         << "view.zoom_in" << "view.zoom_out" << "view.fit_page";
}

void Toolbar::loadConfiguration()
{
    Settings& settings = Settings::instance();
    QStringList layout = settings.value<QStringList>(d->configKey, defaultActionLayout());

    // Clear current actions
    clear();

    // Rebuild toolbar from saved layout
    buildFromActionList(layout);

    LOG_DEBUG("Loaded toolbar configuration with " << layout.size() << " items.");
}

void Toolbar::saveConfiguration()
{
    Settings& settings = Settings::instance();
    QStringList layout;

    // Iterate through current actions on the toolbar to get the order
    for (QAction* action : actions()) {
        if (action->isSeparator()) {
            layout.append("separator");
        } else {
            layout.append(action->objectName()); // Use object name as ID
        }
    }

    settings.setValue(d->configKey, layout);
    LOG_DEBUG("Saved toolbar configuration with " << layout.size() << " items.");
}

void Toolbar::resetToDefault()
{
    clear();
    buildFromActionList(defaultActionLayout());
    saveConfiguration(); // Save the reset state
    LOG_INFO("Toolbar reset to default configuration.");
}

void Toolbar::addCustomAction(QAction* action, const QString& insertBefore)
{
    if (!action) return;

    if (insertBefore.isEmpty()) {
        addAction(action); // Append to end
    } else {
        QAction* beforeAction = d->standardActions.value(insertBefore);
        if (beforeAction) {
            insertAction(beforeAction, action);
        } else {
            // If 'insertBefore' is not a standard action, try to find it among current actions
            for (QAction* currentAction : actions()) {
                if (currentAction->objectName() == insertBefore) {
                    insertAction(currentAction, action);
                    break;
                }
            }
        }
    }
    emit configurationChanged();
    saveConfiguration(); // Update settings
}

void Toolbar::removeCustomAction(const QString& actionId)
{
    // Find the action by ID (object name)
    for (QAction* action : actions()) {
        if (action->objectName() == actionId) {
            removeAction(action);
            // Note: The action object itself is not deleted by removeAction,
            // the caller is responsible if it owns it.
            break;
        }
    }
    emit configurationChanged();
    saveConfiguration(); // Update settings
}

bool Toolbar::isActionVisible(const QString& actionId) const
{
    for (QAction* action : actions()) {
        if (action->objectName() == actionId) {
            return !action->isSeparator(); // Return false for separators too, but they are not 'actions' in the user sense
        }
    }
    return false; // Action not found on toolbar
}

void Toolbar::setActionVisible(const QString& actionId, bool visible)
{
    if (visible) {
        // If not visible, add it. Need to determine where to add it based on the saved order.
        // A more complex implementation would track a full list of potential actions and their desired visibility/order.
        // For now, this is a stub.
        LOG_WARN("setActionVisible(true) not fully implemented for adding action '" << actionId << "'. Use addCustomAction.");
    } else {
        // If visible, remove it
        removeCustomAction(actionId);
    }
}

QStringList Toolbar::visibleActionIds() const
{
    QStringList ids;
    for (QAction* action : actions()) {
        if (!action->isSeparator()) {
            ids.append(action->objectName());
        }
    }
    return ids;
}

QAction* Toolbar::actionById(const QString& actionId) const
{
    return d->standardActions.value(actionId, nullptr);
}

void Toolbar::onActionTriggered(QAction* action)
{
    Q_UNUSED(action);
    // This slot can be used for logging or other generic handling when *any* action on this toolbar is triggered.
    LOG_DEBUG("Toolbar action triggered: " << (action ? action->objectName() : "nullptr"));
}

void Toolbar::buildFromActionList(const QStringList& actionIds)
{
    for (const QString& id : actionIds) {
        if (id == "separator") {
            addSeparator();
        } else {
            QAction* action = d->standardActions.value(id);
            if (action) {
                addAction(action);
            } else {
                LOG_WARN("Toolbar configuration references unknown action ID: " << id);
            }
        }
    }
    LOG_DEBUG("Built toolbar from action list of size " << actionIds.size());
}

} // namespace QuantilyxDoc