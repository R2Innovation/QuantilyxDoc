/**
 * QuantilyxDoc - Professional Document Editor
 * Copyright (C) 2025 R² Innovative Software
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */
#ifndef QUANTILYX_FULLTEXTINDEX_H
#define QUANTILYX_FULLTEXTINDEX_H

#include <QObject>
#include <QHash>
#include <QMultiHash>
#include <QMap>
#include <QSet>
#include <QMutex>
#include <QFuture>
#include <memory>
#include <functional>

namespace QuantilyxDoc {

class Document; // Forward declaration

/**
 * @brief Structure holding information about a search result hit.
 */
struct SearchResult {
    Document* document;     // Pointer to the document containing the hit
    int pageIndex;          // Page index where the hit occurred (-1 if not page-specific)
    QString text;           // The matching text snippet
    QString context;        // Context surrounding the match
    float score;            // Relevance score (higher is more relevant)
    // Add more fields like hit position, page number, etc.
};

/**
 * @brief Builds and queries a full-text search index across multiple documents.
 * 
 * Uses an underlying indexing library (e.g., CLucene, Xapian, SQLite FTS) or a custom implementation.
 */
class FullTextIndex : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief Constructor.
     * @param parent Parent object.
     */
    explicit FullTextIndex(QObject* parent = nullptr);

    /**
     * @brief Destructor.
     */
    ~FullTextIndex() override;

    /**
     * @brief Get singleton instance.
     * @return Reference to the global FullTextIndex instance.
     */
    static FullTextIndex& instance();

    /**
     * @brief Initialize the index.
     * Opens the index database/file.
     * @param indexPath Path to the index storage location.
     * @return True if initialization was successful.
     */
    bool initialize(const QString& indexPath = QString());

    /**
     * @brief Check if the index is initialized and ready.
     * @return True if ready.
     */
    bool isReady() const;

    /**
     * @brief Add a document's content to the index.
     * @param document The document to index.
     * @return True if indexing was successful.
     */
    bool addDocument(Document* document);

    /**
     * @brief Remove a document's content from the index.
     * @param document The document to remove from the index.
     * @return True if removal was successful.
     */
    bool removeDocument(Document* document);

    /**
     * @brief Update the index for a document if its content has changed.
     * @param document The document to update in the index.
     * @return True if update was successful.
     */
    bool updateDocument(Document* document);

    /**
     * @brief Query the index for a specific term or phrase.
     * @param query The search query string.
     * @param maxResults Maximum number of results to return.
     * @param contextLength Number of characters of context to include around the match.
     * @return List of search results.
     */
    QList<SearchResult> query(const QString& query, int maxResults = 50, int contextLength = 100) const;

    /**
     * @brief Query the index asynchronously.
     * @param query The search query string.
     * @param maxResults Maximum number of results to return.
     * @param contextLength Number of characters of context to include around the match.
     * @return A QFuture that will hold the list of search results upon completion.
     */
    QFuture<QList<SearchResult>> queryAsync(const QString& query, int maxResults = 50, int contextLength = 100) const;

    /**
     * @brief Get the total number of documents indexed.
     * @return Document count.
     */
    int documentCount() const;

    /**
     * @brief Get the total number of terms indexed.
     * @return Term count.
     */
    int termCount() const;

    /**
     * @brief Commit pending changes to the index.
     * Ensures all added/removed/updated documents are written to storage.
     */
    void commit();

    /**
     * @brief Optimize the index for performance.
     * This might involve merging segments (in Lucene-like indexes) or rebuilding (in custom indexes).
     */
    void optimize();

    /**
     * @brief Clear the entire index.
     */
    void clear();

    /**
     * @brief Get the path to the index storage.
     * @return Index path string.
     */
    QString indexPath() const;

signals:
    /**
     * @brief Emitted when indexing starts for a document.
     * @param document The document being indexed.
     */
    void indexingStarted(Document* document);

    /**
     * @brief Emitted when indexing finishes for a document.
     * @param document The document that was indexed.
     * @param success Whether indexing was successful.
     */
    void indexingFinished(Document* document, bool success);

    /**
     * @brief Emitted when a query is started.
     */
    void queryStarted();

    /**
     * @brief Emitted when a query finishes.
     * @param results The list of results returned by the query.
     */
    void queryFinished(const QList<QuantilyxDoc::SearchResult>& results);

    /**
     * @brief Emitted when the index content changes (document added/removed/updated).
     */
    void indexContentChanged();

private:
    class Private;
    std::unique_ptr<Private> d;
};

} // namespace QuantilyxDoc

#endif // QUANTILYX_FULLTEXTINDEX_H