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
#include "../core/Logger.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QListWidget>
#include <QListWidgetItem>
#include <QLabel>
#include <QKeyEvent>
#include <QApplication>
#include <QDesktopWidget>
#include <QPainter>
#include <QStyleOption>
#include <QTimer>
#include <QMutex>
#include <QMutexLocker>
#include <QCompleter>
#include <QStringListModel>
#include <QStandardItemModel>
#include <QStyledItemDelegate>
#include <QPainter>
#include <QDebug>

namespace QuantilyxDoc {

class CommandPaletteDelegate : public QStyledItemDelegate
{
public:
    CommandPaletteDelegate(QObject* parent = nullptr) : QStyledItemDelegate(parent) {}

    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override
    {
        QStyleOptionViewItem opt(option);
        initStyleOption(&opt, index);

        // Draw background
        painter->fillRect(opt.rect, opt.palette.brush(QPalette::Base));

        // Draw highlight for selected item
        if (opt.state & QStyle::State_Selected) {
            painter->fillRect(opt.rect, opt.palette.brush(QPalette::Highlight));
        }

        // Get data (assuming CommandPalette stores Command structs or similar in the model)
        // This is complex because QListView needs a model (QStringListModel, QStandardItemModel).
        // We'll assume for this example that the model stores the Command title as string,
        // and the Command object itself is stored as a pointer in the item's data (Qt::UserRole).
        QString title = opt.text;
        QString category = index.data(Qt::UserRole + 1).toString(); // Store category in a custom role
        QString description = index.data(Qt::UserRole + 2).toString(); // Store description in a custom role

        QRect textRect = opt.rect.adjusted(10, 5, -10, -5); // Add some padding

        // Draw title
        painter->setPen(opt.palette.color(opt.state & QStyle::State_Selected ? QPalette::HighlightedText : QPalette::Text));
        painter->setFont(QFont(painter->font().family(), painter->font().pointSize(), QFont::Bold));
        painter->drawText(textRect, Qt::AlignTop | Qt::AlignLeft, title);

        // Draw category (smaller font, lighter color)
        if (!category.isEmpty()) {
            painter->setPen(opt.palette.color(opt.state & QStyle::State_Selected ? QPalette::HighlightedText : QPalette::Mid)); // Use Mid color for less emphasis
            painter->setFont(QFont(painter->font().family(), painter->font().pointSize() - 2));
            QRect categoryRect = textRect;
            categoryRect.setTop(categoryRect.top() + painter->fontMetrics().height() + 2); // Position below title
            painter->drawText(categoryRect, Qt::AlignTop | Qt::AlignLeft, category);
        }

        // Draw description (even smaller font, lighter color)
        if (!description.isEmpty()) {
            painter->setPen(opt.palette.color(opt.state & QStyle::State_Selected ? QPalette::HighlightedText : QPalette::ToolTipText)); // Use ToolTipText color for description
            painter->setFont(QFont(painter->font().family(), painter->font().pointSize() - 3));
            QRect descRect = textRect;
            descRect.setTop(descRect.top() + (painter->fontMetrics().height() * 2) + 4); // Position below category
            painter->drawText(descRect, Qt::AlignTop | Qt::AlignLeft, description);
        }

        // Optionally, draw icon if stored in Qt::DecorationRole
        if (opt.features & QStyleOptionViewItem::HasDisplay) {
             QIcon icon = qvariant_cast<QIcon>(index.data(Qt::DecorationRole));
             if (!icon.isNull()) {
                 QPixmap pixmap = icon.pixmap(QSize(16, 16)); // Standard small icon size
                 painter->drawPixmap(textRect.left(), textRect.center().y() - pixmap.height()/2, pixmap);
             }
        }
    }

    QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override
    {
        QStyleOptionViewItem opt(option);
        initStyleOption(&opt, index);

        QString title = opt.text;
        QString description = index.data(Qt::UserRole + 2).toString(); // Store description in a custom role

        QFontMetrics fm_title(QFont(opt.font.family(), opt.font.pointSize(), QFont::Bold));
        QFontMetrics fm_desc(QFont(opt.font.family(), opt.font.pointSize() - 3));

        int height = fm_title.height() + 2; // Height for title
        if (!description.isEmpty()) {
            height += fm_desc.height() + 2; // Add height for description
        }
        height += 10; // Add some padding

        return QSize(opt.rect.width(), height);
    }
};

class CommandPalette::Private {
public:
    Private(CommandPalette* q_ptr)
        : q(q_ptr), maxResultsVal(20), closeOnExecuteVal(true) {}

