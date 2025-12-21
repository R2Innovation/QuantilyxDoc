/**
 * QuantilyxDoc - Professional Document Editor
 * Copyright (C) 2025 R² Innovative Software
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */
#include "FullTextIndex.h"
#include "../core/Document.h"
#include "../core/Logger.h"
#include <QHash>
#include <QMultiHash>
#include <QMap>
#include <QSet>
#include <QMutex>
#include <QFuture>
#include <QtConcurrent/QtConcurrent>
#include <QDir>
#include <QStandardPaths>
#include <QTextStream>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QDebug>

namespace QuantilyxDoc {

// For this example, we'll implement a *very basic* in-memory index using Qt containers.
// This is NOT suitable for large corpora or production use. A proper solution would use
// CLucene, Xapian, or SQLite FTS.

struct IndexedDocumentData {
    QString fullText; // Cached full text for quick access during queries
    QHash<QString, QList<int>> wordPositions; // Word -> List of character positions in fullText
    // Could also store page-specific text mappings if needed
};

class FullTextIndex::Private {
public:
    Private(FullTextIndex* q_ptr)
        : q(q_ptr), ready(false) {}

    FullTextIndex* q;
    mutable QMutex mutex; // Protect access to the index maps
    bool ready;
    QString indexPathStr;
    QHash<Document*, IndexedDocumentData> docIndex; // Map Document* to its indexed data
    QMultiHash<QString, Document*> wordToDocs; // Map Word -> Set of Documents containing it
    QSet<Document*> indexedDocuments; // Track which documents are indexed

    // Helper to tokenize text (simple split by whitespace/punctuation)
    QStringList tokenizeText(const QString& text) const {
        QRegularExpression re(R"([\W]+)"); // Split on non-word characters
        QStringList tokens = text.split(re, Qt::SkipEmptyParts);
        // Convert to lowercase for case-insensitive indexing
        for (auto& token : tokens) {
            token = token.toLower();
        }
        return tokens;
    }

