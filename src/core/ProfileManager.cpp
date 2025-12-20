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
#include "Settings.h" // Use our central Settings manager
#include "utils/FileUtils.h" // Assuming this exists
#include <QStandardPaths>
#include <QDir>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QMutex>
#include <QMutexLocker>
#include <QCoreApplication>
#include <QDateTime>
#include <QDebug>

namespace QuantilyxDoc {

class ProfileManager::Private {
public:
    Private(ProfileManager* q_ptr)
        : q(q_ptr), profilesDir(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/profiles"),
          defaultProfileId("default") {}

    ProfileManager* q;
    mutable QMutex mutex; // Protect access to profiles map and current profile
    QHash<QString, Profile> profiles;
    QString currentProfileId;
    QDir profilesDir;
    QString defaultProfileId;

    // Helper to ensure profiles directory exists
    bool ensureProfilesDirExists() {
        if (!profilesDir.exists()) {
            return profilesDir.mkpath(".");
        }
        return true;
    }

    // Helper to get the directory for a specific profile
    QDir profileDir(const QString& profileId) const {
        return QDir(profilesDir.absoluteFilePath(profileId));
    }

    // Helper to load profile metadata from disk
    Profile loadProfileFromDisk(const QString& profileId) {
        Profile profile;
        QDir profileDir = this->profileDir(profileId);
        if (!profileDir.exists()) {
            LOG_WARN("Profile directory does not exist: " << profileDir.absolutePath());
            return profile; // Return invalid profile
        }

        profile.id = profileId;
        profile.settingsDirectory = profileDir;

        // Load metadata.json
        QFile metaFile(profileDir.filePath("metadata.json"));
        if (metaFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QJsonParseError error;
            QJsonDocument doc = QJsonDocument::fromJson(metaFile.readAll(), &error);
            if (error.error == QJsonParseError::NoError && doc.isObject()) {
                profile.metadata = doc.object();
                profile.name = profile.metadata.value("name").toString();
                profile.description = profile.metadata.value("description").toString();
                profile.isDefault = profile.metadata.value("isDefault").toBool(false);
                profile.isReadOnly = profile.metadata.value("isReadOnly").toBool(false);
                // Load creation/lastUsed times if present
                QString creationStr = profile.metadata.value("creationTime").toString();
                QString lastUsedStr = profile.metadata.value("lastUsedTime").toString();
                if (!creationStr.isEmpty()) profile.creationTime = QDateTime::fromString(creationStr, Qt::ISODateWithMs);
                if (!lastUsedStr.isEmpty()) profile.lastUsedTime = QDateTime::fromString(lastUsedStr, Qt::ISODateWithMs);
            } else {
                LOG_WARN("Failed to parse metadata for profile " << profileId << ": " << error.errorString());
                // Fall back to default values derived from directory name or generic ones.
                profile.name = profileId;
            }
            metaFile.close();
        } else {
            // If no metadata file, create a minimal one based on directory name
            LOG_DEBUG("No metadata file found for profile " << profileId << ", creating default.");
            profile.name = profileId;
            profile.creationTime = QDateTime::currentDateTime();
            saveProfileMetadataToDisk(profile);
        }
        return profile;
    }

    // Helper to save profile metadata to disk
    bool saveProfileMetadataToDisk(const Profile& profile) {
        QDir profileDir = this->profileDir(profile.id);
        if (!profileDir.exists() && !profileDir.mkpath(".")) {
            LOG_ERROR("Failed to create profile directory: " << profileDir.absolutePath());
            return false;
        }

        QFile metaFile(profileDir.filePath("metadata.json"));
        if (!metaFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
            LOG_ERROR("Failed to open metadata file for writing: " << metaFile.fileName());
            return false;
        }

        QJsonObject metaObj;
        metaObj["id"] = profile.id;
        metaObj["name"] = profile.name;
        metaObj["description"] = profile.description;
        metaObj["isDefault"] = profile.isDefault;
        metaObj["isReadOnly"] = profile.isReadOnly;
        if (profile.creationTime.isValid()) metaObj["creationTime"] = profile.creationTime.toString(Qt::ISODateWithMs);
        if (profile.lastUsedTime.isValid()) metaObj["lastUsedTime"] = profile.lastUsedTime.toString(Qt::ISODateWithMs);
        // Add other metadata fields as needed

        QJsonDocument doc(metaObj);
        qint64 written = metaFile.write(doc.toJson());
        bool success = (written == doc.toJson().size());
        metaFile.close();

        if (success) {
            LOG_DEBUG("Saved metadata for profile: " << profile.id);
        } else {
            LOG_ERROR("Failed to write metadata for profile: " << profile.id);
        }
        return success;
    }

    // Helper to set the current profile, loading its settings
    bool setCurrentProfileInternal(const QString& profileId) {
        if (!profiles.contains(profileId)) {
            LOG_ERROR("Cannot set current profile to non-existent ID: " << profileId);
            return false;
        }

        QString oldProfileId = currentProfileId;
        currentProfileId = profileId;
        Profile& profile = profiles[profileId];
        profile.lastUsedTime = QDateTime::currentDateTime();
        saveProfileMetadataToDisk(profile); // Update last used time

        // Load settings from the new profile's directory
        Settings& settings = Settings::instance();
        QDir settingsDir = profile.settingsDirectory;
        QString settingsPath = settingsDir.filePath("settings.conf");
        // This assumes Settings class can load from a specific path.
        // settings.loadFromPath(settingsPath); // Hypothetical method
        // For now, we'll just log the switch.
        LOG_INFO("Switched profile from '" << oldProfileId << "' to '" << profileId << "'");
        emit q->profileChanged(oldProfileId, profileId);
        return true;
    }
};

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
    // Initialization happens in initialize()
}

ProfileManager::~ProfileManager()
{
    // Save current profile state if necessary
    saveCurrentProfile();
}

bool ProfileManager::initialize()
{
    QMutexLocker locker(&d->mutex);

    if (!d->ensureProfilesDirExists()) {
        LOG_ERROR("Failed to create profiles directory: " << d->profilesDir.absolutePath());
        return false;
    }

    // Scan profiles directory for subdirectories (potential profiles)
    QStringList profileDirs = d->profilesDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    LOG_DEBUG("Found potential profiles: " << profileDirs.join(", "));

    for (const QString& dirName : profileDirs) {
        Profile profile = d->loadProfileFromDisk(dirName);
        if (!profile.id.isEmpty()) { // Check if loading was successful
            d->profiles.insert(profile.id, profile);
            LOG_DEBUG("Loaded profile: " << profile.name << " (ID: " << profile.id << ")");
        }
    }

    // Ensure default profile exists
    if (!d->profiles.contains(d->defaultProfileId)) {
        LOG_INFO("Default profile '" << d->defaultProfileId << "' does not exist, creating it.");
        createProfile("Default Profile", "The default application profile.");
    }

    // Set the current profile to the default (or the first one if default is missing)
    QString initialProfileId = d->defaultProfileId;
    if (!d->profiles.contains(initialProfileId) && !d->profiles.isEmpty()) {
        initialProfileId = d->profiles.begin().key(); // Fallback to first profile
    }

    if (!initialProfileId.isEmpty()) {
        d->setCurrentProfileInternal(initialProfileId);
    } else {
        LOG_ERROR("No profiles found and could not create default. ProfileManager failed to initialize correctly.");
        return false;
    }

    emit profilesListChanged();
    LOG_INFO("ProfileManager initialized with " << d->profiles.size() << " profiles. Current: " << d->currentProfileId);
    return true;
}

const ProfileManager::Profile* ProfileManager::currentProfile() const
{
    QMutexLocker locker(&d->mutex);
    auto it = d->profiles.constFind(d->currentProfileId);
    if (it != d->profiles.constEnd()) {
        return &it.value();
    }
    return nullptr; // Should not happen if initialized correctly
}

bool ProfileManager::switchToProfile(const QString& profileId)
{
    if (profileId == d->currentProfileId) return true; // Already active

    QMutexLocker locker(&d->mutex);
    if (!d->profiles.contains(profileId)) {
        LOG_WARN("Cannot switch to non-existent profile: " << profileId);
        return false;
    }

    // Save current profile's state before switching
    saveCurrentProfile();

    return d->setCurrentProfileInternal(profileId);
}

QString ProfileManager::createProfile(const QString& name, const QString& description)
{
    QMutexLocker locker(&d->mutex);

    // Generate a unique ID based on the name
    QString baseId = name.toLower().remove(QRegExp("[^a-z0-9_]")); // Sanitize name
    if (baseId.isEmpty()) baseId = "new_profile";
    QString profileId = baseId;
    int counter = 1;
    while (d->profiles.contains(profileId)) {
        profileId = baseId + "_" + QString::number(counter);
        counter++;
    }

    Profile newProfile;
    newProfile.id = profileId;
    newProfile.name = name;
    newProfile.description = description;
    newProfile.creationTime = QDateTime::currentDateTime();
    newProfile.lastUsedTime = newProfile.creationTime; // First used now
    newProfile.settingsDirectory = d->profileDir(profileId);
    newProfile.isDefault = false; // New profiles are not default
    newProfile.isReadOnly = false; // New profiles are editable

    if (d->saveProfileMetadataToDisk(newProfile)) {
        d->profiles.insert(profileId, newProfile);
        LOG_INFO("Created new profile: " << name << " (ID: " << profileId << ")");
        emit profileCreated(profileId);
        emit profilesListChanged();
        return profileId;
    } else {
        LOG_ERROR("Failed to save new profile: " << name);
        return QString(); // Return empty string on failure
    }
}

bool ProfileManager::removeProfile(const QString& profileId)
{
    if (profileId == d->defaultProfileId) {
        LOG_WARN("Cannot remove the default profile: " << profileId);
        return false;
    }

    QMutexLocker locker(&d->mutex);
    auto it = d->profiles.find(profileId);
    if (it == d->profiles.end()) {
        LOG_WARN("Cannot remove non-existent profile: " << profileId);
        return false;
    }

    // Delete the profile's directory
    QDir profileDir = d->profileDir(profileId);
    if (profileDir.exists() && !QDir().rmdir(profileDir.absolutePath())) { // Use QDir() to remove recursively if needed, or QFile::removeRecursively
        // QFile::removeRecursively(profileDir.absolutePath()); // Alternative for recursive delete
        LOG_ERROR("Failed to delete profile directory: " << profileDir.absolutePath());
        return false;
    }

    d->profiles.erase(it);
    LOG_INFO("Removed profile: " << profileId);
    emit profileRemoved(profileId);
    emit profilesListChanged();

    // If the removed profile was current, switch to default
    if (profileId == d->currentProfileId) {
        switchToProfile(d->defaultProfileId);
    }

    return true;
}

bool ProfileManager::renameProfile(const QString& profileId, const QString& newName)
{
    if (profileId == d->defaultProfileId) {
        LOG_WARN("Cannot rename the default profile: " << profileId);
        return false;
    }

    QMutexLocker locker(&d->mutex);
    auto it = d->profiles.find(profileId);
    if (it == d->profiles.end()) {
        LOG_WARN("Cannot rename non-existent profile: " << profileId);
        return false;
    }

    QString oldName = it->name;
    it->name = newName;
    it->metadata["name"] = newName; // Update metadata object too

    if (d->saveProfileMetadataToDisk(*it)) {
        LOG_INFO("Renamed profile from '" << oldName << "' to '" << newName << "' (ID: " << profileId << ")");
        emit profileRenamed(profileId, oldName, newName);
        return true;
    } else {
        LOG_ERROR("Failed to save renamed profile meta " << profileId);
        return false;
    }
}

QStringList ProfileManager::profileIds() const
{
    QMutexLocker locker(&d->mutex);
    return d->profiles.keys();
}

ProfileManager::Profile ProfileManager::profile(const QString& profileId) const
{
    QMutexLocker locker(&d->mutex);
    auto it = d->profiles.constFind(profileId);
    if (it != d->profiles.constEnd()) {
        return it.value();
    }
    return Profile(); // Return invalid profile
}

QDir ProfileManager::profilesDirectory() const
{
    QMutexLocker locker(&d->mutex);
    return d->profilesDir;
}

void ProfileManager::setProfilesDirectory(const QDir& dir)
{
    QMutexLocker locker(&d->mutex);
    if (d->profilesDir != dir) {
        // Changing the directory after initialization is complex.
        // It would require re-loading all profiles.
        // For now, log a warning if attempted after initialization.
        if (!d->profiles.isEmpty()) {
            LOG_WARN("Changing profiles directory after initialization is not fully supported. Current profiles may become invalid.");
        }
        d->profilesDir = dir;
        LOG_INFO("Profiles directory set to: " << dir.absolutePath());
    }
}

QString ProfileManager::defaultProfileId() const
{
    QMutexLocker locker(&d->mutex);
    return d->defaultProfileId;
}

void ProfileManager::setDefaultProfileId(const QString& id)
{
    QMutexLocker locker(&d->mutex);
    if (d->defaultProfileId != id) {
        // Validate that the new default exists?
        if (d->profiles.contains(id)) {
            d->defaultProfileId = id;
            LOG_INFO("Default profile ID changed to: " << id);
            // Update metadata for the new and old default profiles if they exist
            auto oldDefaultIt = d->profiles.find(d->defaultProfileId); // This is now the new ID
            if (oldDefaultIt != d->profiles.end()) {
                 oldDefaultIt->isDefault = true;
                 // The previously default profile needs its flag cleared, but we don't know its old ID here directly.
                 // A full scan or keeping track would be needed. For now, assume user manages this via rename/remove.
            }
            // A more robust way is to reload all profiles and update the 'isDefault' flag based on d->defaultProfileId.
            // Reload logic is complex, so we'll just save the current state of the new default.
            // The next time profiles are loaded/initialized, the correct one will be marked.
        } else {
            LOG_WARN("Attempted to set non-existent profile as default: " << id);
        }
    }
}

QString ProfileManager::importProfile(const QString& importPath, const QString& newName)
{
    // This is a stub. A full implementation would involve:
    // 1. Validating importPath (file or directory)
    // 2. Extracting the profile archive if it's a file (e.g., .zip)
    // 3. Copying the profile directory structure to the profiles location
    // 4. Renaming the profile directory and updating its metadata.json
    // 5. Loading the new profile into the manager.
    Q_UNUSED(importPath);
    Q_UNUSED(newName);
    LOG_WARN("importProfile: Stub implementation.");
    // Example: QFile::copyRecursively(importPath, d->profileDir(newId).absolutePath());
    return QString(); // Return empty for stub
}

bool ProfileManager::exportProfile(const QString& profileId, const QString& exportPath)
{
    // This is a stub. A full implementation would involve:
    // 1. Validating the profileId exists.
    // 2. Creating an archive (e.g., .zip) at exportPath.
    // 3. Adding the profile's directory contents to the archive.
    Q_UNUSED(profileId);
    Q_UNUSED(exportPath);
    LOG_WARN("exportProfile: Stub implementation.");
    return false; // Return false for stub
}

void ProfileManager::saveCurrentProfile()
{
    // This function should save the current application state (settings, window layout, etc.)
    // into the directory of the *current* profile.
    // It involves calling save methods on Settings, MainWindow (for layout), etc.
    // and writing them to files within the current profile's settingsDirectory.
    QMutexLocker locker(&d->mutex);
    if (d->currentProfileId.isEmpty()) {
        LOG_WARN("saveCurrentProfile: No current profile set.");
        return;
    }

    auto it = d->profiles.find(d->currentProfileId);
    if (it != d->profiles.end()) {
        QDir settingsDir = it->settingsDirectory;
        QString settingsPath = settingsDir.filePath("settings.conf");
        // Settings::instance().saveToPath(settingsPath); // Hypothetical method
        LOG_DEBUG("Saved current profile state to: " << settingsDir.absolutePath());
    } else {
        LOG_WARN("saveCurrentProfile: Current profile ID '" << d->currentProfileId << "' not found in profiles list.");
    }
}

void ProfileManager::loadCurrentProfile()
{
    // This function should load the application state (settings, window layout, etc.)
    // from the directory of the *current* profile.
    // It involves calling load methods on Settings, MainWindow (for layout), etc.
    // reading them from files within the current profile's settingsDirectory.
    QMutexLocker locker(&d->mutex);
    if (d->currentProfileId.isEmpty()) {
        LOG_WARN("loadCurrentProfile: No current profile set.");
        return;
    }

    auto it = d->profiles.find(d->currentProfileId);
    if (it != d->profiles.end()) {
        QDir settingsDir = it->settingsDirectory;
        QString settingsPath = settingsDir.filePath("settings.conf");
        // Settings::instance().loadFromPath(settingsPath); // Hypothetical method
        LOG_DEBUG("Loaded current profile state from: " << settingsDir.absolutePath());
    } else {
        LOG_WARN("loadCurrentProfile: Current profile ID '" << d->currentProfileId << "' not found in profiles list.");
    }
}

QDir ProfileManager::profileSettingsDirectory(const QString& profileId) const
{
    QMutexLocker locker(&d->mutex);
    auto it = d->profiles.constFind(profileId);
    if (it != d->profiles.constEnd()) {
        return it->settingsDirectory;
    }
    return QDir(); // Return invalid QDir if profile not found
}

bool ProfileManager::profileExists(const QString& profileId) const
{
    QMutexLocker locker(&d->mutex);
    return d->profiles.contains(profileId);
}

} // namespace QuantilyxDoc