/**
 * QuantilyxDoc - Professional Document Editor
 * Copyright (C) 2025 R² Innovative Software
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */
#ifndef QUANTILYX_CONTENTSWIDGET_H
#define QUANTILYX_CONTENTSWIDGET_H

#include <QWidget>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <QLabel>
#include <QPointer>
#include <QVariantList>

namespace QuantilyxDoc {

class Document;

/**
 * @brief Widget for displaying document contents like Table of Contents or Bookmarks.
 * 
 * Shows a hierarchical list of document sections and allows navigation.
 */
class ContentsWidget : public QWidget
{
    Q_OBJECT

public:
    /**
     * @brief Constructor.
     * @param parent Parent widget.
     */
    explicit ContentsWidget(QWidget* parent = nullptr);

    /**
     * @brief Destructor.
     */
    ~ContentsWidget() override;

    /**
     * @brief Set the document whose contents should be displayed.
     * @param doc The document, or nullptr to clear.
     */
    void setDocument(Document* doc);

    /**
     * @brief Get the currently associated document.
     * @return Pointer to the document, or nullptr.
     */
    Document* document() const;

signals:
    /**
     * @brief Emitted when a TOC item is clicked/activated.
     * @param destination Information about where to navigate (e.g., page index, coordinates).
     */
    void navigateRequested(const QVariant& destination);

private slots:
    void onCurrentDocumentChanged(Document* doc);
    void onTocItemActivated(QTreeWidgetItem* item, int column);

private:
    QPointer<Document> m_document; // Use QPointer for safety
    QVBoxLayout* m_layout;
    QLabel* m_noContentsLabel;
    QTreeWidget* m_treeWidget;

    void clearContents();
    void populateContents(const QVariantList& tocData);
    void populateTreeItem(QTreeWidgetItem* parentItem, const QVariantMap& itemData);
};

} // namespace QuantilyxDoc

#endif // QUANTILYX_CONTENTSWIDGET_H