/**
 * QuantilyxDoc - Professional Document Editor
 * Copyright (C) 2025 R² Innovative Software
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "SplashScreen.h"
#include "../core/ConfigManager.h"
#include "../utils/Version.h"

#include <QPainter>
#include <QApplication>
#include <QScreen>
#include <QElapsedTimer>
#include <QFont>
#include <QLinearGradient>

namespace QuantilyxDoc {

// Private implementation
class SplashScreen::Private
{
public:
    Private() 
        : progress(0)
        , minimumTime(3000)
    {
        timer.start();
    }

    QString currentMessage;
    int progress;
    int minimumTime;
    QElapsedTimer timer;
};

SplashScreen::SplashScreen(QWidget* parent)
    : QSplashScreen(parent, createSplashContent())
    , d(new Private())
{
    // Center on screen
    QScreen* screen = QApplication::primaryScreen();
    QRect screenGeometry = screen->geometry();
    int x = (screenGeometry.width() - width()) / 2;
    int y = (screenGeometry.height() - height()) / 2;
    move(x, y);

    // Set window flags
    setWindowFlags(Qt::WindowStaysOnTopHint | Qt::FramelessWindowHint | Qt::SplashScreen);

    // Get minimum time from config
    ConfigManager& config = ConfigManager::instance();
    d->minimumTime = config.getInt("General", "splash_timeout", 3000);
}

SplashScreen::~SplashScreen()
{
}

void SplashScreen::showMessage(const QString& message, int progress,
                               int alignment, const QColor& color)
{
    d->currentMessage = message;
    
    if (progress >= 0 && progress <= 100) {
        d->progress = progress;
    }
    
    QSplashScreen::showMessage(message, alignment, color);
    QApplication::processEvents();
}

void SplashScreen::setProgress(int progress)
{
    if (progress >= 0 && progress <= 100) {
        d->progress = progress;
        repaint();
        QApplication::processEvents();
    }
}

int SplashScreen::progress() const
{
    return d->progress;
}

void SplashScreen::setMinimumTime(int ms)
{
    d->minimumTime = ms;
}

bool SplashScreen::minimumTimeElapsed() const
{
    return d->timer.elapsed() >= d->minimumTime;
}

void SplashScreen::drawContents(QPainter* painter)
{
    // Call base implementation for message
    QSplashScreen::drawContents(painter);
    
    // Draw progress bar
    int barWidth = width() - 80;
    int barHeight = 8;
    int barX = (width() - barWidth) / 2;
    int barY = height() - 60;
    
    // Draw progress bar background
    painter->setPen(Qt::NoPen);
    painter->setBrush(QColor(200, 200, 200, 100));
    painter->drawRoundedRect(barX, barY, barWidth, barHeight, 4, 4);
    
    // Draw progress bar fill
    if (d->progress > 0) {
        int fillWidth = (barWidth * d->progress) / 100;
        
        QLinearGradient gradient(barX, barY, barX + fillWidth, barY);
        gradient.setColorAt(0, QColor(52, 152, 219)); // #3498db
        gradient.setColorAt(1, QColor(41, 128, 185)); // #2980b9
        
        painter->setBrush(gradient);
        painter->drawRoundedRect(barX, barY, fillWidth, barHeight, 4, 4);
    }
    
    // Draw progress percentage
    painter->setPen(Qt::white);
    QFont font = painter->font();
    font.setPointSize(9);
    painter->setFont(font);
    
    QString progressText = QString("%1%").arg(d->progress);
    QRect textRect(barX, barY + barHeight + 5, barWidth, 20);
    painter->drawText(textRect, Qt::AlignCenter, progressText);
}

QPixmap SplashScreen::loadSplashImage()
{
    // Try to load splash screen image from resources
    QPixmap splash(":/images/splashScreen.png");
    
    if (splash.isNull()) {
        // If image not found, create a default splash
        splash = QPixmap(800, 480);
        splash.fill(QColor(44, 62, 80)); // #2c3e50
    }
    
    return splash;
}

QPixmap SplashScreen::createSplashContent()
{
    QPixmap base = loadSplashImage();
    QPixmap result(base.size());
    result.fill(Qt::transparent);
    
    QPainter painter(&result);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::TextAntialiasing);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);
    
    // Draw base image
    painter.drawPixmap(0, 0, base);
    
    // Add dark overlay for better text visibility
    painter.fillRect(result.rect(), QColor(0, 0, 0, 100));
    
    int width = result.width();
    int height = result.height();
    
    // Draw company logo (R² Innovative Software)
    QPixmap companyLogo(":/images/RSqrTech.png");
    if (!companyLogo.isNull()) {
        int logoSize = 120;
        companyLogo = companyLogo.scaled(logoSize, logoSize, 
                                         Qt::KeepAspectRatio, 
                                         Qt::SmoothTransformation);
        int logoX = (width - companyLogo.width()) / 2;
        int logoY = 40;
        painter.drawPixmap(logoX, logoY, companyLogo);
    }
    
    // Draw application logo
    QPixmap appLogo(":/images/QuantilyxDoc.png");
    if (!appLogo.isNull()) {
        int logoSize = 128;
        appLogo = appLogo.scaled(logoSize, logoSize, 
                                 Qt::KeepAspectRatio, 
                                 Qt::SmoothTransformation);
        int logoX = (width - appLogo.width()) / 2;
        int logoY = 180;
        painter.drawPixmap(logoX, logoY, appLogo);
    }
    
    // Draw application name
    painter.setPen(Qt::white);
    QFont nameFont("Sans Serif", 32, QFont::Bold);
    painter.setFont(nameFont);
    
    QRect nameRect(0, 320, width, 50);
    painter.drawText(nameRect, Qt::AlignCenter, "QuantilyxDoc");
    
    // Draw version
    QFont versionFont("Sans Serif", 12);
    painter.setFont(versionFont);
    painter.setPen(QColor(52, 152, 219)); // #3498db
    
    QRect versionRect(0, 370, width, 20);
    QString versionText = QString("Version %1").arg(QUANTILYXDOC_VERSION_STRING);
    painter.drawText(versionRect, Qt::AlignCenter, versionText);
    
    // Draw company name
    QFont companyFont("Sans Serif", 10);
    painter.setFont(companyFont);
    painter.setPen(QColor(189, 195, 199)); // #bdc3c7
    
    QRect companyRect(0, 395, width, 20);
    painter.drawText(companyRect, Qt::AlignCenter, "R² Innovative Software");
    
    // Draw motto
    QFont mottoFont("Sans Serif", 9, QFont::Light);
    painter.setFont(mottoFont);
    painter.setPen(QColor(149, 165, 166)); // #95a6a6
    
    QRect mottoRect(0, 415, width, 20);
    painter.drawText(mottoRect, Qt::AlignCenter, 
                    "\"Where innovation is the key to success\"");
    
    painter.end();
    
    return result;
}

} // namespace QuantilyxDoc