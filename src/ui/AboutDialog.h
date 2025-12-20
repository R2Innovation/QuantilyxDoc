/**
 * QuantilyxDoc - Professional Document Editor
 * Copyright (C) 2025 R² Innovative Software
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */
#ifndef QUANTILYX_ABOUTDIALOG_H
#define QUANTILYX_ABOUTDIALOG_H

#include <QDialog>
#include <memory>

namespace QuantilyxDoc {

/**
 * @brief Dialog displaying application information, version, license, and credits.
 *
 * Shows the application name, logo, version, copyright, license notice,
 * and potentially links to third-party libraries used.
 */
class AboutDialog : public QDialog
{
    Q_OBJECT

public:
    /**
     * @brief Constructor.
     * @param parent Parent widget.
     */
    explicit AboutDialog(QWidget* parent = nullptr);

    /**
     * @brief Destructor.
     */
    ~AboutDialog() override;

private:
    class Private;
    std::unique_ptr<Private> d;
};

} // namespace QuantilyxDoc

#endif // QUANTILYX_ABOUTDIALOG_H