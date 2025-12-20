/**
 * QuantilyxDoc - Professional Document Editor
 * Copyright (C) 2025 R² Innovative Software
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */
#ifndef QUANTILYX_COMMANDPALETTE_H
#define QUANTILYX_COMMANDPALETTE_H

#include <QWidget>
#include <QLineEdit>
#include <QListWidget>
#include <QLabel>
#include <QTimer>
#include <QCompleter>
#include <QStringListModel>
#include <functional>
#include <memory>

namespace QuantilyxDoc {

/**
 * @brief A searchable command palette for quick access to application features.
 *
 * Provides a popup window with a search box that filters a list of available commands.
 * Commands can be registered dynamically and are sorted by relevance/frequency.
 */
class CommandPalette : public QWidget
{
    Q_OBJECT

public:
    /**
     * @brief Constructor.
     * @param parent Parent widget (usually the MainWindow).
     */
    explicit CommandPalette(QWidget* parent = nullptr);

    /**
     * @brief Destructor.
     */
    ~CommandPalette() override;

    /**
     * @brief Show the command palette centered over the parent widget.
     * @param parentWidget The widget to center over.
     */
    void showAtCenter(QWidget* parentWidget);

    /**
     * @brief Add a command to the palette.
     * @param id Unique identifier for the command.
     * @param title Display title for the command.
     * @param category Category for grouping/filtering (e.g., "File", "Edit").
     * @param description Detailed description shown as tooltip.
     * @param shortcut Keyboard shortcut string (e.g., "Ctrl+S").
     * @param handler Function to execute when the command is selected.
     * @param icon Optional icon for the command.
     * @param priority Priority for sorting (higher numbers appear first).
     */
    void addCommand(const QString& id, const QString& title, const QString& category,
                    const QString& description, const QString& shortcut,
                    std::function<void()> handler,
                    const QIcon& icon = QIcon(), int priority = 0);

    /**
     * @brief Remove a command from the palette.
     * @param id The ID of the command to remove.
     */
    void removeCommand(const QString& id);

    /**
     * @brief Get the list of all registered command IDs.
     * @return List of command IDs.
     */
    QStringList commandIds() const;

    /**
     * @brief Set the delay (in milliseconds) before filtering results after text input.
     * @param delayMs Delay in milliseconds.
     */
    void setSearchDelay(int delayMs);

    /**
     * @brief Get the current search delay.
     * @return Delay in milliseconds.
     */
    int searchDelay() const;

signals:
    /**
     * @brief Emitted when a command is executed from the palette.
     * @param commandId The ID of the executed command.
     */
    void commandExecuted(const QString& commandId);

protected:
    /**
     * @brief Handle show event to set focus.
     * @param event Show event.
     */
    void showEvent(QShowEvent* event) override;

    /**
     * @brief Handle key press events for navigation and closing.
     * @param event Key press event.
     */
    void keyPressEvent(QKeyEvent* event) override;

    /**
     * @brief Paint event for custom appearance (e.g., rounded corners).
     * @param event Paint event.
     */
    void paintEvent(QPaintEvent* event) override;

private:
    class Private;
    std::unique_ptr<Private> d;
};

} // namespace QuantilyxDoc

#endif // QUANTILYX_COMMANDPALETTE_H