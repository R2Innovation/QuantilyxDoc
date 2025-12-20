/**
 * QuantilyxDoc - Professional Document Editor
 * Copyright (C) 2025 R² Innovative Software
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */
#ifndef QUANTILYX_QUICKACTIONSPANEL_H
#define QUANTILYX_QUICKACTIONSPANEL_H

#include <QWidget>
#include <QScrollArea>
#include <QGridLayout>
#include <QToolButton>
#include <QButtonGroup>
#include <functional>
#include <memory>

namespace QuantilyxDoc {

/**
 * @brief A panel providing quick access to frequently used actions.
 *
 * Displays a grid of action buttons that can be configured by the user
 * or adapt based on usage frequency. Commonly used actions float to the top.
 */
class QuickActionsPanel : public QWidget
{
    Q_OBJECT

public:
    /**
     * @brief Constructor.
     * @param parent Parent widget (usually the MainWindow or a dock).
     */
    explicit QuickActionsPanel(QWidget* parent = nullptr);

    /**
     * @brief Destructor.
     */
    ~QuickActionsPanel() override;

    /**
     * @brief Add a quick action button to the panel.
     * @param id Unique identifier for the action.
     * @param title Display title for the action.
     * @param description Detailed description shown as tooltip.
     * @param icon Icon for the action button.
     * @param handler Function to execute when the action is triggered.
     */
    void addQuickAction(const QString& id, const QString& title, const QString& description,
                        const QIcon& icon, std::function<void()> handler);

    /**
     * @brief Remove a quick action from the panel.
     * @param id The ID of the action to remove.
     */
    void removeQuickAction(const QString& id);

    /**
     * @brief Mark a quick action as frequently used or not.
     * This influences its visibility/priority in the panel.
     * @param id The ID of the action.
     * @param frequent Whether the action is considered frequent.
     */
    void setActionAsFrequent(const QString& id, bool frequent);

    /**
     * @brief Set the maximum number of actions to display in the panel.
     * @param max Maximum number of visible actions.
     */
    void setMaxVisibleActions(int max);

    /**
     * @brief Get the maximum number of visible actions.
     * @return Maximum number.
     */
    int maxVisibleActions() const;

    /**
     * @brief Get the list of all registered action IDs.
     * @return List of action IDs.
     */
    QStringList actionIds() const;

signals:
    /**
     * @brief Emitted when a quick action is executed.
     * @param actionId The ID of the executed action.
     */
    void actionExecuted(const QString& actionId);

    /**
     * @brief Emitted when the list of visible actions changes.
     */
    void visibleActionsChanged();

protected:
    /**
     * @brief Paint event for custom appearance.
     * @param event Paint event.
     */
    void paintEvent(QPaintEvent* event) override;

private:
    class Private;
    std::unique_ptr<Private> d;
};

} // namespace QuantilyxDoc

#endif // QUANTILYX_QUICKACTIONSPANEL_H