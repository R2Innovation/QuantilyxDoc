/**
 * QuantilyxDoc - Professional Document Editor
 * Copyright (C) 2025 R² Innovative Software
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */
#include "CommandPalette.h"
#include "../core/Settings.h"
#include "../core/Logger.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QListWidget>
#include <QListWidgetItem>
#include <QLabel>
#include <QKeyEvent>
#include <QApplication>
#include <QTimer>
#include <QCompleter>
#include <QStringListModel>
#include <QPainter>
#include <QStyleOption>
#include <QDebug>

namespace QuantilyxDoc {

// Forward declaration of Command structure
struct Command {
    QString id;
    QString title;
    QString category; // e.g., "File", "Edit", "View", "Tools"
    QString description;
    QString shortcut;
    std::function<void()> handler; // Function to execute the command
    QIcon icon; // Optional icon
    int priority; // For sorting/search ranking

    Command(const QString& i, const QString& t, const QString& c, const QString& d, const QString& s, std::function<void()> h, const QIcon& ic = QIcon(), int p = 0)
        : id(i), title(t), category(c), description(d), shortcut(s), handler(std::move(h)), icon(ic), priority(p) {}
};

class CommandPalette::Private {
public:
    Private(CommandPalette* q_ptr)
        : q(q_ptr), searchDelayMs(300) {} // Delay for search throttling

    CommandPalette* q;
    QLineEdit* searchLineEdit;
    QListWidget* resultsListWidget;
    QLabel* placeholderLabel;
    QStringListModel* completerModel;
    QCompleter* completer;

    QList<Command> allCommands;
    QList<Command> filteredCommands; // Results of the current search

    int searchDelayMs;
    QTimer* searchTimer;

    // Helper to populate the initial command list
    void populateCommands();

    // Helper to filter commands based on search text
    void filterCommands(const QString& searchText);

    // Helper to update the results list
    void updateResultsList();

