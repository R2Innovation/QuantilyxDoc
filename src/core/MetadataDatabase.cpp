/**
 * QuantilyxDoc - Professional Document Editor
 * Copyright (C) 2025 R² Innovative Software
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */
#include "MetadataDatabase.h"
#include "Logger.h"
#include "utils/FileUtils.h" // Assuming this exists
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QSqlRecord>
#include <QSqlDriver>
#include <QDir>
#include <QStandardPaths>
#include <QDateTime>
#include <QMutex>
#include <QMutexLocker>
#include <QVariantMap>
#include <QVariantList>
#include <QRegularExpression>
#include <QDebug>
#include <QCryptographicHash>

namespace QuantilyxDoc {

// Static instance pointer
MetadataDatabase* MetadataDatabase::s_instance = nullptr;

MetadataDatabase& MetadataDatabase::instance()
{
    if (!s_instance) {
        s_instance = new MetadataDatabase();
    }
    return *s_instance;
}

MetadataDatabase::MetadataDatabase(QObject* parent)
    : QObject(parent)
    , d(new Private(this))
{
    LOG_INFO("MetadataDatabase created.");
}

MetadataDatabase::~MetadataDatabase()
{
    if (d->db.isOpen()) {
        d->db.close();
    }
    LOG_INFO("MetadataDatabase destroyed.");
}

bool MetadataDatabase::initialize(const QString& dbPath)
{
    QMutexLocker locker(&d->mutex);

    if (d->initialized) {
        LOG_WARN("MetadataDatabase::initialize: Already initialized.");
        return true;
    }

    d->dbPathStr = dbPath.isEmpty() ? QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/metadata.db" : dbPath;

    // Ensure the directory exists
    QFileInfo dbInfo(d->dbPathStr);
    QDir().mkpath(dbInfo.absolutePath());

    // Add a unique connection name to avoid conflicts if other parts of the app use QSqlDatabase
    QString connectionName = "metadata_db_connection_" + QUuid::createUuid().toString(QUuid::WithoutBraces);
    d->db = QSqlDatabase::addDatabase("QSQLITE", connectionName);
    d->db.setDatabaseName(d->dbPathStr);

    if (!d->db.open()) {
        LOG_ERROR("MetadataDatabase: Failed to open SQLite database: " << d->db.lastError().text());
        return false;
    }

    // Enable foreign keys (good practice for referential integrity)
    QSqlQuery pragmaForeignKey(d->db);
    if (!pragmaForeignKey.exec("PRAGMA foreign_keys = ON;")) {
        LOG_WARN("MetadataDatabase: Could not enable foreign keys: " << pragmaForeignKey.lastError().text());
        // This might be acceptable depending on the schema, but it's generally good to have them on.
    }

    // Create tables if they don't exist
    if (!createTables()) {
        LOG_ERROR("MetadataDatabase: Failed to create required tables.");
        d->db.close();
        return false;
    }

    d->initialized = true;
    LOG_INFO("MetadataDatabase initialized successfully at: " << d->dbPathStr);
    emit initialized(true);
    return true;
}

bool MetadataDatabase::isInitialized() const
{
    QMutexLocker locker(&d->mutex);
    return d->initialized && d->db.isOpen();
}

bool MetadataDatabase::storeMetadata(const QString& filePath, const QVariantMap& metadata)
{
    if (!isInitialized()) {
        LOG_ERROR("MetadataDatabase::storeMetadata: Database is not initialized.");
        return false;
    }

    QMutexLocker locker(&d->mutex);

    // Calculate file hash for uniqueness/duplicate detection
    QFile file(filePath);
    QCryptographicHash hash(QCryptographicHash::Sha256);
    if (!file.open(QIODevice::ReadOnly) || !hash.addData(&file)) {
        LOG_ERROR("MetadataDatabase::storeMetadata: Failed to calculate hash for file: " << filePath);
        return false;
    }
    QString fileHash = hash.result().toHex();

    QSqlQuery query(d->db);
    // Use UPSERT (INSERT ... ON CONFLICT ... DO UPDATE in SQLite 3.24+)
    // Or use REPLACE INTO (which deletes and re-inserts)
    // Or SELECT first then INSERT/UPDATE.
    // Let's use INSERT OR REPLACE for simplicity, though UPSERT is more efficient if available.
    // Schema: files (id INTEGER PRIMARY KEY, path TEXT UNIQUE, hash TEXT UNIQUE, size INTEGER, ...)
    // metadata_table (file_id INTEGER, key TEXT, value TEXT, FOREIGN KEY(file_id) REFERENCES files(id))
    // This requires a join or separate inserts/updates.

    // First, upsert the file record to get its ID
    query.prepare("INSERT OR REPLACE INTO files (path, hash, size, last_modified) VALUES (?, ?, ?, ?);");
    QFileInfo info(filePath);
    query.addBindValue(filePath);
    query.addBindValue(fileHash);
    query.addBindValue(info.size());
    query.addBindValue(info.lastModified().toMSecsSinceEpoch() / 1000); // Store as seconds since epoch

    if (!query.exec()) {
        LOG_ERROR("MetadataDatabase::storeMetadata: Failed to upsert file record: " << query.lastError().text());
        return false;
    }

    // Get the inserted/updated file ID
    qint64 fileId = d->db.lastInsertId().toLongLong();
    if (fileId == 0) {
        // If lastInsertId is 0, it might mean the row existed and was updated.
        // We need to fetch the ID explicitly.
        QSqlQuery idQuery(d->db);
        idQuery.prepare("SELECT id FROM files WHERE path = ?;");
        idQuery.addBindValue(filePath);
        if (idQuery.exec() && idQuery.next()) {
            fileId = idQuery.value(0).toLongLong();
        } else {
            LOG_ERROR("MetadataDatabase::storeMetadata: Failed to get file ID after upsert.");
            return false;
        }
    }

    // Clear existing metadata for this file ID (optional, depending on merge strategy)
    // QSqlQuery clearQuery(d->db);
    // clearQuery.prepare("DELETE FROM metadata WHERE file_id = ?;");
    // clearQuery.addBindValue(fileId);
    // if (!clearQuery.exec()) { ... handle error ... }

    // Insert/update metadata key-value pairs
    QSqlQuery metadataQuery(d->db);
    metadataQuery.prepare("INSERT OR REPLACE INTO metadata (file_id, key, value) VALUES (?, ?, ?);");
    bool success = true;
    for (auto it = metadata.constBegin(); it != metadata.constEnd(); ++it) {
        metadataQuery.addBindValue(fileId);
        metadataQuery.addBindValue(it.key());
        metadataQuery.addBindValue(it.value().toString()); // Store all values as strings for simplicity, or use BLOB for complex types
        if (!metadataQuery.exec()) {
            LOG_ERROR("MetadataDatabase::storeMetadata: Failed to upsert metadata for key '" << it.key() << "', file: " << filePath << ", Error: " << metadataQuery.lastError().text());
            success = false; // Keep going to try other keys
        }
    }

    if (success) {
        LOG_DEBUG("MetadataDatabase: Stored metadata for file: " << filePath);
        emit metadataStored(filePath);
    }
    return success;
}

QVariantMap MetadataDatabase::retrieveMetadata(const QString& filePath) const
{
    if (!isInitialized()) {
        LOG_ERROR("MetadataDatabase::retrieveMetadata: Database is not initialized.");
        return QVariantMap();
    }

    QMutexLocker locker(&d->mutex);

    QSqlQuery query(d->db);
    query.prepare("SELECT m.key, m.value FROM metadata m JOIN files f ON m.file_id = f.id WHERE f.path = ?;");
    query.addBindValue(filePath);

    if (!query.exec()) {
        LOG_ERROR("MetadataDatabase::retrieveMetadata: Query failed: " << query.lastError().text());
        return QVariantMap();
    }

    QVariantMap metadata;
    while (query.next()) {
        QString key = query.value(0).toString();
        QString value = query.value(1).toString(); // Retrieve as string, cast later if needed
        metadata.insert(key, value);
    }

    LOG_DEBUG("MetadataDatabase: Retrieved metadata for file: " << filePath << " (Keys: " << metadata.keys().size() << ")");
    return metadata;
}

bool MetadataDatabase::removeMetadata(const QString& filePath)
{
    if (!isInitialized()) {
        LOG_ERROR("MetadataDatabase::removeMetadata: Database is not initialized.");
        return false;
    }

    QMutexLocker locker(&d->mutex);

    QSqlQuery query(d->db);
    query.prepare("DELETE FROM files WHERE path = ?;"); // CASCADE DELETE should remove associated metadata if FKs are on
    query.addBindValue(filePath);

    if (!query.exec()) {
        LOG_ERROR("MetadataDatabase::removeMetadata: Failed to delete file record: " << query.lastError().text());
        return false;
    }

    if (query.numRowsAffected() > 0) {
        LOG_DEBUG("MetadataDatabase: Removed metadata for file: " << filePath);
        emit metadataRemoved(filePath);
        return true;
    } else {
        LOG_WARN("MetadataDatabase::removeMetadata: No metadata record found for file: " << filePath);
        return false; // Or true? Semantically, the metadata is "removed" (doesn't exist anymore).
    }
}

QList<MetadataDatabase::SearchResult> MetadataDatabase::searchMetadata(const QString& query, const QStringList& keys) const
{
    if (!isInitialized() || query.isEmpty()) {
        LOG_ERROR("MetadataDatabase::searchMetadata: Database not initialized or query is empty.");
        return QList<SearchResult>();
    }

    QMutexLocker locker(&d->mutex);

    QSqlQuery sqlQuery(d->db);

    // Build the WHERE clause based on keys and query string
    QString whereClause = "m.value LIKE ? ESCAPE '\\' ";
    QStringList bindValues;
    bindValues.append("%" + query.replace("\\", "\\\\").replace("%", "\\%").replace("_", "\\_") + "%"); // Escape LIKE wildcards

    if (!keys.isEmpty()) {
        QString keyConditions = " AND (";
        QStringList keyParts;
        for (const QString& key : keys) {
            keyParts.append("m.key = ?");
            bindValues.append(key);
        }
        keyConditions += keyParts.join(" OR ") + ")";
        whereClause += keyConditions;
    }

    // Join files and metadata tables
    QString queryString = "SELECT f.path, m.key, m.value FROM files f JOIN metadata m ON f.id = m.file_id WHERE " + whereClause + ";";
    sqlQuery.prepare(queryString);

    for (const QString& val : bindValues) {
        sqlQuery.addBindValue(val);
    }

    if (!sqlQuery.exec()) {
        LOG_ERROR("MetadataDatabase::searchMetadata: Query failed: " << sqlQuery.lastError().text());
        return QList<SearchResult>();
    }

    QList<SearchResult> results;
    while (sqlQuery.next()) {
        SearchResult result;
        result.filePath = sqlQuery.value(0).toString(); // f.path
        result.key = sqlQuery.value(1).toString();      // m.key
        result.value = sqlQuery.value(2).toString();    // m.value
        results.append(result);
    }

    LOG_DEBUG("MetadataDatabase: Search query '" << query << "' returned " << results.size() << " results.");
    return results;
}

QStringList MetadataDatabase::getAllKeys() const
{
    if (!isInitialized()) {
        LOG_ERROR("MetadataDatabase::getAllKeys: Database is not initialized.");
        return QStringList();
    }

    QMutexLocker locker(&d->mutex);

    QSqlQuery query(d->db);
    query.prepare("SELECT DISTINCT key FROM metadata ORDER BY key ASC;");

    if (!query.exec()) {
        LOG_ERROR("MetadataDatabase::getAllKeys: Query failed: " << query.lastError().text());
        return QStringList();
    }

    QStringList keys;
    while (query.next()) {
        keys.append(query.value(0).toString());
    }

    LOG_DEBUG("MetadataDatabase: Retrieved " << keys.size() << " unique metadata keys.");
    return keys;
}

QStringList MetadataDatabase::getAllFilePaths() const
{
    if (!isInitialized()) {
        LOG_ERROR("MetadataDatabase::getAllFilePaths: Database is not initialized.");
        return QStringList();
    }

    QMutexLocker locker(&d->mutex);

    QSqlQuery query(d->db);
    query.prepare("SELECT path FROM files ORDER BY path ASC;");

    if (!query.exec()) {
        LOG_ERROR("MetadataDatabase::getAllFilePaths: Query failed: " << query.lastError().text());
        return QStringList();
    }

    QStringList paths;
    while (query.next()) {
        paths.append(query.value(0).toString());
    }

    LOG_DEBUG("MetadataDatabase: Retrieved " << paths.size() << " unique file paths.");
    return paths;
}

int MetadataDatabase::entryCount() const
{
    if (!isInitialized()) {
        LOG_ERROR("MetadataDatabase::entryCount: Database is not initialized.");
        return -1;
    }

    QMutexLocker locker(&d->mutex);

    QSqlQuery query(d->db);
    query.prepare("SELECT COUNT(*) FROM metadata;");

    if (!query.exec() || !query.next()) {
        LOG_ERROR("MetadataDatabase::entryCount: Query failed: " << (query.isValid() ? query.lastError().text() : "No result"));
        return -1;
    }

    int count = query.value(0).toInt();
    LOG_DEBUG("MetadataDatabase: Total metadata entries: " << count);
    return count;
}

void MetadataDatabase::beginTransaction()
{
    if (isInitialized()) {
        QMutexLocker locker(&d->mutex);
        if (!d->db.transaction()) {
            LOG_ERROR("MetadataDatabase::beginTransaction: Failed to start transaction: " << d->db.lastError().text());
        } else {
            LOG_DEBUG("MetadataDatabase: Transaction begun.");
        }
    }
}

void MetadataDatabase::commitTransaction()
{
    if (isInitialized()) {
        QMutexLocker locker(&d->mutex);
        if (!d->db.commit()) {
            LOG_ERROR("MetadataDatabase::commitTransaction: Failed to commit transaction: " << d->db.lastError().text());
        } else {
            LOG_DEBUG("MetadataDatabase: Transaction committed.");
        }
    }
}

void MetadataDatabase::rollbackTransaction()
{
    if (isInitialized()) {
        QMutexLocker locker(&d->mutex);
        if (!d->db.rollback()) {
            LOG_ERROR("MetadataDatabase::rollbackTransaction: Failed to rollback transaction: " << d->db.lastError().text());
        } else {
            LOG_DEBUG("MetadataDatabase: Transaction rolled back.");
        }
    }
}

bool MetadataDatabase::vacuum()
{
    if (!isInitialized()) {
        LOG_ERROR("MetadataDatabase::vacuum: Database is not initialized.");
        return false;
    }

    QMutexLocker locker(&d->mutex);

    QSqlQuery query(d->db);
    if (!query.exec("VACUUM;")) {
        LOG_ERROR("MetadataDatabase::vacuum: VACUUM command failed: " << query.lastError().text());
        return false;
    }

    LOG_INFO("MetadataDatabase: VACUUM completed successfully.");
    return true;
}

bool MetadataDatabase::createTables()
{
    // Transaction for creating tables atomically
    if (!d->db.transaction()) {
         LOG_ERROR("MetadataDatabase::createTables: Failed to start transaction for table creation.");
         return false;
    }

    bool success = true;
    QSqlQuery query(d->db);

    // Create 'files' table to store basic file information
    QString createFilesTable = R"(
        CREATE TABLE IF NOT EXISTS files (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            path TEXT UNIQUE NOT NULL,
            hash TEXT UNIQUE, -- SHA256 hash of the file content
            size INTEGER,
            last_modified INTEGER, -- Unix timestamp (seconds since epoch)
            created_at INTEGER DEFAULT (unixepoch('now')) -- When this record was added to the DB
        );
    )";