    CommandPalette* q;
    QLineEdit* searchLineEdit;
    QListWidget* resultsListWidget;
    QLabel* placeholderLabel;
    QStringListModel* listModel; // Model for the list widget
    QSortFilterProxyModel* proxyModel; // Proxy model for filtering
    QTimer* searchDebounceTimer; // Timer to debounce search as user types
    QList<Command> allCommands; // List of all registered commands
    QHash<QString, Command> commandMap; // Hash map for fast lookup by ID
    int maxResultsVal; // Maximum number of results to show
    bool closeOnExecuteVal; // Whether to close the palette after executing a command
    QString currentQueryStr; // Cache the current query string
    mutable QMutex mutex; // Protect access to the command list during concurrent access (e.g., registration from different threads)

    // Helper to filter commands based on the current query string and update the list model
    void filterAndPopulateList(const QString& query) {
        QMutexLocker locker(&mutex); // Lock during read of command list and filtering
        QStringList results;
        QList<Command> matchedCommands;

        if (query.isEmpty()) {
            // If query is empty, show top-level categories or most frequent commands, or just the first N commands
            // For simplicity, let's show the first N commands.
            int count = qMin(allCommands.size(), maxResultsVal);
            for (int i = 0; i < count; ++i) {
                const Command& cmd = allCommands[i];
                results.append(cmd.title);
                matchedCommands.append(cmd);
            }
        } else {
            // Simple fuzzy search across title, category, description
            QString lowerQuery = query.toLower();
            for (const auto& cmd : allCommands) {
                if (cmd.title.toLower().contains(lowerQuery) ||
                    cmd.category.toLower().contains(lowerQuery) ||
                    cmd.description.toLower().contains(lowerQuery)) {
                    matchedCommands.append(cmd);
                }
            }

            // Sort results: prioritize by priority, then by title
            std::sort(matchedCommands.begin(), matchedCommands.end(), [](const Command& a, const Command& b) {
                if (a.priority != b.priority) {
                    return a.priority > b.priority; // Higher priority first
                }
                return a.title.localeAwareCompare(b.title) < 0; // Alphabetical for same priority
            });

            // Take up to maxResults
            int count = qMin(matchedCommands.size(), maxResultsVal);
            for (int i = 0; i < count; ++i) {
                results.append(matchedCommands[i].title);
            }
        }

        // Update the source model
        listModel->setStringList(results);

        // Update the internal list of *matched* commands for execution mapping
        currentFilteredCommands = matchedCommands;

        // Update placeholder visibility
        placeholderLabel->setVisible(results.isEmpty());
        resultsListWidget->setVisible(!results.isEmpty());

        emit q->resultsChanged(results.size());
        LOG_DEBUG("CommandPalette: Filtered to " << results.size() << " commands for query: '" << query << "'");
    }