    // Helper to execute a command
    void executeCommand(int index);
};

void CommandPalette::Private::populateCommands() {
    // This is where we would register all available commands in the application.
    // For now, we'll add some example commands.
    // In a real application, this would likely be done by the main application or UI manager
    // when commands are registered globally.

    // File Commands
    allCommands.append(Command("file.new", "New Document", "File", "Create a new document", "Ctrl+N", []() { LOG_INFO("Command Palette: New Document"); }));
    allCommands.append(Command("file.open", "Open Document...", "File", "Open an existing document", "Ctrl+O", []() { LOG_INFO("Command Palette: Open Document"); }));
    allCommands.append(Command("file.save", "Save Document", "File", "Save the current document", "Ctrl+S", []() { LOG_INFO("Command Palette: Save Document"); }));
    allCommands.append(Command("file.print", "Print Document...", "File", "Print the current document", "Ctrl+P", []() { LOG_INFO("Command Palette: Print Document"); }));

    // Edit Commands
    allCommands.append(Command("edit.undo", "Undo", "Edit", "Undo the last action", "Ctrl+Z", []() { LOG_INFO("Command Palette: Undo"); }));
    allCommands.append(Command("edit.redo", "Redo", "Edit", "Redo the last undone action", "Ctrl+Y", []() { LOG_INFO("Command Palette: Redo"); }));
    allCommands.append(Command("edit.find", "Find...", "Edit", "Find text in the document", "Ctrl+F", []() { LOG_INFO("Command Palette: Find"); }));
    allCommands.append(Command("edit.copy", "Copy", "Edit", "Copy selected content", "Ctrl+C", []() { LOG_INFO("Command Palette: Copy"); }));
    allCommands.append(Command("edit.paste", "Paste", "Edit", "Paste content from clipboard", "Ctrl+V", []() { LOG_INFO("Command Palette: Paste"); }));

    // View Commands
    allCommands.append(Command("view.zoom_in", "Zoom In", "View", "Increase the zoom level", "Ctrl++", []() { LOG_INFO("Command Palette: Zoom In"); }));
    allCommands.append(Command("view.zoom_out", "Zoom Out", "View", "Decrease the zoom level", "Ctrl+-", []() { LOG_INFO("Command Palette: Zoom Out"); }));
    allCommands.append(Command("view.fit_page", "Fit Page", "View", "Fit the entire page to the window", "Ctrl+0", []() { LOG_INFO("Command Palette: Fit Page"); }));
    allCommands.append(Command("view.fullscreen", "Toggle Fullscreen", "View", "Toggle full screen mode", "F11", []() { LOG_INFO("Command Palette: Toggle Fullscreen"); }));

    // Settings Commands
    allCommands.append(Command("settings.preferences", "Preferences...", "Settings", "Modify application settings", "", []() { LOG_INFO("Command Palette: Preferences"); }));

    // Help Commands
    allCommands.append(Command("help.about", "About QuantilyxDoc", "Help", "Show information about QuantilyxDoc", "", []() { LOG_INFO("Command Palette: About"); }));

    LOG_INFO("Populated command palette with " << allCommands.size() << " commands.");
}

void CommandPalette::Private::filterCommands(const QString& searchText) {
    filteredCommands.clear();
    QString lowerSearch = searchText.toLower();

    for (const auto& cmd : allCommands) {
        // Simple matching: check title, category, description
        if (cmd.title.toLower().contains(lowerSearch) ||
            cmd.category.toLower().contains(lowerSearch) ||
            cmd.description.toLower().contains(lowerSearch)) {
            filteredCommands.append(cmd);
        }
    }

    // Sort by priority and then by title
    std::sort(filteredCommands.begin(), filteredCommands.end(), [](const Command& a, const Command& b) {
        if (a.priority != b.priority) {
            return a.priority > b.priority; // Higher priority first
        }
        return a.title < b.title; // Alphabetical for same priority
    });
}

void CommandPalette::Private::updateResultsList() {
    resultsListWidget->clear();
    for (const auto& cmd : filteredCommands) {
        QListWidgetItem* item = new QListWidgetItem();
        item->setText(QString("%1 (%2)").arg(cmd.title).arg(cmd.category));
        if (!cmd.description.isEmpty()) {
            item->setToolTip(cmd.description); // Show description as tooltip
        }
        if (!cmd.shortcut.isEmpty()) {
            // Append shortcut to text or use setData for custom display
            item->setText(item->text() + QString("  (%1)").arg(cmd.shortcut));
        }
        if (!cmd.icon.isNull()) {
            item->setIcon(cmd.icon);
        }
        resultsListWidget->addItem(item);
    }
    LOG_DEBUG("Updated command palette results list with " << filteredCommands.size() << " items.");
}

void CommandPalette::Private::executeCommand(int index) {
    if (index >= 0 && index < filteredCommands.size()) {
        const Command& cmd = filteredCommands[index];
        LOG_INFO("Executing command from palette: " << cmd.id << " - " << cmd.title);
        if (cmd.handler) {
            cmd.handler(); // Call the associated function
        }
        // Optionally, hide the palette after execution
        q->hide();
    }
}

CommandPalette::CommandPalette(QWidget* parent)
    : QWidget(parent)
    , d(new Private(this))
{
    setWindowFlags(Qt::Popup | Qt::FramelessWindowHint); // Popup window without frame
    setAttribute(Qt::WA_TranslucentBackground); // Allow custom background painting

    // Layout
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(10, 10, 10, 10); // Add some padding

    // Search input
    d->searchLineEdit = new QLineEdit(this);
    d->searchLineEdit->setPlaceholderText(tr("Type a command or search..."));
    d->searchLineEdit->setFocus(); // Start with focus on the search box
    mainLayout->addWidget(d->searchLineEdit);

    // Results list
    d->resultsListWidget = new QListWidget(this);
    d->resultsListWidget->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff); // Keep it simple
    d->resultsListWidget->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    d->resultsListWidget->setSelectionMode(QAbstractItemView::SingleSelection);
    d->resultsListWidget->setSelectionBehavior(QAbstractItemView::SelectRows);
    d->resultsListWidget->setFrameStyle(QFrame::NoFrame); // Remove default frame if desired
    mainLayout->addWidget(d->resultsListWidget);

    // Placeholder label (optional, shown when no results)
    d->placeholderLabel = new QLabel(tr("No commands found"), this);
    d->placeholderLabel->setAlignment(Qt::AlignCenter);
    d->placeholderLabel->hide(); // Hidden by default
    mainLayout->addWidget(d->placeholderLabel);

    // Set up search delay timer
    d->searchTimer = new QTimer(this);
    d->searchTimer->setSingleShot(true);
    connect(d->searchTimer, &QTimer::timeout, [this]() {
        d->filterCommands(d->searchLineEdit->text());
        d->updateResultsList();
        d->resultsListWidget->setCurrentRow(0); // Select first result
        // Show/hide placeholder
        d->placeholderLabel->setVisible(d->filteredCommands.isEmpty());
        d->resultsListWidget->setVisible(!d->filteredCommands.isEmpty());
    });

    // Connect search input to filtering
    connect(d->searchLineEdit, &QLineEdit::textChanged, [this](const QString& text) {
        d->searchTimer->stop(); // Cancel previous delayed search
        d->searchTimer->start(d->searchDelayMs); // Start new delayed search
    });

    // Connect list item selection/activation
    connect(d->resultsListWidget, &QListWidget::itemActivated, [this](QListWidgetItem* item) {
        int index = d->resultsListWidget->row(item);
        d->executeCommand(index);
    });

    // Connect Enter key press in search box to execute selected command
    connect(d->searchLineEdit, &QLineEdit::returnPressed, [this]() {
        int currentRow = d->resultsListWidget->currentRow();
        if (currentRow >= 0) {
            d->executeCommand(currentRow);
        }
    });

