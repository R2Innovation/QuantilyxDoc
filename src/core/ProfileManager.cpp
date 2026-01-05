/**
 * QuantilyxDoc - Professional Document Editor
 * Copyright (C) 2025 R² Innovative Software
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */
#include "ProfileManager.h"
#include "Logger.h"
#include "Settings.h"
#include "utils/FileUtils.h" // Assuming this exists
#include <QDir>
#include <QStandardPaths>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QFile>
#include <QTextStream>
#include <QMutex>
#include <QMutexLocker>
#include <QCoreApplication>
#include <QDebug>

namespace QuantilyxDoc {

// Static instance pointer
ProfileManager* ProfileManager::s_instance = nullptr;

ProfileManager& ProfileManager::instance()
{
    if (!s_instance) {
        s_instance = new ProfileManager();
    }
    return *s_instance;
}

ProfileManager::ProfileManager(QObject* parent)
    : QObject(parent)
    , d(new Private(this))
{
    LOG_INFO("ProfileManager created.");
}

ProfileManager::~ProfileManager()
{
    // Ensure current profile is saved before destruction?
    // saveCurrentProfile();
    LOG_INFO("ProfileManager destroyed.");
}

bool ProfileManager::initialize()
{
    QMutexLocker locker(&d->mutex);

    if (d->initialized) {
        LOG_WARN("ProfileManager::initialize: Already initialized.");
        return true;
    }

    d->profilesDirectory = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/profiles";
    QDir().mkpath(d->profilesDirectory); // Ensure profiles directory exists

    if (!loadProfileList()) {
        LOG_ERROR("ProfileManager: Failed to load profile list from " << d->profilesDirectory);
        return false;
    }

    // Determine the initial active profile
    QString desiredProfileName = Settings::instance().value<QString>("General/ActiveProfile", "default");
    if (d->profiles.contains(desiredProfileName)) {
        if (!switchToProfile(desiredProfileName)) {
            LOG_WARN("ProfileManager: Could not switch to startup profile '" << desiredProfileName << "', falling back to 'default'.");
            if (!switchToProfile("default") && !d->profiles.isEmpty()) {
                // If 'default' doesn't exist or fails, try the first available profile
                QString firstProfileName = d->profiles.keys().first();
                if (!switchToProfile(firstProfileName)) {
                    LOG_CRITICAL("ProfileManager: Failed to switch to any available profile after 'default' failed.");
                    return false;
                }
            } else if (d->profiles.isEmpty()) {
                 // No profiles exist, create a default one
                 if (!createProfile("default", "Default profile created at first run.")) {
                      LOG_CRITICAL("ProfileManager: Failed to create initial default profile.");
                      return false;
                 }
                 if (!switchToProfile("default")) {
                      LOG_CRITICAL("ProfileManager: Failed to switch to the newly created default profile.");
                      return false;
                 }
            }
        }
    } else {
        LOG_WARN("ProfileManager: Startup profile '" << desiredProfileName << "' not found, falling back to 'default' or creating it.");
        if (!d->profiles.contains("default")) {
            if (!createProfile("default", "Default profile")) {
                LOG_CRITICAL("ProfileManager: Failed to create default profile.");
                return false;
            }
        }
        if (!switchToProfile("default")) {
            LOG_CRITICAL("ProfileManager: Failed to switch to default profile.");
            return false;
        }
    }

    d->initialized = true;
    LOG_INFO("ProfileManager initialized. Active profile: " << d->currentProfileName);
    emit initializationComplete(true);
    return true;
}

bool ProfileManager::isInitialized() const
{
    QMutexLocker locker(&d->mutex);
    return d->initialized;
}

bool ProfileManager::switchToProfile(const QString& profileName)
{
    QMutexLocker locker(&d->mutex);

    if (!d->initialized) {
        LOG_ERROR("ProfileManager::switchToProfile: Not initialized.");
        return false;
    }

    if (profileName == d->currentProfileName) {
        LOG_DEBUG("ProfileManager::switchToProfile: Already on profile '" << profileName << "'.");
        return true;
    }

    if (!d->profiles.contains(profileName)) {
        LOG_ERROR("ProfileManager::switchToProfile: Profile '" << profileName << "' does not exist.");
        return false;
    }

    // Save current profile's settings before switching
    if (!saveCurrentProfile()) {
        LOG_WARN("ProfileManager::switchToProfile: Failed to save settings for current profile '" << d->currentProfileName << "'. Continuing with switch.");
        // We might want to abort the switch here, depending on requirements.
        // For now, continue.
    }

    // Load the new profile's settings
    QString profilePath = profilePathForName(profileName);
    if (!loadSettingsFromPath(profilePath)) {
        LOG_ERROR("ProfileManager::switchToProfile: Failed to load settings for profile '" << profileName << "'.");
        return false;
    }

    d->currentProfileName = profileName;
    LOG_INFO("ProfileManager: Switched to profile '" << profileName << "'. Settings loaded.");
    emit profileSwitched(profileName);
    return true;
}

QString ProfileManager::currentProfileName() const
{
    QMutexLocker locker(&d->mutex);
    return d->currentProfileName;
}

QStringList ProfileManager::profileNames() const
{
    QMutexLocker locker(&d->mutex);
    return d->profiles.keys(); // Returns a copy of the keys (profile names)
}

bool ProfileManager::createProfile(const QString& profileName, const QString& description)
{
    if (profileName.isEmpty()) {
        LOG_ERROR("ProfileManager::createProfile: Profile name cannot be empty.");
        return false;
    }

    QMutexLocker locker(&d->mutex);

    if (d->profiles.contains(profileName)) {
        LOG_ERROR("ProfileManager::createProfile: Profile '" << profileName << "' already exists.");
        return false;
    }

    QString profilePath = profilePathForName(profileName);
    if (!QDir().mkpath(profilePath)) {
        LOG_ERROR("ProfileManager::createProfile: Failed to create profile directory: " << profilePath);
        return false;
    }

    // Create an initial, minimal settings file for the new profile.
    // This could be a copy of the current profile's settings, default settings, or empty.
    // Let's start with an empty settings file that will be populated by Settings::save() when something changes.
    QString settingsPath = profilePath + "/settings.json"; // Or settings.ini, depending on Settings implementation
    QFile settingsFile(settingsPath);
    if (settingsFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QJsonDocument emptyDoc(QJsonObject()); // Start with an empty object
        settingsFile.write(emptyDoc.toJson());
        settingsFile.close();
        LOG_INFO("ProfileManager: Created new profile '" << profileName << "' at: " << profilePath);
    } else {
        LOG_ERROR("ProfileManager::createProfile: Failed to create initial settings file for profile '" << profileName << "': " << settingsFile.errorString());
        QDir().rmdir(profilePath); // Clean up the created directory if settings file creation failed
        return false;
    }

    // Add profile to internal list
    ProfileInfo info;
    info.name = profileName;
    info.description = description;
    info.path = profilePath;
    info.creationDate = QDateTime::currentDateTime();
    info.modificationDate = info.creationDate;
    d->profiles.insert(profileName, info);

    // Update the profiles list file
    if (!saveProfileList()) {
        LOG_WARN("ProfileManager::createProfile: Created profile '" << profileName << "' but failed to update profiles list file.");
        // The profile exists on disk and in memory, but the list file is out of sync.
        // We could remove the profile directory here, but that might be harsh.
        // Let's log and continue, assuming the user can fix the list file or re-initialize.
    }

    emit profileCreated(profileName);
    return true;
}

bool ProfileManager::deleteProfile(const QString& profileName)
{
    if (profileName == "default") {
        LOG_ERROR("ProfileManager::deleteProfile: Cannot delete the 'default' profile.");
        return false;
    }

    QMutexLocker locker(&d->mutex);

    if (!d->profiles.contains(profileName)) {
        LOG_WARN("ProfileManager::deleteProfile: Profile '" << profileName << "' does not exist.");
        return false;
    }

    if (profileName == d->currentProfileName) {
        LOG_ERROR("ProfileManager::deleteProfile: Cannot delete the currently active profile '" << profileName << "'. Switch profiles first.");
        return false;
    }

    QString profilePath = d->profiles.value(profileName).path;
    bool success = QDir(profilePath).removeRecursively(); // Delete the entire profile directory

    if (success) {
        d->profiles.remove(profileName);
        LOG_INFO("ProfileManager: Deleted profile '" << profileName << "' from: " << profilePath);
        if (!saveProfileList()) {
            LOG_WARN("ProfileManager::deleteProfile: Deleted profile '" << profileName << "' but failed to update profiles list file.");
        }
        emit profileDeleted(profileName);
    } else {
        LOG_ERROR("ProfileManager::deleteProfile: Failed to delete profile directory '" << profilePath << "'. Check permissions.");
    }
    return success;
}

bool ProfileManager::renameProfile(const QString& oldName, const QString& newName)
{
    if (oldName.isEmpty() || newName.isEmpty()) {
        LOG_ERROR("ProfileManager::renameProfile: Old or new name is empty.");
        return false;
    }
    if (oldName == newName) {
        LOG_WARN("ProfileManager::renameProfile: Old and new names are the same.");
        return true; // Not an error, just nothing to do
    }
    if (newName == "default") {
        LOG_ERROR("ProfileManager::renameProfile: Cannot rename a profile to 'default'.");
        return false; // Or handle 'default' specially if needed
    }

    QMutexLocker locker(&d->mutex);

    if (!d->profiles.contains(oldName)) {
        LOG_ERROR("ProfileManager::renameProfile: Profile '" << oldName << "' does not exist.");
        return false;
    }

    if (d->profiles.contains(newName)) {
        LOG_ERROR("ProfileManager::renameProfile: Profile '" << newName << "' already exists.");
        return false;
    }

    ProfileInfo info = d->profiles.take(oldName); // Remove from map
    info.name = newName; // Update name in info struct
    QString oldPath = info.path;
    QString newPath = profilePathForName(newName); // Generate new path based on new name
    info.path = newPath;

    // Rename the directory
    bool success = QDir().rename(oldPath, newPath);
    if (success) {
        d->profiles.insert(newName, info); // Re-insert with new key
        LOG_INFO("ProfileManager: Renamed profile from '" << oldName << "' to '" << newName << "'. Path: " << newPath);
        if (!saveProfileList()) {
            LOG_WARN("ProfileManager::renameProfile: Renamed profile '" << newName << "' but failed to update profiles list file.");
        }
        emit profileRenamed(oldName, newName);
    } else {
        LOG_ERROR("ProfileManager::renameProfile: Failed to rename profile directory from '" << oldPath << "' to '" << newPath << "'. Check permissions.");
        // Put the profile info back in the map under the old name as the rename failed
        d->profiles.insert(oldName, info);
    }
    return success;
}

bool ProfileManager::saveCurrentProfile()
{
    QMutexLocker locker(&d->mutex);

    if (d->currentProfileName.isEmpty()) {
        LOG_WARN("ProfileManager::saveCurrentProfile: No active profile to save.");
        return false;
    }

    QString profilePath = profilePathForName(d->currentProfileName);
    QString settingsPath = profilePath + "/settings.json"; // Or the format used by Settings

    // Get current settings from the global Settings instance and save them to the profile-specific path
    if (Settings::instance().saveToPath(settingsPath)) { // Assuming Settings has a saveToPath method
        LOG_DEBUG("ProfileManager: Saved settings for current profile '" << d->currentProfileName << "' to: " << settingsPath);
        // Update modification date in the profile list info
        if (d->profiles.contains(d->currentProfileName)) {
            d->profiles[d->currentProfileName].modificationDate = QDateTime::currentDateTime();
        }
        return true;
    } else {
        LOG_ERROR("ProfileManager::saveCurrentProfile: Failed to save settings for profile '" << d->currentProfileName << "'.");
        return false;
    }
}

bool ProfileManager::exportProfile(const QString& profileName, const QString& exportPath)
{
    if (profileName.isEmpty() || exportPath.isEmpty()) {
        LOG_ERROR("ProfileManager::exportProfile: Profile name or export path is empty.");
        return false;
    }

    QMutexLocker locker(&d->mutex);

    if (!d->profiles.contains(profileName)) {
        LOG_ERROR("ProfileManager::exportProfile: Profile '" << profileName << "' does not exist.");
        return false;
    }

    QString profilePath = d->profiles.value(profileName).path;

    // Use a utility function or QProcess to create an archive (e.g., zip) of the profile directory
    // This is complex and might require an external tool or a library like libzip.
    // For now, let's assume a hypothetical utility function exists.
    // bool success = FileUtils::createZipArchive(profilePath, exportPath);
    Q_UNUSED(profilePath);
    Q_UNUSED(exportPath);
    LOG_WARN("ProfileManager::exportProfile: Not implemented. Requires archiving library or external tool.");
    return false; // Placeholder
}

bool ProfileManager::importProfile(const QString& importPath, const QString& newName)
{
    if (importPath.isEmpty() || newName.isEmpty()) {
        LOG_ERROR("ProfileManager::importProfile: Import path or new name is empty.");
        return false;
    }

    // Use a utility function or QProcess to extract an archive (e.g., zip) to the profiles directory
    // Check if the extracted directory contains valid settings/config files.
    // Add the new profile to the internal list and update the list file.
    // For now, let's assume a hypothetical utility function exists.
    // QString extractedPath = profilesDirectory() + "/" + newName;
    // bool extractSuccess = FileUtils::extractZipArchive(importPath, extractedPath);
    // if (!extractSuccess) { ... return false; ... }
    // bool validateSuccess = validateProfileDirectory(extractedPath); // Hypothetical validator
    // if (!validateSuccess) { ... QDir(extractedPath).removeRecursively(); ... return false; ... }
    // Add profile to internal list and save list file.
    Q_UNUSED(importPath);
    Q_UNUSED(newName);
    LOG_WARN("ProfileManager::importProfile: Not implemented. Requires archiving library or external tool.");
    return false; // Placeholder
}

// --- Helpers ---
bool ProfileManager::loadProfileList()
{
    QString listPath = d->profilesDirectory + "/profiles.json"; // Or profiles.list, profiles.ini, etc.
    QFile listFile(listPath);

    if (!listFile.exists()) {
        LOG_INFO("ProfileManager: Profiles list file does not exist. Assuming first run, creating default profile.");
        // If the list file doesn't exist, it might be the first run. Create a default profile.
        // This logic might be better handled by the initialize() method calling createProfile if d->profiles is empty after attempting load.
        // Let's just return true here, meaning no profiles were loaded from the list file (which is fine if it didn't exist).
        // initialize() will handle creating default if needed.
        return true;
    }

    if (!listFile.open(QIODevice::ReadOnly)) {
        LOG_ERROR("ProfileManager: Failed to open profiles list file for reading: " << listPath << ", Error: " << listFile.errorString());
        return false;
    }

    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(listFile.readAll(), &parseError);
    listFile.close();

    if (parseError.error != QJsonParseError::NoError) {
        LOG_ERROR("ProfileManager: Failed to parse profiles list JSON: " << parseError.errorString());
        return false;
    }

    if (!doc.isObject()) {
        LOG_ERROR("ProfileManager: Profiles list file does not contain a JSON object.");
        return false;
    }

    QJsonObject rootObj = doc.object();
    QJsonValue profilesValue = rootObj.value("profiles");
    if (!profilesValue.isArray()) {
        LOG_ERROR("ProfileManager: Profiles list JSON root object does not have a 'profiles' array.");
        return false;
    }

    QJsonArray profilesArray = profilesValue.toArray();
    d->profiles.clear(); // Clear existing list in memory

    for (const auto& profileValue : profilesArray) {
        if (profileValue.isObject()) {
            QJsonObject profileObj = profileValue.toObject();
            QString name = profileObj.value("name").toString();
            QString description = profileObj.value("description").toString();
            QString path = profileObj.value("path").toString(); // Should be relative or validated absolute
            QString creationStr = profileObj.value("creationDate").toString(); // ISO date string
            QString modificationStr = profileObj.value("modificationDate").toString(); // ISO date string

            if (name.isEmpty() || path.isEmpty()) {
                LOG_WARN("ProfileManager::loadProfileList: Invalid profile entry found in list file (missing name or path), skipping.");
                continue;
            }

            // Validate that the path points to an existing directory within the expected profiles location
            QFileInfo pathInfo(path);
            if (!pathInfo.exists() || !pathInfo.isDir() || !path.startsWith(d->profilesDirectory)) {
                 LOG_WARN("ProfileManager::loadProfileList: Profile '" << name << "' has invalid path: " << path << ". Skipping.");
                 continue;
            }

            ProfileInfo info;
            info.name = name;
            info.description = description;
            info.path = path;
            info.creationDate = QDateTime::fromString(creationStr, Qt::ISODateWithMs);
            info.modificationDate = QDateTime::fromString(modificationStr, Qt::ISODateWithMs);
            if (!info.creationDate.isValid()) info.creationDate = QDateTime::currentDateTime(); // Fallback
            if (!info.modificationDate.isValid()) info.modificationDate = info.creationDate; // Fallback

            d->profiles.insert(name, info);
            LOG_DEBUG("ProfileManager: Loaded profile from list: " << name << " at " << path);
        } else {
            LOG_WARN("ProfileManager::loadProfileList: Non-object entry found in profiles array, skipping.");
        }
    }

    LOG_INFO("ProfileManager: Loaded " << d->profiles.size() << " profiles from list file: " << listPath);
    return true;
}

bool ProfileManager::saveProfileList() const
{
    QMutexLocker locker(&d->mutex);

    QJsonArray profilesArray;
    for (auto it = d->profiles.constBegin(); it != d->profiles.constEnd(); ++it) {
        QJsonObject profileObj;
        profileObj["name"] = it.key();
        profileObj["description"] = it.value().description;
        profileObj["path"] = it.value().path; // Store the full path
        profileObj["creationDate"] = it.value().creationDate.toString(Qt::ISODateWithMs);
        profileObj["modificationDate"] = it.value().modificationDate.toString(Qt::ISODateWithMs);
        profilesArray.append(profileObj);
    }

    QJsonObject rootObj;
    rootObj["profiles"] = profilesArray;

    QJsonDocument doc(rootObj);

    QString listPath = d->profilesDirectory + "/profiles.json";
    QFile listFile(listPath);
    if (!listFile.open(QIODevice::WriteOnly)) {
        LOG_ERROR("ProfileManager: Failed to open profiles list file for writing: " << listPath << ", Error: " << listFile.errorString());
        return false;
    }

    qint64 bytesWritten = listFile.write(doc.toJson());
    listFile.close();

    if (bytesWritten != doc.toJson().size()) {
        LOG_ERROR("ProfileManager: Failed to write full profiles list file: " << listPath);
        return false;
    }

    LOG_DEBUG("ProfileManager: Saved profiles list to: " << listPath);
    return true;
}

QString ProfileManager::profilePathForName(const QString& profileName) const
{
    // Construct the expected path for a profile based on its name.
    // Profile directories are named after the profile.
    return d->profilesDirectory + "/" + profileName;
}

bool ProfileManager::loadSettingsFromPath(const QString& profilePath) const
{
    QString settingsPath = profilePath + "/settings.json"; // Or the format used by Settings
    if (!QFileInfo::exists(settingsPath)) {
        LOG_WARN("ProfileManager::loadSettingsFromPath: Settings file does not exist in profile: " << profilePath << ". Using defaults.");
        Settings::instance().loadDefaults(); // Hypothetical method to reset to defaults
        return true; // Not an error, just means the profile has default settings
    }

    if (Settings::instance().loadFromPath(settingsPath)) { // Assuming Settings has a loadFromPath method
        LOG_DEBUG("ProfileManager: Loaded settings from profile path: " << settingsPath);
        return true;
    } else {
        LOG_ERROR("ProfileManager::loadSettingsFromPath: Failed to load settings from profile: " << settingsPath);
        return false;
    }
}

class ProfileManager::Private {
public:
    Private(ProfileManager* q_ptr)
        : q(q_ptr), initialized(false) {}

    ProfileManager* q;
    mutable QMutex mutex; // Protect access to the profiles map and current profile name
    bool initialized;
    QString profilesDirectory;
    QString currentProfileName;
    QHash<QString, ProfileInfo> profiles; // Map profile name -> ProfileInfo

    struct ProfileInfo {
        QString name;
        QString description;
        QString path; // Full path to the profile directory
        QDateTime creationDate;
        QDateTime modificationDate;
    };
};

} // namespace QuantilyxDoc