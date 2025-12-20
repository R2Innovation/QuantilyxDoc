/**
 * QuantilyxDoc - Professional Document Editor
 * Copyright (C) 2025 R² Innovative Software
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */
#ifndef QUANTILYX_PROFILEMANAGER_H
#define QUANTILYX_PROFILEMANAGER_H

#include <QObject>
#include <QHash>
#include <QDir>
#include <QMutex>
#include <QJsonDocument>
#include <QJsonObject>
#include <memory>

namespace QuantilyxDoc {

/**
 * @brief Manages user profiles, each containing settings, preferences, and layout states.
 * 
 * Allows users to switch between different configurations (e.g., "Writer", "Editor", "Reviewer")
 * that persist their specific settings, window layouts, toolbar configurations, etc.
 */
class ProfileManager : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief Structure representing a user profile.
     */
    struct Profile {
        QString id;                 // Unique identifier (e.g., "default", "writer_mode")
        QString name;               // Display name (e.g., "Default Profile", "Writer Mode")
        QString description;        // Optional description
        QDir settingsDirectory;     // Directory containing this profile's settings/configs
        QDateTime creationTime;     // When the profile was created
        QDateTime lastUsedTime;     // When the profile was last used
        bool isDefault;             // Is this the default profile?
        bool isReadOnly;            // Is this profile locked (e.g., system profile)?
        QJsonObject metadata;       // Additional metadata (e.g., author, version)
    };

    /**
     * @brief Constructor.
     * @param parent Parent object.
     */
    explicit ProfileManager(QObject* parent = nullptr);

    /**
     * @brief Destructor.
     */
    ~ProfileManager() override;

    /**
     * @brief Get singleton instance.
     * @return Reference to the global ProfileManager instance.
     */
    static ProfileManager& instance();

    /**
     * @brief Initialize the profile system.
     * Loads the default profile and any others found in the profiles directory.
     * @return True if initialization was successful.
     */
    bool initialize();

    /**
     * @brief Get the currently active profile.
     * @return Pointer to the active profile, or nullptr if none is active.
     */
    const Profile* currentProfile() const;

    /**
     * @brief Switch to a different profile.
     * Loads the settings and layout associated with the specified profile.
     * @param profileId The ID of the profile to switch to.
     * @return True if the switch was successful.
     */
    bool switchToProfile(const QString& profileId);

    /**
     * @brief Create a new profile based on the current one.
     * Copies settings from the current profile.
     * @param name Display name for the new profile.
     * @param description Optional description.
     * @return The ID of the newly created profile, or an empty string on failure.
     */
    QString createProfile(const QString& name, const QString& description = QString());

    /**
     * @brief Remove a profile.
     * Deletes the profile's directory and settings. Cannot remove the default profile.
     * @param profileId The ID of the profile to remove.
     * @return True if the profile was removed.
     */
    bool removeProfile(const QString& profileId);

    /**
     * @brief Rename an existing profile.
     * @param profileId The ID of the profile to rename.
     * @param newName The new display name.
     * @return True if the rename was successful.
     */
    bool renameProfile(const QString& profileId, const QString& newName);

    /**
     * @brief Get a list of all available profile IDs.
     * @return List of profile IDs.
     */
    QStringList profileIds() const;

    /**
     * @brief Get information about a specific profile.
     * @param profileId The ID of the profile.
     * @return Profile struct, or an invalid one if not found.
     */
    Profile profile(const QString& profileId) const;

    /**
     * @brief Get the directory where profiles are stored.
     * @return Profiles directory path.
     */
    QDir profilesDirectory() const;

    /**
     * @brief Set the directory where profiles are stored.
     * @param dir Profiles directory path.
     */
    void setProfilesDirectory(const QDir& dir);

    /**
     * @brief Get the ID of the default profile.
     * @return Default profile ID.
     */
    QString defaultProfileId() const;

    /**
     * @brief Set the ID of the default profile.
     * @param id Default profile ID.
     */
    void setDefaultProfileId(const QString& id);

    /**
     * @brief Import a profile from an external file/directory.
     * @param importPath Path to the profile archive or directory.
     * @param newName Optional new name for the imported profile.
     * @return The ID of the imported profile, or an empty string on failure.
     */
    QString importProfile(const QString& importPath, const QString& newName = QString());

    /**
     * @brief Export the current or a specific profile to a file/directory.
     * @param profileId The ID of the profile to export (empty for current).
     * @param exportPath Path to export the profile to.
     * @return True if the export was successful.
     */
    bool exportProfile(const QString& profileId, const QString& exportPath);

    /**
     * @brief Save the current state of the application (settings, layout) to the active profile.
     * This is typically called before switching profiles or on application exit.
     */
    void saveCurrentProfile();

    /**
     * @brief Load the state of the application (settings, layout) from the active profile.
     * This is typically called after switching profiles or on application start.
     */
    void loadCurrentProfile();

    /**
     * @brief Get the settings directory path for a specific profile.
     * @param profileId The ID of the profile.
     * @return Settings directory path for the profile.
     */
    QDir profileSettingsDirectory(const QString& profileId) const;

    /**
     * @brief Check if a profile with the given ID exists.
     * @param profileId The ID to check.
     * @return True if the profile exists.
     */
    bool profileExists(const QString& profileId) const;

signals:
    /**
     * @brief Emitted when the active profile changes.
     * @param oldProfileId The ID of the previous profile.
     * @param newProfileId The ID of the new profile.
     */
    void profileChanged(const QString& oldProfileId, const QString& newProfileId);

    /**
     * @brief Emitted when a new profile is created.
     * @param profileId The ID of the created profile.
     */
    void profileCreated(const QString& profileId);

    /**
     * @brief Emitted when a profile is removed.
     * @param profileId The ID of the removed profile.
     */
    void profileRemoved(const QString& profileId);

    /**
     * @brief Emitted when a profile is renamed.
     * @param profileId The ID of the renamed profile.
     * @param oldName The old name.
     * @param newName The new name.
     */
    void profileRenamed(const QString& profileId, const QString& oldName, const QString& newName);

    /**
     * @brief Emitted when the list of available profiles changes.
     */
    void profilesListChanged();

private:
    class Private;
    std::unique_ptr<Private> d;
};

} // namespace QuantilyxDoc

#endif // QUANTILYX_PROFILEMANAGER_H