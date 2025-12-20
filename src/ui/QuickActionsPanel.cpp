/**
 * QuantilyxDoc - Professional Document Editor
 * Copyright (C) 2025 R² Innovative Software
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */
#include "QuickActionsPanel.h"
#include "../core/Settings.h"
#include "../core/Logger.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QToolButton>
#include <QLabel>
#include <QFrame>
#include <QScrollArea>
#include <QButtonGroup>
#include <QPainter>
#include <QStyleOption>
#include <QApplication>
#include <QDesktopWidget>
#include <QTimer>
#include <QDebug>

namespace QuantilyxDoc {

// Forward declaration of QuickAction structure
struct QuickAction {
    QString id;
    QString title;
    QString description;
    QIcon icon;
    std::function<void()> handler; // Function to execute the action
    bool isFrequent; // Whether it's considered a frequent action
    int usageCount;  // How many times it has been used (for adaptive UI)
    QDateTime lastUsed; // When it was last used (for adaptive UI)

    QuickAction(const QString& i, const QString& t, const QString& d, const QIcon& ic, std::function<void()> h)
        : id(i), title(t), description(d), icon(ic), handler(std::move(h)), isFrequent(false), usageCount(0) {}
};

class QuickActionsPanel::Private {
public:
    Private(QuickActionsPanel* q_ptr)
        : q(q_ptr), maxVisibleActions(10) {} // Configurable max actions to show

    QuickActionsPanel* q;
    QScrollArea* scrollArea;
    QWidget* contentWidget;
    QGridLayout* contentLayout;
    QButtonGroup* buttonGroup;

    QList<QuickAction> allActions;
    QList<QuickAction> visibleActions; // Subset currently shown based on frequency/favorites

    int maxVisibleActions;

    // Helper to populate the initial action list
    void populateActions();

    // Helper to update the UI based on the current visible actions list
    void updateUi();

    // Helper to handle button clicks
    void onButtonClicked(int id);

    // Helper to promote an action as frequent based on usage
    void markActionAsFrequent(const QString& actionId);

