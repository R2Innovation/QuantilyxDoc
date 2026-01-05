/**
 * QuantilyxDoc - Professional Document Editor
 * Copyright (C) 2025 R² Innovative Software
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */
#include "ContentsWidget.h"
#include "../core/Document.h"
#include "../core/Logger.h"
#include <QVBoxLayout>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QLabel>
#include <QHeaderView>
#include <QApplication>
#include <QDesktopWidget>
#include <QUrl>
#include <QDesktopServices>
#include <QDebug>

namespace QuantilyxDoc {

ContentsWidget::ContentsWidget(QWidget* parent)
    : QWidget(parent)
    , m_document(nullptr)
{
    m_layout = new QVBoxLayout(this);
    m_layout->setContentsMargins(2, 2, 2, 2); // Tighter margins for dock widget

    // Label shown when no TOC is available
    m_noContentsLabel = new QLabel(tr("No table of contents available."), this);
    m_noContentsLabel->setAlignment(Qt::AlignCenter);
    m_noContentsLabel->setWordWrap(true);
    m_layout->addWidget(m_noContentsLabel);

    // Tree widget for TOC
    m_treeWidget = new QTreeWidget(this);
    m_treeWidget->setHeaderLabel(tr("Table of Contents"));
    m_treeWidget->setRootIsDecorated(true); // Show +/- icons for collapsible items
    m_treeWidget->setAlternatingRowColors(true);
    m_treeWidget->setUniformRowHeights(true);
    m_treeWidget->setSortingEnabled(false); // Keep the order from the document
    m_layout->addWidget(m_treeWidget);

    // Initially hide the tree, show the label
    m_treeWidget->hide();

    // Connect double-click/activation signal
    connect(m_treeWidget, &QTreeWidget::itemActivated, this, &ContentsWidget::onTocItemActivated);

    LOG_INFO("ContentsWidget initialized.");
}

ContentsWidget::~ContentsWidget()
{
    LOG_INFO("ContentsWidget destroyed.");
}

void ContentsWidget::setDocument(Document* doc)
{
    if (m_document == doc) return; // No change

    // Disconnect from old document if necessary
    if (m_document) {
        // disconnect(m_document, ...); // If we had specific document signals to connect to
    }

    m_document = doc; // Use QPointer

    if (doc) {
        LOG_DEBUG("ContentsWidget: Setting document to " << doc->filePath());
        if (doc->hasTableOfContents()) {
            QVariantList tocData = doc->tableOfContents();
            populateContents(tocData);
        } else {
            LOG_DEBUG("ContentsWidget: Document has no TOC, clearing display.");
            clearContents();
            m_noContentsLabel->setText(tr("Document has no table of contents."));
        }
    } else {
        LOG_DEBUG("ContentsWidget: Clearing document.");
        clearContents();
        m_noContentsLabel->setText(tr("No document loaded."));
    }
}

Document* ContentsWidget::document() const
{
    return m_document.data(); // Returns nullptr if document was deleted
}

void ContentsWidget::onCurrentDocumentChanged(Document* doc)
{
    setDocument(doc); // Convenience slot for connecting to main window's signal
}

void ContentsWidget::onTocItemActivated(QTreeWidgetItem* item, int column)
{
    Q_UNUSED(column); // Usually only one column (title)
    if (!item || !m_document) return;

    // The destination information should be stored in the item's data.
    // Let's assume it's stored in the UserRole (role 1) as a QVariantMap or QString.
    QVariant destinationData = item->data(0, Qt::UserRole);
    if (destinationData.isValid()) {
        LOG_DEBUG("ContentsWidget: Item activated, destination data: " << destinationData.toString());
        emit navigateRequested(destinationData);
    } else {
        LOG_WARN("ContentsWidget: Activated TOC item has no destination data attached.");
    }
}

void ContentsWidget::clearContents()
{
    m_treeWidget->clear();
    m_noContentsLabel->show();
    m_treeWidget->hide();
    LOG_DEBUG("ContentsWidget: Cleared contents display.");
}

void ContentsWidget::populateContents(const QVariantList& tocData)
{
    m_treeWidget->clear(); // Clear existing items

    for (const auto& itemVariant : tocData) {
        if (itemVariant.type() == QVariant::Map) {
            QVariantMap itemMap = itemVariant.toMap();
            QTreeWidgetItem* topLevelItem = new QTreeWidgetItem(m_treeWidget);
            populateTreeItem(topLevelItem, itemMap);
        }
    }

    if (m_treeWidget->topLevelItemCount() > 0) {
        m_noContentsLabel->hide();
        m_treeWidget->show();
        m_treeWidget->expandToDepth(2); // Expand top 3 levels by default for usability
    } else {
        m_noContentsLabel->setText(tr("Table of contents is empty."));
        m_noContentsLabel->show();
        m_treeWidget->hide();
    }

    LOG_DEBUG("ContentsWidget: Populated with " << m_treeWidget->topLevelItemCount() << " top-level TOC items.");
}

void ContentsWidget::populateTreeItem(QTreeWidgetItem* parentItem, const QVariantMap& itemData)
{
    // Set the title text for the item
    QString title = itemData.value("title", "Untitled").toString();
    parentItem->setText(0, title);

    // Store the destination data (e.g., page number, coordinates, named destination string) in the item's user data
    // This is what will be emitted via navigateRequested.
    QVariant destination = itemData.value("destination"); // e.g., {"type": "page", "page": 5} or a string like "/Page 5 /XYZ 100 700 null"
    parentItem->setData(0, Qt::UserRole, destination);

    // Recursively add child items if they exist
    QVariantList childrenList = itemData.value("children", QVariantList()).toList();
    for (const auto& childVariant : childrenList) {
        if (childVariant.type() == QVariant::Map) {
            QVariantMap childMap = childVariant.toMap();
            QTreeWidgetItem* childItem = new QTreeWidgetItem(parentItem);
            populateTreeItem(childItem, childMap);
        }
    }
}

} // namespace QuantilyxDoc