    // List of currently filtered commands (subset of allCommands matching the query)
    QList<Command> currentFilteredCommands;
};

// Static instance pointer
CommandPalette* CommandPalette::s_instance = nullptr;

CommandPalette& CommandPalette::instance()
{
    if (!s_instance) {
        s_instance = new CommandPalette();
    }
    return *s_instance;
}

CommandPalette::CommandPalette(QWidget* parent)
    : QWidget(parent, Qt::Popup | Qt::FramelessWindowHint) // Make it a popup/floating window without a frame
    , d(new Private(this))
{
    setAttribute(Qt::WA_TranslucentBackground); // Allow custom background painting if desired

    // Main layout
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(5, 5, 5, 5); // Small margin around the entire widget

    // Search input
    d->searchLineEdit = new QLineEdit(this);
    d->searchLineEdit->setPlaceholderText(tr("Type a command..."));
    d->searchLineEdit->setClearButtonEnabled(true);
    d->searchLineEdit->setFocus(); // Start with focus on the search box
    mainLayout->addWidget(d->searchLineEdit);

    // Model and Proxy Model for filtering
    d->listModel = new QStringListModel(this);
    d->proxyModel = new QSortFilterProxyModel(this);
    d->proxyModel->setSourceModel(d->listModel);
    // d->proxyModel->setFilterCaseSensitivity(Qt::CaseInsensitive); // We filter manually
    // d->proxyModel->setFilterKeyColumn(-1); // Filter all columns

    // Results list
    d->resultsListWidget = new QListWidget(this);
    d->resultsListWidget->setModel(d->proxyModel); // Set the proxy model
    d->resultsListWidget->setItemDelegate(new CommandPaletteDelegate(this)); // Use custom delegate for styling
    d->resultsListWidget->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    d->resultsListWidget->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    d->resultsListWidget->setSelectionMode(QAbstractItemView::SingleSelection);
    d->resultsListWidget->setSelectionBehavior(QAbstractItemView::SelectRows);
    d->resultsListWidget->setFrameStyle(QFrame::NoFrame); // Remove default frame if desired
    d->resultsListWidget->setSpacing(2); // Small space between items
    mainLayout->addWidget(d->resultsListWidget);

    // Placeholder label (shown when no results match)
    d->placeholderLabel = new QLabel(tr("No commands found"), this);
    d->placeholderLabel->setAlignment(Qt::AlignCenter);
    d->placeholderLabel->setStyleSheet("QLabel { color: gray; }"); // Style the placeholder
    mainLayout->addWidget(d->placeholderLabel);

    // Debounce timer for search
    d->searchDebounceTimer = new QTimer(this);
    d->searchDebounceTimer->setSingleShot(true);
    connect(d->searchDebounceTimer, &QTimer::timeout, [this]() {
        d->filterAndPopulateList(d->searchLineEdit->text());
    });

    // Connect search box changes
    connect(d->searchLineEdit, &QLineEdit::textChanged, [this](const QString& text) {
        d->currentQueryStr = text;
        d->searchDebounceTimer->start(150); // Wait 150ms after user stops typing
        emit queryChanged(text);
    });

    // Connect list item activation (click or Enter when item is selected)
    connect(d->resultsListWidget, &QListWidget::activated, [this](const QModelIndex& index) {
        int sourceRow = d->proxyModel->mapToSource(index).row();
        if (sourceRow >= 0 && sourceRow < d->currentFilteredCommands.size()) {
            const Command& cmd = d->currentFilteredCommands[sourceRow];
            executeCommand(cmd);
        }
    });

    // Connect Enter key press in search box to activate selected item or the first result if none selected
    connect(d->searchLineEdit, &QLineEdit::returnPressed, [this]() {
        if (d->resultsListWidget->currentIndex().isValid()) {
            // Activate the currently selected item in the list
            d->resultsListWidget->activated(d->resultsListWidget->currentIndex());
        } else if (!d->currentFilteredCommands.isEmpty()) {
            // If no item is selected, execute the first result if available
            executeCommand(d->currentFilteredCommands[0]);
        }
    });

    // Connect Escape key to hide the palette
    connect(d->searchLineEdit, &QLineEdit::keyPressEvent, [this](QKeyEvent* event) {
        if (event->key() == Qt::Key_Escape) {
            hidePalette();
            event->accept();
        } else {
            // Let the line edit handle other keys normally
            QLineEdit::keyPressEvent(event);
        }
    });

    // Initially hide the placeholder, show the list
    d->placeholderLabel->hide();

    LOG_INFO("CommandPalette initialized.");
}

CommandPalette::~CommandPalette()
{
    LOG_INFO("CommandPalette destroyed.");
}

void CommandPalette::showPalette()
{
    // Center on parent or primary screen
    QWidget* parentWidget = parentWidget();
    QRect screenGeometry;
    if (parentWidget) {
        screenGeometry = parentWidget->screen()->availableGeometry();
    } else {
        screenGeometry = QApplication::primaryScreen()->availableGeometry();
    }

    // Calculate size based on screen size (e.g., 40% width, 30% height, max 600x400)
    int maxWidth = qMin(600, screenGeometry.width() * 0.4);
    int maxHeight = qMin(400, screenGeometry.height() * 0.3);
    resize(maxWidth, maxHeight);

    int x = screenGeometry.center().x() - width() / 2;
    int y = screenGeometry.center().y() - height() / 2;
    move(x, y);

    show();
    d->searchLineEdit->setFocus();
    d->searchLineEdit->selectAll(); // Select all text for easy replacement
    emit paletteShown();
    LOG_DEBUG("CommandPalette shown.");
}

void CommandPalette::hidePalette()
{
    hide();
    emit paletteHidden();
    LOG_DEBUG("CommandPalette hidden.");
}

bool CommandPalette::isShown() const
{
    return isVisible(); // isVisible is inherited from QWidget
}

void CommandPalette::addCommand(const QString& id, const QString& title, const QString& category, const QString& description, const QString& shortcut, std::function<void()> handler, const QIcon& icon, int priority)
{
    QMutexLocker locker(&d->mutex);

    // Check for duplicates by ID
    auto existingCmdIt = std::find_if(d->allCommands.begin(), d->allCommands.end(),
                                      [&id](const Command& cmd) { return cmd.id == id; });
    if (existingCmdIt != d->allCommands.end()) {
        LOG_WARN("CommandPalette::addCommand: Command with ID already exists, overwriting: " << id);
        *existingCmdIt = Command{id, title, category, description, shortcut, std::move(handler), icon, priority};
    } else {
        d->allCommands.append(Command{id, title, category, description, shortcut, std::move(handler), icon, priority});
        d->commandMap.insert(id, d->allCommands.back()); // Update map
    }

    // If the palette is currently visible and the query matches, update the list
    if (isVisible() && d->currentQueryStr.isEmpty()) { // Simple update if query is empty, otherwise rely on next filter call
        d->filterAndPopulateList(d->currentQueryStr);
    }

    LOG_DEBUG("CommandPalette: Added command '" << title << "' (ID: " << id << ")");
}

void CommandPalette::removeCommand(const QString& id)
{
    QMutexLocker locker(&d->mutex);

    auto it = std::find_if(d->allCommands.begin(), d->allCommands.end(),
                           [&id](const Command& cmd) { return cmd.id == id; });
    if (it != d->allCommands.end()) {
        QString title = it->title; // Store title before erasing
        d->commandMap.remove(id); // Remove from map
        d->allCommands.erase(it); // Remove from list

        // Update the display if visible
        if (isVisible()) {
            d->filterAndPopulateList(d->searchLineEdit->text()); // Re-filter with current text
        }

        LOG_DEBUG("CommandPalette: Removed command '" << title << "' (ID: " << id << ")");
    } else {
        LOG_WARN("CommandPalette::removeCommand: Command ID not found: " << id);
    }
}

QStringList CommandPalette::commandIds() const
{
    QMutexLocker locker(&d->mutex);
    return d->commandMap.keys();
}

int CommandPalette::maxResults() const
{
    QMutexLocker locker(&d->mutex);
    return d->maxResultsVal;
}

void CommandPalette::setMaxResults(int max)
{
    if (max > 0) {
        QMutexLocker locker(&d->mutex);
        if (d->maxResultsVal != max) {
            d->maxResultsVal = max;
            LOG_INFO("CommandPalette: Max results set to " << max);

            // Update the display if visible
            if (isVisible()) {
                d->filterAndPopulateList(d->searchLineEdit->text()); // Re-filter with new limit
            }
        }
    }
}

bool CommandPalette::closeOnExecute() const
{
    QMutexLocker locker(&d->mutex);
    return d->closeOnExecuteVal;
}

void CommandPalette::setCloseOnExecute(bool close)
{
    QMutexLocker locker(&d->mutex);
    if (d->closeOnExecuteVal != close) {
        d->closeOnExecuteVal = close;
        LOG_INFO("CommandPalette: Close on execute set to " << close);
    }
}

void CommandPalette::executeCommand(const Command& cmd)
{
    LOG_INFO("CommandPalette: Executing command '" << cmd.title << "' (ID: " << cmd.id << ")");
    if (cmd.handler) {
        cmd.handler(); // Execute the associated function
    }
    emit commandExecuted(cmd.id);

    if (d->closeOnExecuteVal) {
        hidePalette(); // Close the palette after execution if configured
    }
}

} // namespace QuantilyxDoc