    // Helper to get the index of an action by ID
    int findActionIndex(const QString& actionId) const;
};

void QuickActionsPanel::Private::populateActions() {
    // This is where we would register initial quick actions.
    // These are typically the most common operations a user might perform.
    // In a real application, this could be loaded from settings or defined centrally.

    allActions.append(QuickAction("action.open", "Open", "Open a document", QIcon::fromTheme("document-open"), []() { LOG_INFO("Quick Action: Open"); }));
    allActions.append(QuickAction("action.save", "Save", "Save the document", QIcon::fromTheme("document-save"), []() { LOG_INFO("Quick Action: Save"); }));
    allActions.append(QuickAction("action.print", "Print", "Print the document", QIcon::fromTheme("document-print"), []() { LOG_INFO("Quick Action: Print"); }));
    allActions.append(QuickAction("action.find", "Find", "Find text", QIcon::fromTheme("edit-find"), []() { LOG_INFO("Quick Action: Find"); }));
    allActions.append(QuickAction("action.undo", "Undo", "Undo last action", QIcon::fromTheme("edit-undo"), []() { LOG_INFO("Quick Action: Undo"); }));
    allActions.append(QuickAction("action.redo", "Redo", "Redo last action", QIcon::fromTheme("edit-redo"), []() { LOG_INFO("Quick Action: Redo"); }));
    allActions.append(QuickAction("action.zoom_in", "Zoom In", "Increase zoom", QIcon::fromTheme("zoom-in"), []() { LOG_INFO("Quick Action: Zoom In"); }));
    allActions.append(QuickAction("action.zoom_out", "Zoom Out", "Decrease zoom", QIcon::fromTheme("zoom-out"), []() { LOG_INFO("Quick Action: Zoom Out"); }));
    allActions.append(QuickAction("action.fit_page", "Fit Page", "Fit page to view", QIcon::fromTheme("zoom-fit-best"), []() { LOG_INFO("Quick Action: Fit Page"); }));
    allActions.append(QuickAction("action.preferences", "Settings", "Open preferences", QIcon::fromTheme("preferences-system"), []() { LOG_INFO("Quick Action: Preferences"); }));
    allActions.append(QuickAction("action.help", "Help", "Open help", QIcon::fromTheme("help-contents"), []() { LOG_INFO("Quick Action: Help"); }));
    allActions.append(QuickAction("action.about", "About", "About the application", QIcon::fromTheme("help-about"), []() { LOG_INFO("Quick Action: About"); }));

    // Mark some as initially frequent based on common usage patterns or settings
    for (auto& action : allActions) {
        if (action.id == "action.open" || action.id == "action.save" || action.id == "action.undo" || action.id == "action.find") {
            action.isFrequent = true;
            action.usageCount = 10; // Give them a higher initial count
        }
    }

    LOG_INFO("Populated quick actions panel with " << allActions.size() << " actions.");
}

void QuickActionsPanel::Private::updateUi() {
    // Clear existing buttons
    QLayoutItem* child;
    while ((child = contentLayout->takeAt(0)) != nullptr) {
        delete child->widget();
        delete child;
    }

    // Determine which actions to show (e.g., top N frequent, or favorites)
    // For now, let's show all 'frequent' actions up to maxVisibleActions
    visibleActions.clear();
    auto frequentActions = allActions;
    std::sort(frequentActions.begin(), frequentActions.end(), [](const QuickAction& a, const QuickAction& b) {
        // Sort by usage count descending, then by last used descending
        if (a.usageCount != b.usageCount) return a.usageCount > b.usageCount;
        return a.lastUsed > b.lastUsed;
    });

    int count = 0;
    for (const auto& action : frequentActions) {
        if (action.isFrequent && count < maxVisibleActions) {
            visibleActions.append(action);
            count++;
        }
    }

    // Create buttons for visible actions
    int row = 0;
    int col = 0;
    const int cols = 5; // Fixed number of columns for grid layout
    for (const auto& action : visibleActions) {
        QToolButton* button = new QToolButton(q);
        button->setIcon(action.icon);
        button->setIconSize(QSize(32, 32)); // Standard icon size for quick actions
        button->setText(action.title);
        button->setToolButtonStyle(Qt::ToolButtonTextUnderIcon); // Icon above text
        button->setToolTip(action.description); // Show description as tooltip
        button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
        button->setMinimumHeight(60); // Ensure buttons are tall enough for icon + text

        // Store the action ID in the button's object name or user property for identification
        button->setProperty("actionId", action.id);

        // Add to layout
        contentLayout->addWidget(button, row, col);
        col++;
        if (col >= cols) {
            col = 0;
            row++;
        }

        // Connect button click
        connect(button, &QToolButton::clicked, [this, button]() {
            QString actionId = button->property("actionId").toString();
            onButtonClicked(findActionIndex(actionId));
        });
    }

    LOG_DEBUG("Updated QuickActionsPanel UI with " << visibleActions.size() << " visible actions.");
}

void QuickActionsPanel::Private::onButtonClicked(int actionIndex) {
    if (actionIndex >= 0 && actionIndex < allActions.size()) {
        QuickAction& action = allActions[actionIndex];
        LOG_INFO("Quick Action clicked: " << action.id << " - " << action.title);
        if (action.handler) {
            action.handler(); // Execute the associated function
        }

        // Update usage statistics
        action.usageCount++;
        action.lastUsed = QDateTime::currentDateTime();
        markActionAsFrequent(action.id);

        // Optionally, update UI immediately if adaptive behavior is desired
        // QTimer::singleShot(0, [this]() { updateUi(); }); // Delay update slightly
    }
}

void QuickActionsPanel::Private::markActionAsFrequent(const QString& actionId) {
    int index = findActionIndex(actionId);
    if (index >= 0) {
        // Simple heuristic: if usage count crosses a threshold, mark as frequent
        const int FREQUENCY_THRESHOLD = 5;
        if (allActions[index].usageCount >= FREQUENCY_THRESHOLD) {
            allActions[index].isFrequent = true;
        }
        // Re-sort and update UI if necessary
        // This could be optimized to only update if the top N actions changed
        updateUi();
    }
}

int QuickActionsPanel::Private::findActionIndex(const QString& actionId) const {
    for (int i = 0; i < allActions.size(); ++i) {
        if (allActions[i].id == actionId) {
            return i;
        }
    }
    return -1; // Not found
}

QuickActionsPanel::QuickActionsPanel(QWidget* parent)
    : QWidget(parent)
    , d(new Private(this))
{
    // Main layout
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(5, 5, 5, 5); // Add some padding

    // Scroll area for actions
    d->scrollArea = new QScrollArea(this);
    d->scrollArea->setWidgetResizable(true);
    d->scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff); // Vertical only for grid
    d->scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    d->scrollArea->setFrameStyle(QFrame::NoFrame); // Remove default frame if desired

