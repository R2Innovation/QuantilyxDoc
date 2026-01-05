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
#include "../core/Settings.h"
#include <QDir>
#include <QStandardPaths>
#include <QTextStream>
#include <QFile>
#include <QFileInfo>
#include <QMutex>
#include <QMutexLocker>
#include <QRegularExpression>
#include <QTextBoundaryFinder>
#include <QDebug>
#include <QElapsedTimer>
// #include "sqlite3.h" // If using SQLite C API directly for FTS
// #include "fts5.h"    // If using SQLite FTS5 C API directly
// Or, if using a C++ wrapper or different library:
// #include "sphinxclient.h" // Example for Sphinx
// #include "xapian.h"       // Example for Xapian

// For this example, we'll demonstrate using Qt's SQL module with SQLite's FTS5 extension enabled.
// This requires the underlying SQLite library used by Qt to be compiled with FTS5 support.
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QSqlRecord>
#include <QSqlDriver>
#include <QSqlTableModel>
#include <QSqlIndex>
#include <QSqlRelation>

namespace QuantilyxDoc {

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
    // Ensure index is closed and resources are freed
    if (d->db.isOpen()) {
        d->db.close();
    }
    LOG_INFO("FullTextIndex destroyed.");
}

bool FullTextIndex::initialize(const QString& indexPath)
{
    QMutexLocker locker(&d->mutex);

    if (d->initialized) {
        LOG_WARN("FullTextIndex::initialize: Already initialized.");
        return true;
    }

    d->indexPathStr = indexPath.isEmpty() ? QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/fts_index" : indexPath;

    // Ensure the index directory exists
    QFileInfo indexInfo(d->indexPathStr);
    if (indexInfo.isDir()) {
        d->indexPathStr = indexInfo.absoluteFilePath(); // Use canonical path
    } else if (indexInfo.exists()) {
        LOG_ERROR("FullTextIndex::initialize: Index path exists but is not a directory: " << indexPath);
        return false;
    } else {
        if (!QDir().mkpath(indexInfo.absolutePath())) {
            LOG_ERROR("FullTextIndex::initialize: Failed to create parent directory for index: " << indexInfo.absolutePath());
            return false;
        }
    }

    // For this implementation using SQLite FTS5 via Qt's SQL module:
    QString dbPath = d->indexPathStr + "/fulltext.sqlite"; // Single DB file for the index
    QString connectionName = "fts_index_db_" + QUuid::createUuid().toString(QUuid::WithoutBraces);

    d->db = QSqlDatabase::addDatabase("QSQLITE", connectionName);
    d->db.setDatabaseName(dbPath);

    if (!d->db.open()) {
        LOG_ERROR("FullTextIndex: Failed to open SQLite database for FTS index: " << d->db.lastError().text());
        return false;
    }

    // Check if FTS5 is available in the underlying SQLite
    QSqlQuery featureQuery(d->db);
    featureQuery.prepare("SELECT sqlite_compileoption_used('ENABLE_FTS5');");
    if (!featureQuery.exec() || !featureQuery.next() || featureQuery.value(0).toInt() != 1) {
        LOG_ERROR("FullTextIndex: SQLite FTS5 extension is not available in the Qt SQL driver.");
        d->db.close();
        return false;
    }

    // Create the FTS5 virtual table
    if (!createIndexTable()) {
        LOG_ERROR("FullTextIndex: Failed to create FTS5 index table.");
        d->db.close();
        return false;
    }

    d->initialized = true;
    LOG_INFO("FullTextIndex initialized successfully at: " << d->indexPathStr);
    emit initialized(true);
    return true;
}

bool FullTextIndex::isInitialized() const
{
    QMutexLocker locker(&d->mutex);
    return d->initialized && d->db.isOpen();
}

