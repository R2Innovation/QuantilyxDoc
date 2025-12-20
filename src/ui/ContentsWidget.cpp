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
#include <QApplication> // For accessing main window signals if needed
#include <QDebug>

namespace QuantilyxDoc {

ContentsWidget::ContentsWidget(QWidget* parent)
    : QWidget(parent)
    , m_document(nullptr)
{
    m_layout = new QVBoxLayout(this);
    m_noContentsLabel = new QLabel(tr("No table of contents available."), this);
    m_noContentsLabel->setAlignment(Qt::AlignCenter);
    m_treeWidget = new QTreeWidget(this);
    m_treeWidget->setHeaderLabel(tr("Contents"));
    m_treeWidget->setRootIsDecorated(true); // Show expand/collapse icons
    m_treeWidget->setAlternatingRowColors(true);
    m_treeWidget->setUniformRowHeights(true);

    m_layout->addWidget(m_noContentsLabel);
    m_layout->addWidget(m_treeWidget);
    m_treeWidget->hide(); // Hide initially, show when contents exist

    connect(m_treeWidget, &QTreeWidget::itemActivated, this, &ContentsWidget::onTocItemActivated);

    LOG_INFO("ContentsWidget initialized.");
}

ContentsWidget::~ContentsWidget()
{
    LOG_INFO("ContentsWidget destroyed.");
}

void ContentsWidget::setDocument(Document* doc)
{
    if (m_document == doc) return;

    // Disconnect from old document signals if necessary
    // (Not directly connected here, but maybe via MainWindow)

    m_document = doc; // Use QPointer

    if (doc) {
        // Update UI based on new document's TOC
        if (doc->hasTableOfContents()) {
            QVariantList toc = doc->tableOfContents();
            populateContents(toc);
        } else {
            clearContents();
            m_noContentsLabel->setText(tr("Document has no table of contents."));
        }
    } else {
        clearContents();
        m_noContentsLabel->setText(tr("No document loaded."));
    }
    LOG_DEBUG("ContentsWidget set to document: " << (doc ? doc->filePath() : "nullptr"));
}

Document* ContentsWidget::document() const
{
    return m_document.data(); // Returns nullptr if document was deleted
}

void ContentsWidget::onCurrentDocumentChanged(Document* doc)
{
    setDocument(doc);
}

void ContentsWidget::onTocItemActivated(QTreeWidgetItem* item, int column)
{
    Q_UNUSED(column);
    if (!item || !m_document) return;

    // The destination information should be stored in the item's data.
    // We populated it in populateTreeItem using Qt::UserRole.
    QVariant destination = item->data(0, Qt::UserRole);
    if (destination.isValid()) {
        LOG_DEBUG("ContentsWidget: Item activated, requesting navigation to: " << destination.toString());
        emit navigateRequested(destination);
    } else {
        LOG_WARN("ContentsWidget: Activated item has no destination data.");
    }
}

void ContentsWidget::clearContents()
{
    m_treeWidget->clear();
    m_noContentsLabel->show();
    m_treeWidget->hide();
    LOG_DEBUG("ContentsWidget cleared.");
}

void ContentsWidget::populateContents(const QVariantList& tocData)
{
    m_treeWidget->clear();

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
        m_treeWidget->expandAll(); // Expand the tree by default
    } else {
        m_noContentsLabel->setText(tr("Table of contents is empty."));
        m_noContentsLabel->show();
        m_treeWidget->hide();
    }
    LOG_DEBUG("ContentsWidget populated with " << m_treeWidget->topLevelItemCount() << " top-level items.");
}

void ContentsWidget::populateTreeItem(QTreeWidgetItem* parentItem, const QVariantMap& itemData)
{
    // Set title
    QString title = itemData.value("title", "Untitled").toString();
    parentItem->setText(0, title);

    // Store destination data in the item
    QVariant destination = itemData.value("destination");
    parentItem->setData(0, Qt::UserRole, destination);

    // Recursively add children
    QVariantList children = itemData.value("children", QVariantList()).toList();
    for (const auto& childVariant : children) {
        if (childVariant.type() == QVariant::Map) {
            QVariantMap childMap = childVariant.toMap();
            QTreeWidgetItem* childItem = new QTreeWidgetItem(parentItem);
            populateTreeItem(childItem, childMap);
        }
    }
}

} // namespace QuantilyxDoc