    // Content widget and layout
    d->contentWidget = new QWidget(d->scrollArea);
    d->contentLayout = new QGridLayout(d->contentWidget);
    d->contentLayout->setAlignment(Qt::AlignTop | Qt::AlignLeft); // Align buttons to top-left
    d->contentLayout->setSpacing(5); // Space between buttons
    d->contentLayout->setContentsMargins(5, 5, 5, 5); // Margin around grid

    d->scrollArea->setWidget(d->contentWidget);
    mainLayout->addWidget(d->scrollArea);

    // Button group (not strictly necessary for QToolButton, but good practice for grouping logic)
    d->buttonGroup = new QButtonGroup(this);
    d->buttonGroup->setExclusive(false); // Allow multiple selections if needed (though not typical for quick actions)

    // Populate initial actions
    d->populateActions();
    d->updateUi(); // Build the initial UI

    LOG_INFO("QuickActionsPanel initialized.");
}

QuickActionsPanel::~QuickActionsPanel()
{
    LOG_INFO("QuickActionsPanel destroyed.");
}

void QuickActionsPanel::addQuickAction(const QString& id, const QString& title, const QString& description, const QIcon& icon, std::function<void()> handler)
{
    // Check if action already exists
    auto it = std::find_if(d->allActions.begin(), d->allActions.end(),
                           [&id](const QuickAction& action) { return action.id == id; });
    if (it != d->allActions.end()) {
        LOG_WARN("Quick action with ID already exists, overwriting: " << id);
        *it = QuickAction(id, title, description, icon, std::move(handler));
    } else {
        d->allActions.append(QuickAction(id, title, description, icon, std::move(handler)));
        LOG_DEBUG("Added quick action: " << id << " - " << title);
    }
    // Update UI
    d->updateUi();
}

void QuickActionsPanel::removeQuickAction(const QString& id)
{
    auto it = std::find_if(d->allActions.begin(), d->allActions.end(),
                           [&id](const QuickAction& action) { return action.id == id; });
    if (it != d->allActions.end()) {
        d->allActions.erase(it);
        LOG_DEBUG("Removed quick action: " << id);
        // Update UI
        d->updateUi();
    }
}

void QuickActionsPanel::setActionAsFrequent(const QString& id, bool frequent)
{
    auto it = std::find_if(d->allActions.begin(), d->allActions.end(),
                           [&id](const QuickAction& action) { return action.id == id; });
    if (it != d->allActions.end()) {
        it->isFrequent = frequent;
        LOG_DEBUG("Set quick action " << id << " frequent status to " << frequent);
        // Update UI
        d->updateUi();
    }
}

void QuickActionsPanel::setMaxVisibleActions(int max)
{
    if (max > 0) {
        d->maxVisibleActions = max;
        LOG_DEBUG("Set max visible quick actions to " << max);
        // Update UI
        d->updateUi();
    }
}

int QuickActionsPanel::maxVisibleActions() const
{
    return d->maxVisibleActions;
}

void QuickActionsPanel::paintEvent(QPaintEvent* event)
{
    // Draw a custom background if needed, similar to CommandPalette
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    QStyleOption opt;
    opt.initFrom(this);
    style()->drawPrimitive(QStyle::PE_Widget, &opt, &painter, this);

    QWidget::paintEvent(event);
}

} // namespace QuantilyxDoc