    // Helper to index a single document's text
    void indexDocumentText(Document* doc, const QString& text) {
        IndexedDocumentData data;
        data.fullText = text;
        QStringList tokens = tokenizeText(text);

        QHash<QString, QSet<int>> uniquePositions; // Use QSet to avoid duplicate positions for a word in a doc
        for (int i = 0; i < tokens.size(); ++i) {
            QString word = tokens[i];
            int posInText = text.indexOf(word, uniquePositions[word].isEmpty() ? 0 : uniquePositions[word].last() + 1, Qt::CaseInsensitive);
            if (posInText != -1) {
                 uniquePositions[word].insert(posInText);
            }
        }

        // Convert QSet to QList for storage
        for (auto it = uniquePositions.begin(); it != uniquePositions.end(); ++it) {
            data.wordPositions.insert(it.key(), it.value().values());
            // Update global word-to-docs map
            wordToDocs.insert(it.key(), doc);
        }

        docIndex.insert(doc, data);
        indexedDocuments.insert(doc);
        LOG_DEBUG("FullTextIndex: Indexed document '" << doc->title() << "' with " << tokens.size() << " tokens.");
    }
};

// Static instance pointer
FullTextIndex* FullTextIndex::s_instance = nullptr;

FullTextIndex& FullTextIndex::instance()
{
    if (!s_instance) {
        s_instance = new FullTextIndex();
    }
    return *s_instance;
}

FullTextIndex::FullTextIndex(QObject* parent)
    : QObject(parent)
    , d(new Private(this))
{
    LOG_INFO("FullTextIndex created.");
}

FullTextIndex::~FullTextIndex()
{
    LOG_INFO("FullTextIndex destroyed.");
}

bool FullTextIndex::initialize(const QString& indexPath)
{
    QMutexLocker locker(&d->mutex);

    // For a basic in-memory index, initialization just means setting the path and marking ready.
    // For a disk-based index (Lucene/Xapian/SQLite), this would open the database/index files.
    d->indexPathStr = indexPath.isEmpty() ? QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/search_index" : indexPath;
    QDir().mkpath(d->indexPathStr); // Ensure directory exists if using disk storage later

    d->ready = true;
    LOG_INFO("FullTextIndex: Initialized at path: " << d->indexPathStr);
    return true;
}

bool FullTextIndex::isReady() const
{
    QMutexLocker locker(&d->mutex);
    return d->ready;
}

bool FullTextIndex::addDocument(Document* document)
{
    if (!isReady() || !document) return false;

    QMutexLocker locker(&d->mutex);

    // Check if already indexed
    if (d->indexedDocuments.contains(document)) {
        LOG_WARN("FullTextIndex: Document '" << document->title() << "' is already indexed.");
        return true; // Or maybe call updateDocument instead?
    }

    emit indexingStarted(document);

    // Get full text from document. This might involve calling OCR if text is not available.
    // For now, assume Document has a method to get all text from all pages.
    // QString fullText = document->getAllText(); // Hypothetical method

    // For this basic example, let's assume we have the text.
    // In a real implementation, you'd iterate pages, get text (potentially via OCRPage),
    // and concatenate it.
    QString fullText = "Sample text content from document " + document->title(); // Placeholder

    d->indexDocumentText(document, fullText);

    emit indexingFinished(document, true);
    emit indexContentChanged();
    LOG_DEBUG("FullTextIndex: Added document '" << document->title() << "' to index.");
    return true;
}

bool FullTextIndex::removeDocument(Document* document)
{
    if (!isReady() || !document) return false;

    QMutexLocker locker(&d->mutex);

    if (!d->indexedDocuments.contains(document)) {
        LOG_WARN("FullTextIndex: Attempted to remove non-indexed document '" << document->title() << "'");
        return false;
    }

    // Remove from docIndex
    d->docIndex.remove(document);

    // Remove from wordToDocs (iterate and remove entries pointing to this doc)
    auto it = d->wordToDocs.begin();
    while (it != d->wordToDocs.end()) {
        if (it.value() == document) {
            it = d->wordToDocs.erase(it);
        } else {
            ++it;
        }
    }

    // Remove from set
    d->indexedDocuments.remove(document);

    emit indexContentChanged();
    LOG_DEBUG("FullTextIndex: Removed document '" << document->title() << "' from index.");
    return true;
}

bool FullTextIndex::updateDocument(Document* document)
{
    // For a basic index, updating means removing and re-adding.
    // More sophisticated indexes might have incremental update mechanisms.
    bool removed = removeDocument(document);
    if (removed) {
        return addDocument(document);
    }
    return false;
}

QList<SearchResult> FullTextIndex::query(const QString& query, int maxResults, int contextLength) const
{
    if (!isReady() || query.isEmpty()) return {};

    QMutexLocker locker(&d->mutex);

    emit queryStarted();

    QStringList queryTokens = d->tokenizeText(query);
    QList<SearchResult> results;
    QHash<Document*, int> docScores; // Document -> Score (number of query terms matched)

    // Simple scoring: count matching terms
    for (const QString& token : queryTokens) {
        auto range = d->wordToDocs.equal_range(token);
        for (auto it = range.first; it != range.second; ++it) {
            Document* doc = it.value();
            docScores[doc]++;
        }
    }

    // Sort documents by score (descending)
    QList<QPair<Document*, int>> scoredDocs = docScores.toList();
    std::sort(scoredDocs.begin(), scoredDocs.end(), [](const QPair<Document*, int>& a, const QPair<Document*, int>& b) {
        return a.second > b.second; // Higher score first
    });

    // Extract text snippets for top results
    int count = 0;
    for (const auto& pair : scoredDocs) {
        if (count >= maxResults) break;

        Document* doc = pair.first;
        const IndexedDocumentData& data = d->docIndex[doc]; // Safe because docScores came from wordToDocs

        // Find the first occurrence of any query token in the document's text
        int bestPos = -1;
        QString bestMatch;
        for (const QString& token : queryTokens) {
            int pos = data.fullText.indexOf(token, 0, Qt::CaseInsensitive);
            if (pos != -1 && (bestPos == -1 || pos < bestPos)) {
                bestPos = pos;
                bestMatch = token;
            }
        }

        if (bestPos != -1) {
            // Extract context
            int start = qMax(0, bestPos - contextLength / 2);
            int end = qMin(data.fullText.size(), bestPos + bestMatch.size() + contextLength / 2);
            QString context = data.fullText.mid(start, end - start);

            SearchResult result;
            result.document = doc;
            result.pageIndex = -1; // Not page-specific in this basic example
            result.text = bestMatch;
            result.context = context;
            result.score = pair.second; // Use the count of matched terms as score

            results.append(result);
            count++;
        }
    }

    emit queryFinished(results);
    LOG_DEBUG("FullTextIndex: Query '" << query << "' returned " << results.size() << " results.");
    return results;
}

QFuture<QList<SearchResult>> FullTextIndex::queryAsync(const QString& query, int maxResults, int contextLength) const
{
    // Use QtConcurrent to run the query in a separate thread
    return QtConcurrent::run([this, query, maxResults, contextLength]() {
        return this->query(query, maxResults, contextLength);
    });
}

int FullTextIndex::documentCount() const
{
    QMutexLocker locker(&d->mutex);
    return d->indexedDocuments.size();
}

int FullTextIndex::termCount() const
{
    QMutexLocker locker(&d->mutex);
    // This is an approximation. A real index would store this count.
    QSet<QString> uniqueTerms;
    for (auto it = d->wordToDocs.begin(); it != d->wordToDocs.end(); ++it) {
        uniqueTerms.insert(it.key());
    }
    return uniqueTerms.size();
}

void FullTextIndex::commit()
{
    // For an in-memory index, commit might mean saving to disk.
    // For a library-based index, this might flush pending writes.
    LOG_DEBUG("FullTextIndex: Commit called (in-memory index, no-op for now).");
    // Example disk save: serialize d->docIndex and d->wordToDocs to files
}

void FullTextIndex::optimize()
{
    // Optimization depends on the underlying technology.
    // For an in-memory index, perhaps compact data structures.
    // For Lucene/Xapian, call their optimize/compact functions.
    LOG_DEBUG("FullTextIndex: Optimize called (no-op for basic in-memory index).");
}

void FullTextIndex::clear()
{
    QMutexLocker locker(&d->mutex);
    d->docIndex.clear();
    d->wordToDocs.clear();
    d->indexedDocuments.clear();
    emit indexContentChanged();
    LOG_DEBUG("FullTextIndex: Cleared all indexed data.");
}

QString FullTextIndex::indexPath() const
{
    QMutexLocker locker(&d->mutex);
    return d->indexPathStr;
}

} // namespace QuantilyxDoc