bool FullTextIndex::addDocument(const QString& filePath, const QString& content, const QVariantMap& metadata)
{
    if (!isInitialized() || filePath.isEmpty() || content.isEmpty()) {
        LOG_ERROR("FullTextIndex::addDocument: Index not initialized, file path is empty, or content is empty.");
        return false;
    }

    QMutexLocker locker(&d->mutex);

    // Calculate a unique ID for the document. Using file path hash is common.
    QString docId = QString(QCryptographicHash::hash(filePath.toUtf8(), QCryptographicHash::Sha256).toHex());

    QSqlQuery query(d->db);
    // The FTS5 table was created with columns: id, path, content, title, author, subject, keywords
    // We'll use REPLACE to update the document if it already exists in the index.
    query.prepare("INSERT OR REPLACE INTO documents_fts (id, path, content, title, author, subject, keywords) VALUES (?, ?, ?, ?, ?, ?, ?);");

    query.addBindValue(docId);
    query.addBindValue(filePath);
    query.addBindValue(content); // The main text content to be indexed
    query.addBindValue(metadata.value("Title", "").toString());
    query.addBindValue(metadata.value("Author", "").toString());
    query.addBindValue(metadata.value("Subject", "").toString());
    query.addBindValue(metadata.value("Keywords", "").toString());

    if (!query.exec()) {
        LOG_ERROR("FullTextIndex::addDocument: Failed to insert document into FTS index: " << query.lastError().text() << ". File: " << filePath);
        return false;
    }

    LOG_DEBUG("FullTextIndex: Added/updated document to index: " << filePath);
    emit documentIndexed(filePath, true); // Emit signal for success
    return true;
}

bool FullTextIndex::removeDocument(const QString& filePath)
{
    if (!isInitialized() || filePath.isEmpty()) {
        LOG_ERROR("FullTextIndex::removeDocument: Index not initialized or file path is empty.");
        return false;
    }

    QMutexLocker locker(&d->mutex);

    QString docId = QString(QCryptographicHash::hash(filePath.toUtf8(), QCryptographicHash::Sha256).toHex());

    QSqlQuery query(d->db);
    query.prepare("DELETE FROM documents_fts WHERE id = ?;");
    query.addBindValue(docId);

    if (!query.exec()) {
        LOG_ERROR("FullTextIndex::removeDocument: Failed to delete document from FTS index: " << query.lastError().text() << ". File: " << filePath);
        return false;
    }

    if (query.numRowsAffected() > 0) {
        LOG_DEBUG("FullTextIndex: Removed document from index: " << filePath);
        emit documentIndexed(filePath, false); // Emit signal indicating removal
        return true;
    } else {
        LOG_WARN("FullTextIndex::removeDocument: Document not found in index: " << filePath);
        // Not necessarily an error if the file wasn't indexed to begin with.
        return true; // Or false? Semantically, the goal (removing from index) is achieved.
    }
}

QList<FullTextIndex::SearchResult> FullTextIndex::search(const QString& query, int maxResults, int contextLength) const
{
    if (!isInitialized() || query.isEmpty()) {
        LOG_ERROR("FullTextIndex::search: Index not initialized or query is empty.");
        return QList<SearchResult>();
    }

    QMutexLocker locker(&d->mutex);

    QList<SearchResult> results;

    QSqlQuery searchQuery(d->db);
    // Use FTS5's built-in match function and snippet function for context.
    // The snippet function allows us to get a portion of the text around the match.
    // Syntax: snippet(table_name, column_number, start_match, end_match, ellipsis, max_tokens)
    // Column numbers: 0=path, 1=content, 2=title, 3=author, 4=subject, 5=keywords (based on CREATE VIRTUAL TABLE statement)
    // We'll snippet the 'content' column (index 1).
    QString queryString = "SELECT path, snippet(documents_fts, 1, '[', ']', '...', 15) AS context_snippet FROM documents_fts WHERE documents_fts MATCH ? ORDER BY rank LIMIT ?;";
    searchQuery.prepare(queryString);
    searchQuery.addBindValue(query);
    searchQuery.addBindValue(maxResults > 0 ? maxResults : 50); // Default to 50 if maxResults is 0 or negative

    if (!searchQuery.exec()) {
        LOG_ERROR("FullTextIndex::search: Search query failed: " << searchQuery.lastError().text() << ". Query: " << query);
        return results;
    }

    while (searchQuery.next()) {
        SearchResult result;
        result.filePath = searchQuery.value(0).toString(); // Path from 'path' column
        result.snippet = searchQuery.value(1).toString();  // Snippet from 'content' column
        // The 'rank' is implicitly used by the 'ORDER BY rank' clause in SQLite FTS5.
        // FTS5 calculates its own rank based on BM25 or similar algorithm.
        // If needed, we could select the rank explicitly: SELECT ..., rank FROM ...
        results.append(result);
    }

    LOG_DEBUG("FullTextIndex: Search query '" << query << "' returned " << results.size() << " results.");
    emit searchFinished(query, results.size());
    return results;
}