    if (!query.exec(createFilesTable)) {
        LOG_ERROR("MetadataDatabase::createTables: Failed to create 'files' table: " << query.lastError().text());
        success = false;
    }

    // Create 'metadata' table to store key-value pairs associated with files
    QString createMetadataTable = R"(
        CREATE TABLE IF NOT EXISTS metadata (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            file_id INTEGER NOT NULL,
            key TEXT NOT NULL,
            value TEXT,
            created_at INTEGER DEFAULT (unixepoch('now')),
            FOREIGN KEY (file_id) REFERENCES files (id) ON DELETE CASCADE
        );
    )";

    if (success && !query.exec(createMetadataTable)) {
        LOG_ERROR("MetadataDatabase::createTables: Failed to create 'metadata' table: " << query.lastError().text());
        success = false;
    }

    // Create indexes for performance
    QString createPathIndex = "CREATE INDEX IF NOT EXISTS idx_files_path ON files (path);";
    QString createHashIndex = "CREATE INDEX IF NOT EXISTS idx_files_hash ON files (hash);";
    QString createMetadataFileIndex = "CREATE INDEX IF NOT EXISTS idx_metadata_file_id ON metadata (file_id);";
    QString createMetadataKeyIndex = "CREATE INDEX IF NOT EXISTS idx_metadata_key ON metadata (key);";

    for (const QString& indexSql : {createPathIndex, createHashIndex, createMetadataFileIndex, createMetadataKeyIndex}) {
        if (success && !query.exec(indexSql)) {
            LOG_WARN("MetadataDatabase::createTables: Failed to create index: " << query.lastError().text() << ". SQL: " << indexSql);
            // Index creation failure is not fatal, but degrades performance.
            // success = true; // Keep success as true for indexes
        }
    }

    if (success) {
        if (!d->db.commit()) {
            LOG_ERROR("MetadataDatabase::createTables: Failed to commit table creation transaction: " << d->db.lastError().text());
            success = false;
        } else {
            LOG_DEBUG("MetadataDatabase::createTables: Tables created successfully.");
        }
    } else {
        if (!d->db.rollback()) {
            LOG_ERROR("MetadataDatabase::createTables: Failed to rollback failed table creation: " << d->db.lastError().text());
        } else {
            LOG_DEBUG("MetadataDatabase::createTables: Rolled back failed table creation.");
        }
    }

    return success;
}

class MetadataDatabase::Private {
public:
    Private(MetadataDatabase* q_ptr)
        : q(q_ptr), initialized(false) {}

    MetadataDatabase* q;
    mutable QMutex mutex; // Protect database access across threads if needed
    bool initialized;
    QString dbPathStr;
    QSqlDatabase db;
};

} // namespace QuantilyxDoc