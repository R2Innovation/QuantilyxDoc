/**
 * QuantilyxDoc - Professional Document Editor
 * Copyright (C) 2025 R² Innovative Software
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */
#ifndef QUANTILYX_TOOLBAR_H
#define QUANTILYX_TOOLBAR_H

#include <QToolBar>
#include <QToolButton>
#include <QAction>
#include <QActionGroup>
#include <QButtonGroup>
#include <QComboBox>
#include <QSpinBox>
#include <QLabel>
#include <memory>

namespace QuantilyxDoc {

/**
 * @brief Custom toolbar with application-specific actions and controls.
 *
 * Manages a set of frequently used actions and controls like file operations,
 * editing tools, view controls (zoom, rotation), and potentially document-specific
 * tools. Can be configured by the user via the Preferences dialog.
 */
class Toolbar : public QToolBar
{
    Q_OBJECT

public:
    /**
     * @brief Constructor.
     * @param parent Parent widget (usually the MainWindow).
     */
    explicit Toolbar(QWidget* parent = nullptr);

    /**
     * @brief Destructor.
     */
    ~Toolbar() override;

    /**
     * @brief Get the default action layout for this toolbar.
     * @return List of action IDs in default order.
     */
    QStringList defaultActionLayout() const;

    /**
     * @brief Load toolbar configuration from settings.
     * Applies saved visibility, order, and customizations.
     */
    void loadConfiguration();

    /**
     * @brief Save current toolbar configuration to settings.
     * Saves visibility, order, and customizations.
     */
    void saveConfiguration();

    /**
     * @brief Reset the toolbar to its default configuration.
     */
    void resetToDefault();

    /**
     * @brief Add a custom action to the toolbar.
     * @param action The action to add.
     * @param insertBefore Optional action ID to insert before. If empty, appends.
     */
    void addCustomAction(QAction* action, const QString& insertBefore = QString());

    /**
     * @brief Remove a custom action from the toolbar.
     * @param actionId The ID of the action to remove.
     */
    void removeCustomAction(const QString& actionId);

    /**
     * @brief Check if a specific action is currently visible on the toolbar.
     * @param actionId The ID of the action.
     * @return True if visible.
     */
    bool isActionVisible(const QString& actionId) const;

    /**
     * @brief Set the visibility of a specific action on the toolbar.
     * @param actionId The ID of the action.
     * @param visible Whether the action should be visible.
     */
    void setActionVisible(const QString& actionId, bool visible);

    /**
     * @brief Get the list of currently visible action IDs.
     * @return List of visible action IDs.
     */
    QStringList visibleActionIds() const;

    /**
     * @brief Get the QAction object for a given ID, if it exists on this toolbar.
     * @param actionId The ID of the action.
     * @return Pointer to the QAction, or nullptr if not found.
     */
    QAction* actionById(const QString& actionId) const;

signals:
    /**
     * @brief Emitted when the toolbar configuration changes (actions added/removed/reordered).
     */
    void configurationChanged();

private slots:
    void onActionTriggered(QAction* action); // Generic handler for actions added to this toolbar

private:
    class Private;
    std::unique_ptr<Private> d;

    // Helper to create standard actions
    void createStandardActions();
    // Helper to add actions to the toolbar based on a list of IDs
    void buildFromActionList(const QStringList& actionIds);
};

} // namespace QuantilyxDoc

#endif // QUANTILYX_TOOLBAR_H