bool FullTextIndex::updateDocument(const QString& filePath, const QString& newContent, const QVariantMap& newMetadata)
{
    // Updating is the same as adding for an FTS table using INSERT OR REPLACE.
    // We just need to fetch the old content/metadata if we want to be smart about it,
    // but for simplicity, we'll just call addDocument.
    // This assumes addDocument uses REPLACE based on the unique ID (derived from filePath).
    LOG_DEBUG("FullTextIndex::updateDocument: Calling addDocument for update (using REPLACE). File: " << filePath);
    return addDocument(filePath, newContent, newMetadata);
}

bool FullTextIndex::isDocumentIndexed(const QString& filePath) const
{
    if (!isInitialized() || filePath.isEmpty()) {
        LOG_ERROR("FullTextIndex::isDocumentIndexed: Index not initialized or file path is empty.");
        return false;
    }

    QMutexLocker locker(&d->mutex);

    QString docId = QString(QCryptographicHash::hash(filePath.toUtf8(), QCryptographicHash::Sha256).toHex());

    QSqlQuery query(d->db);
    query.prepare("SELECT COUNT(*) FROM documents_fts WHERE id = ?;");
    query.addBindValue(docId);

    if (!query.exec() || !query.next()) {
        LOG_ERROR("FullTextIndex::isDocumentIndexed: Query failed: " << (query.isValid() ? query.lastError().text() : "No result"));
        return false;
    }

    bool indexed = query.value(0).toInt() > 0;
    LOG_DEBUG("FullTextIndex: Document '" << filePath << "' is indexed: " << indexed);
    return indexed;
}

int FullTextIndex::documentCount() const
{
    if (!isInitialized()) {
        LOG_ERROR("FullTextIndex::documentCount: Index not initialized.");
        return -1;
    }

    QMutexLocker locker(&d->mutex);

    QSqlQuery query(d->db);
    query.prepare("SELECT COUNT(*) FROM documents_fts;");

    if (!query.exec() || !query.next()) {
        LOG_ERROR("FullTextIndex::documentCount: Query failed: " << (query.isValid() ? query.lastError().text() : "No result"));
        return -1;
    }

    int count = query.value(0).toInt();
    LOG_DEBUG("FullTextIndex: Total indexed documents: " << count);
    return count;
}

void FullTextIndex::beginTransaction()
{
    if (isInitialized()) {
        QMutexLocker locker(&d->mutex);
        if (!d->db.transaction()) {
            LOG_ERROR("FullTextIndex::beginTransaction: Failed to start transaction: " << d->db.lastError().text());
        } else {
            LOG_DEBUG("FullTextIndex: Transaction begun for batch indexing.");
        }
    }
}

void FullTextIndex::commitTransaction()
{
    if (isInitialized()) {
        QMutexLocker locker(&d->mutex);
        if (!d->db.commit()) {
            LOG_ERROR("FullTextIndex::commitTransaction: Failed to commit transaction: " << d->db.lastError().text());
        } else {
            LOG_DEBUG("FullTextIndex: Transaction committed for batch indexing.");
        }
    }
}