    // Connect Escape key to hide the palette
    connect(d->searchLineEdit, &QLineEdit::textChanged, [this](const QString& text) {
        if (text.isEmpty() && !d->searchLineEdit->hasFocus()) {
             // Maybe hide if text is cleared and focus is lost? Or just on Esc key.
             // Let's handle Esc in keyPressEvent.
        }
    });

    // Populate initial commands
    d->populateCommands();
    d->filterCommands(""); // Show all commands initially
    d->updateResultsList();

    LOG_INFO("CommandPalette initialized.");
}

CommandPalette::~CommandPalette()
{
    LOG_INFO("CommandPalette destroyed.");
}

void CommandPalette::showEvent(QShowEvent* event)
{
    QWidget::showEvent(event);
    d->searchLineEdit->setFocus(); // Ensure search box has focus when shown
    d->searchLineEdit->selectAll(); // Select all text for easy replacement
    LOG_DEBUG("CommandPalette shown.");
}

void CommandPalette::keyPressEvent(QKeyEvent* event)
{
    // Handle navigation keys
    switch (event->key()) {
        case Qt::Key_Down:
            if (d->resultsListWidget->count() > 0) {
                int currentRow = d->resultsListWidget->currentRow();
                d->resultsListWidget->setCurrentRow(qMin(currentRow + 1, d->resultsListWidget->count() - 1));
            }
            event->accept();
            return;
        case Qt::Key_Up:
            if (d->resultsListWidget->count() > 0) {
                int currentRow = d->resultsListWidget->currentRow();
                d->resultsListWidget->setCurrentRow(qMax(currentRow - 1, 0));
            }
            event->accept();
            return;
        case Qt::Key_Escape:
            hide(); // Close the palette
            event->accept();
            return;
        case Qt::Key_Return:
        case Qt::Key_Enter:
            // Already handled by returnPressed signal, but catch here if needed for other logic
            int currentRow = d->resultsListWidget->currentRow();
            if (currentRow >= 0) {
                d->executeCommand(currentRow);
            }
            event->accept();
            return;
        // Add other keys if needed
    }
    // Let base class handle other keys or pass to search box
    QWidget::keyPressEvent(event);
}

void CommandPalette::paintEvent(QPaintEvent* event)
{
    // Draw a custom background with rounded corners and shadow
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    // Draw background rect with rounded corners
    QStyleOption opt;
    opt.initFrom(this);
    style()->drawPrimitive(QStyle::PE_Widget, &opt, &painter, this);

    // Draw a subtle shadow or border if desired
    // painter.setPen(QPen(QColor(0, 0, 0, 50), 1)); // Light shadow color
    // painter.drawRoundedRect(rect().adjusted(1, 1, -1, -1), 5, 5); // Slightly inset rect

    QWidget::paintEvent(event);
}

void CommandPalette::showAtCenter(QWidget* parentWidget)
{
    if (!parentWidget) {
        LOG_WARN("CommandPalette::showAtCenter: parent widget is null.");
        return;
    }

    // Calculate position to center over the parent
    QRect parentRect = parentWidget->geometry();
    QPoint globalPoint = parentWidget->mapToGlobal(parentRect.center());
    globalPoint -= QPoint(width() / 2, height() / 2); // Adjust for own size

    // Ensure it stays within screen bounds
    QRect screenRect = QApplication::desktop()->availableGeometry(parentWidget);
    globalPoint.setX(qMax(screenRect.left(), qMin(globalPoint.x(), screenRect.right() - width())));
    globalPoint.setY(qMax(screenRect.top(), qMin(globalPoint.y(), screenRect.bottom() - height())));

    move(globalPoint);
    show();
    LOG_DEBUG("CommandPalette shown at center of parent widget.");
}

void CommandPalette::addCommand(const QString& id, const QString& title, const QString& category, const QString& description, const QString& shortcut, std::function<void()> handler, const QIcon& icon, int priority)
{
    d->allCommands.append(Command(id, title, category, description, shortcut, std::move(handler), icon, priority));
    LOG_DEBUG("Added command to palette: " << id << " - " << title);
    // If the palette is currently visible and showing all commands, update the list
    if (isVisible() && d->searchLineEdit->text().isEmpty()) {
        d->filterCommands(""); // Re-filter with empty string
        d->updateResultsList();
    }
}

void CommandPalette::removeCommand(const QString& id)
{
    auto it = std::find_if(d->allCommands.begin(), d->allCommands.end(),
                           [&id](const Command& cmd) { return cmd.id == id; });
    if (it != d->allCommands.end()) {
        d->allCommands.erase(it);
        LOG_DEBUG("Removed command from palette: " << id);
        // Update list if visible
        if (isVisible()) {
            d->filterCommands(d->searchLineEdit->text()); // Re-filter current search
            d->updateResultsList();
        }
    }
}

} // namespace QuantilyxDoc