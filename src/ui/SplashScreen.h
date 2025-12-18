/**
 * QuantilyxDoc - Professional Document Editor
 * Copyright (C) 2025 R² Innovative Software
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#ifndef QUANTILYX_SPLASHSCREEN_H
#define QUANTILYX_SPLASHSCREEN_H

#include <QSplashScreen>
#include <QPixmap>
#include <QString>
#include <memory>

namespace QuantilyxDoc {

/**
 * @brief Splash screen displayed during application startup
 * 
 * Shows application logo, version, company information, and initialization progress.
 */
class SplashScreen : public QSplashScreen
{
    Q_OBJECT

public:
    /**
     * @brief Constructor
     * @param parent Parent widget
     */
    explicit SplashScreen(QWidget* parent = nullptr);

    /**
     * @brief Destructor
     */
    ~SplashScreen();

    /**
     * @brief Show message with progress
     * @param message Message to display
     * @param progress Progress percentage (0-100)
     * @param alignment Text alignment
     * @param color Text color
     */
    void showMessage(const QString& message, int progress = -1,
                    int alignment = Qt::AlignBottom | Qt::AlignHCenter,
                    const QColor& color = Qt::white);

    /**
     * @brief Set progress percentage
     * @param progress Progress (0-100)
     */
    void setProgress(int progress);

    /**
     * @brief Get current progress
     * @return Current progress (0-100)
     */
    int progress() const;

    /**
     * @brief Set minimum display time (milliseconds)
     * @param ms Minimum time to display splash
     */
    void setMinimumTime(int ms);

    /**
     * @brief Check if minimum time has elapsed
     * @return true if minimum time has elapsed
     */
    bool minimumTimeElapsed() const;

protected:
    /**
     * @brief Draw splash screen contents
     * @param painter QPainter object
     */
    void drawContents(QPainter* painter) override;

private:
    /**
     * @brief Load splash screen image
     * @return Splash screen pixmap
     */
    QPixmap loadSplashImage();

    /**
     * @brief Create splash screen content
     * @return Composed splash screen pixmap
     */
    QPixmap createSplashContent();

private:
    class Private;
    std::unique_ptr<Private> d;
};

} // namespace QuantilyxDoc

#endif // QUANTILYX_SPLASHSCREEN_H