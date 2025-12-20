/**
 * QuantilyxDoc - Professional Document Editor
 * Copyright (C) 2025 R² Innovative Software
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */
#include "AboutDialog.h"
#include "../core/Application.h" // Assuming Application provides version info
#include "../core/Logger.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QDialogButtonBox>
#include <QLabel>
#include <QTextBrowser>
#include <QPushButton>
#include <QIcon>
#include <QDesktopServices>
#include <QUrl>
#include <QApplication>
#include <QTextStream>
#include <QFile>
#include <QDebug>

namespace QuantilyxDoc {

class AboutDialog::Private {
public:
    Private(AboutDialog* q_ptr) : q(q_ptr) {}

    AboutDialog* q;

    // UI Elements
    QLabel* logoLabel;
    QLabel* titleLabel;
    QLabel* versionLabel;
    QLabel* companyLabel;
    QLabel* mottoLabel;
    QLabel* descriptionLabel;
    QLabel* licenseLabel;
    QTextBrowser* licenseBrowser; // For scrolling license text
    QDialogButtonBox* buttonBox;
    QPushButton* okButton;
    QPushButton* creditsButton; // Optional button to show credits
    QPushButton* thirdPartyButton; // Optional button to show third-party licenses

    // Helper to create the main layout and widgets
    void createLayout();
    // Helper to populate text fields
    void populateText();
    // Helper to connect signals
    void connectSignals();
};

void AboutDialog::Private::createLayout() {
    // Main layout
    QVBoxLayout* mainLayout = new QVBoxLayout(q);

    // Top section: Logo and Title
    QHBoxLayout* topLayout = new QHBoxLayout();
    logoLabel = new QLabel(q);
    logoLabel->setPixmap(QIcon(":/images/QuantilyxDoc.png").pixmap(64, 64)); // Use a resource or path
    logoLabel->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    topLayout->addWidget(logoLabel);

    // Title, Version, Company, Motto, Description
    QVBoxLayout* textLayout = new QVBoxLayout();
    titleLabel = new QLabel(q);
    titleLabel->setTextFormat(Qt::RichText); // Allow HTML formatting
    titleLabel->setOpenExternalLinks(true); // For any links in the title
    titleLabel->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Minimum);
    textLayout->addWidget(titleLabel);

    versionLabel = new QLabel(q);
    versionLabel->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Minimum);
    textLayout->addWidget(versionLabel);

    companyLabel = new QLabel(q);
    companyLabel->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Minimum);
    textLayout->addWidget(companyLabel);

    mottoLabel = new QLabel(q);
    mottoLabel->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Minimum);
    mottoLabel->setStyleSheet("QLabel { font-style: italic; color: gray; }"); // Style the motto
    textLayout->addWidget(mottoLabel);

    descriptionLabel = new QLabel(q);
    descriptionLabel->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Minimum);
    descriptionLabel->setWordWrap(true); // Allow description to wrap
    textLayout->addWidget(descriptionLabel);

    topLayout->addLayout(textLayout);
    topLayout->addStretch(); // Push content to the left
    mainLayout->addLayout(topLayout);

    // License text (scrollable)
    licenseBrowser = new QTextBrowser(q);
    licenseBrowser->setOpenExternalLinks(true); // Allow links in license text
    licenseBrowser->setMaximumHeight(150); // Limit height, make it scrollable
    licenseBrowser->setReadOnly(true);
    mainLayout->addWidget(licenseBrowser);

    // Buttons
    buttonBox = new QDialogButtonBox(q);
    okButton = new QPushButton(tr("OK"), q);
    creditsButton = new QPushButton(tr("Credits"), q); // Optional
    thirdPartyButton = new QPushButton(tr("Licenses"), q); // Optional

    buttonBox->addButton(okButton, QDialogButtonBox::AcceptRole);
    buttonBox->addButton(creditsButton, QDialogButtonBox::ActionRole);
    buttonBox->addButton(thirdPartyButton, QDialogButtonBox::ActionRole);

    mainLayout->addWidget(buttonBox);

    // Set dialog properties
    q->setWindowTitle(tr("About QuantilyxDoc"));
    q->setModal(true); // Modal dialog
    q->setFixedSize(500, 400); // Fixed size for consistency, adjust as needed
}

void AboutDialog::Private::populateText() {
    // Title
    titleLabel->setText(tr("<h2>QuantilyxDoc</h2>"));

    // Version - Get from Application or a version header
    versionLabel->setText(tr("Version %1").arg(Application::version()));

    // Company
    companyLabel->setText(tr("<b>R² Innovative Software</b>"));

    // Motto
    mottoLabel->setText(tr("\"Where innovation is the key to success\""));

    // Description
    descriptionLabel->setText(tr("Professional document editor for liberation and productivity."));

    // License Text
    // This could be loaded from a file or embedded as a string.
    // For now, we'll embed a short GPL notice.
    QString licenseText = tr(
        "<h3>License</h3>"
        "<p>This program is free software: you can redistribute it and/or modify "
        "it under the terms of the GNU General Public License as published by "
        "the Free Software Foundation, either version 3 of the License, or "
        "(at your option) any later version.</p>"
        "<p>This program is distributed in the hope that it will be useful, "
        "but WITHOUT ANY WARRANTY; without even the implied warranty of "
        "MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the "
        "GNU General Public License for more details.</p>"
        "<p>You should have received a copy of the GNU General Public License "
        "along with this program.  If not, see "
        "<a href='https://www.gnu.org/licenses/gpl-3.0.html'>https://www.gnu.org/licenses/gpl-3.0.html</a>.</p>"
        "<h3>Third-Party Libraries</h3>"
        "<p>QuantilyxDoc uses the following third-party libraries:</p>"
        "<ul>"
        "<li>Qt Framework (LGPL v2.1)</li>"
        "<li>Poppler (GPL v2 or later) - for PDF handling</li>"
        "<li>OpenSSL (OpenSSL License) - for cryptographic features</li>"
        "<li>Other libraries...</li>" // Add more as needed
        "</ul>"
        "<p>Specific license details for each library are included with the source code.</p>"
    );
    licenseBrowser->setHtml(licenseText);
}

void AboutDialog::Private::connectSignals() {
    // Connect the OK button to accept the dialog
    connect(okButton, &QPushButton::clicked, q, &QDialog::accept);

    // Connect optional buttons (stubs)
    connect(creditsButton, &QPushButton::clicked, [this]() {
        // Show a simple message box or another dialog with credits
        LOG_INFO("AboutDialog: Credits button clicked.");
        // QMessageBox::information(q, tr("Credits"), tr("Contributors and translators..."));
    });

    connect(thirdPartyButton, &QPushButton::clicked, [this]() {
        // Show a dialog or expand the text browser to show third-party licenses
        LOG_INFO("AboutDialog: Third-party licenses button clicked.");
        // QMessageBox::information(q, tr("Licenses"), tr("Licenses for third-party libraries..."));
    });
}

AboutDialog::AboutDialog(QWidget* parent)
    : QDialog(parent)
    , d(new Private(this))
{
    d->createLayout();
    d->populateText();
    d->connectSignals();

    LOG_INFO("AboutDialog initialized.");
}

AboutDialog::~AboutDialog()
{
    LOG_INFO("AboutDialog destroyed.");
}

} // namespace QuantilyxDoc