void FullTextIndex::rollbackTransaction()
{
    if (isInitialized()) {
        QMutexLocker locker(&d->mutex);
        if (!d->db.rollback()) {
            LOG_ERROR("FullTextIndex::rollbackTransaction: Failed to rollback transaction: " << d->db.lastError().text());
        } else {
            LOG_DEBUG("FullTextIndex: Transaction rolled back for batch indexing.");
        }
    }
}

bool FullTextIndex::optimize()
{
    if (!isInitialized()) {
        LOG_ERROR("FullTextIndex::optimize: Index not initialized.");
        return false;
    }

    QMutexLocker locker(&d->mutex);

    // FTS5 has an integrity-check command and potentially internal optimization.
    // The main optimization for SQLite FTS is often just VACUUM, which rebuilds the entire DB file.
    // FTS5 tables also have internal structures that might benefit from re-indexing if many updates/deletes occurred.
    // A common optimization command for FTS tables is: INSERT INTO table_name(table_name) VALUES('optimize');
    QSqlQuery optimizeQuery(d->db);
    QString optimizeSql = "INSERT INTO documents_fts(documents_fts) VALUES('optimize');";
    if (!optimizeQuery.exec(optimizeSql)) {
        LOG_WARN("FullTextIndex::optimize: Optimize command failed or not supported by this FTS setup: " << optimizeQuery.lastError().text());
        // Optimize failure might be non-critical.
        return false;
    }

    LOG_INFO("FullTextIndex: Optimization command executed.");
    return true;
}

bool FullTextIndex::createIndexTable()
{
    // Begin a transaction for table creation
    if (!d->db.transaction()) {
         LOG_ERROR("FullTextIndex::createIndexTable: Failed to start transaction.");
         return false;
    }

    bool success = true;
    QSqlQuery query(d->db);

    // Create the FTS5 virtual table.
    // The columns defined here are what will be indexed and searched.
    // Common ones are path, content, title, author, subject, keywords.
    // The 'content=' option can be used to point to an external content table if desired,
    // but for simplicity, we'll store content directly in the FTS table.
    QString createTableSql = R"(
        CREATE VIRTUAL TABLE IF NOT EXISTS documents_fts USING fts5(
            id UNINDEXED, -- Unique identifier, not part of search index
            path UNINDEXED, -- File path, not part of search index, but useful for results
            content,      -- The main content to be indexed
            title,        -- Title field
            author,       -- Author field
            subject,      -- Subject field
            keywords,     -- Keywords field
            tokenize='porter' -- Optional: Use Porter Stemmer for tokenization
        );
    )";

    if (!query.exec(createTableSql)) {
        LOG_ERROR("FullTextIndex::createIndexTable: Failed to create FTS5 table: " << query.lastError().text());
        success = false;
    }

    // Commit or rollback the transaction
    if (success) {
        if (!d->db.commit()) {
            LOG_ERROR("FullTextIndex::createIndexTable: Failed to commit table creation: " << d->db.lastError().text());
            success = false;
        } else {
            LOG_DEBUG("FullTextIndex::createIndexTable: FTS5 table created successfully.");
        }
    } else {
        if (!d->db.rollback()) {
            LOG_ERROR("FullTextIndex::createIndexTable: Failed to rollback failed table creation: " << d->db.lastError().text());
        } else {
            LOG_DEBUG("FullTextIndex::createIndexTable: Rolled back failed table creation.");
        }
    }

    return success;
}

class FullTextIndex::Private {
public:
    Private(FullTextIndex* q_ptr)
        : q(q_ptr), initialized(false) {}

    FullTextIndex* q;
    mutable QMutex mutex; // Protect database access across threads if needed
    bool initialized;
    QString indexPathStr;
    QSqlDatabase db;
};

} // namespace QuantilyxDoc