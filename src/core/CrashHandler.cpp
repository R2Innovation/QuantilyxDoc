/**
 * QuantilyxDoc - Professional Document Editor
 * Copyright (C) 2025 R² Innovative Software
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */
#include "CrashHandler.h"
#include "Logger.h"
#include "Settings.h"
#include "utils/FileUtils.h" // Assuming this exists for path operations
#include <QStandardPaths>
#include <QDir>
#include <QDateTime>
#include <QMutex>
#include <QMutexLocker>
#include <QCoreApplication>
#include <QThread>
#include <QDebug>
#include <signal.h> // For POSIX signals (Linux/macOS)
#include <setjmp.h> // For setjmp/longjmp (not recommended for C++, use exceptions)
#include <exception> // For standard C++ exceptions
// #include <windows.h> // For Windows-specific crash handling (e.g., SetUnhandledExceptionFilter)
// #include <dbghelp.h> // For Windows minidump generation (requires linking to dbghelp.lib)

namespace QuantilyxDoc {

// Static instance pointer
CrashHandler* CrashHandler::s_instance = nullptr;

CrashHandler& CrashHandler::instance()
{
    if (!s_instance) {
        s_instance = new CrashHandler();
    }
    return *s_instance;
}

CrashHandler::CrashHandler(QObject* parent)
    : QObject(parent)
    , d(new Private(this))
{
    LOG_INFO("CrashHandler created.");
}

CrashHandler::~CrashHandler()
{
    uninstall(); // Ensure handler is uninstalled on destruction
    LOG_INFO("CrashHandler destroyed.");
}

bool CrashHandler::install()
{
    QMutexLocker locker(&d->mutex);
    if (d->handlerInstalled) {
        LOG_WARN("CrashHandler::install: Handler is already installed.");
        return true;
    }

    LOG_INFO("Installing crash handler...");

    bool success = true;
    d->crashDumpPathStr = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/crash_dumps";
    QDir().mkpath(d->crashDumpPathStr); // Ensure directory exists

#ifdef Q_OS_WIN
    // --- Windows-specific crash handling ---
    // Use SetUnhandledExceptionFilter
    SetUnhandledExceptionFilter(&CrashHandler::Private::windowsExceptionHandler);
    LOG_DEBUG("CrashHandler: Installed Windows unhandled exception filter.");
    // Note: Minidump generation requires more setup (SymInitialize, MiniDumpWriteDump).
    // This is complex and often handled by external libraries like Breakpad or Crashpad.
    // For now, we'll just log the crash attempt.
    // d->minidumpEnabled = Settings::instance().value<bool>("CrashReporting/EnableMiniDump", true);
    d->minidumpEnabled = false; // Disable for now without proper library integration
    if (d->minidumpEnabled) {
        // Initialize DbgHelp if minidump is enabled
        // SymInitialize(GetCurrentProcess(), nullptr, TRUE);
        LOG_INFO("CrashHandler: Minidump generation is enabled (requires DbgHelp library integration).");
    }
#elif defined(Q_OS_UNIX) // Covers Linux, macOS, BSD
    // --- Unix/Linux/macOS-specific crash handling ---
    // Install signal handlers for common crash signals
    signal(SIGSEGV, &CrashHandler::Private::posixSignalHandler); // Segmentation fault
    signal(SIGABRT, &CrashHandler::Private::posixSignalHandler); // Abort signal (e.g., from assert)
    signal(SIGFPE, &CrashHandler::Private::posixSignalHandler); // Floating-point exception
    signal(SIGILL, &CrashHandler::Private::posixSignalHandler); // Illegal instruction
    // SIGBUS might also be relevant on some Unix systems
    LOG_DEBUG("CrashHandler: Installed POSIX signal handlers for SEGV, ABRT, FPE, ILL.");
#else
    LOG_WARN("CrashHandler: No native crash handler available for this platform. Crashes may not be caught.");
    success = false; // Or keep success = true if the handler is optional/non-critical
#endif

    if (success) {
        d->handlerInstalled = true;
        LOG_INFO("Crash handler installed successfully.");
        emit handlerInstalled(true);
    } else {
        LOG_ERROR("Failed to install crash handler.");
        emit handlerInstalled(false);
    }
    return success;
}

void CrashHandler::uninstall()
{
    QMutexLocker locker(&d->mutex);
    if (!d->handlerInstalled) {
        LOG_DEBUG("CrashHandler::uninstall: Handler was not installed.");
        return;
    }

    LOG_INFO("Uninstalling crash handler...");

#ifdef Q_OS_WIN
    SetUnhandledExceptionFilter(nullptr);
    if (d->minidumpEnabled) {
        // SymCleanup(GetCurrentProcess()); // Clean up symbol handler if initialized
    }
#elif defined(Q_OS_UNIX)
    signal(SIGSEGV, SIG_DFL); // Reset to default handler
    signal(SIGABRT, SIG_DFL);
    signal(SIGFPE, SIG_DFL);
    signal(SIGILL, SIG_DFL);
#endif

    d->handlerInstalled = false;
    LOG_INFO("Crash handler uninstalled.");
    emit handlerUninstalled();
}

bool CrashHandler::isInstalled() const
{
    QMutexLocker locker(&d->mutex);
    return d->handlerInstalled;
}

QString CrashHandler::crashDumpPath() const
{
    QMutexLocker locker(&d->mutex);
    return d->crashDumpPathStr;
}

void CrashHandler::setCrashDumpPath(const QString& path)
{
    QMutexLocker locker(&d->mutex);
    if (d->crashDumpPathStr != path) {
        d->crashDumpPathStr = path;
        LOG_INFO("CrashHandler: Dump path set to: " << path);
        // QDir().mkpath(path); // Ensure new path exists? Maybe do this just before writing a dump.
    }
}

bool CrashHandler::isMinidumpEnabled() const
{
    QMutexLocker locker(&d->mutex);
    return d->minidumpEnabled;
}

void CrashHandler::setMinidumpEnabled(bool enabled)
{
    QMutexLocker locker(&d->mutex);
    if (d->minidumpEnabled != enabled) {
        d->minidumpEnabled = enabled;
        LOG_INFO("CrashHandler: Minidump generation set to: " << enabled);
    }
}

// --- Static Handler Functions ---

#ifdef Q_OS_WIN
LONG CrashHandler::Private::windowsExceptionHandler(PEXCEPTION_POINTERS ExceptionInfo)
{
    // This runs in a compromised context. Be extremely careful.
    // Do not call C++ destructors, allocate memory, or call most Win32 APIs.
    // Log to a pre-opened file handle or use DebugBreak/OutputDebugString potentially.

    LOG_CRITICAL("Windows Unhandled Exception occurred! Code: 0x" << QString::number(ExceptionInfo->ExceptionRecord->ExceptionCode, 16));

    // Generate minidump if enabled
    if (d->minidumpEnabled) {
        generateMinidumpWin(ExceptionInfo); // Hypothetical function using DbgHelp
    }

    // Attempt to notify the main thread/application
    // This is tricky. Could try QueueUserAPC, PostThreadMessage, or a pipe.
    // For now, we'll just try to print to stderr which might be captured.
    fprintf(stderr, "QuantilyxDoc crashed (Windows Exception 0x%08x)\n", ExceptionInfo->ExceptionRecord->ExceptionCode);
    fflush(stderr);

    // Return EXCEPTION_EXECUTE_HANDLER to terminate the process,
    // or EXCEPTION_CONTINUE_SEARCH to let other handlers try.
    // Usually, we want to terminate after logging/generating dump.
    return EXCEPTION_EXECUTE_HANDLER;
}

void CrashHandler::Private::generateMinidumpWin(PEXCEPTION_POINTERS ExceptionInfo)
{
    // This function uses Windows DbgHelp API to write a .dmp file.
    // It requires significant setup and is quite complex.
    // This is a stub outline.
    LOG_WARN("CrashHandler::generateMinidumpWin: Requires DbgHelp API integration (SymInitialize, MiniDumpWriteDump). Not implemented in stub.");
    Q_UNUSED(ExceptionInfo);
    // HANDLE hDumpFile = CreateFile(...);
    // MINIDUMP_EXCEPTION_INFORMATION dumpInfo;
    // dumpInfo.ThreadId = GetCurrentThreadId();
    // dumpInfo.ExceptionPointers = ExceptionInfo;
    // dumpInfo.ClientPointers = FALSE;
    // BOOL success = MiniDumpWriteDump(...);
    // CloseHandle(hDumpFile);
}
#endif

#ifdef Q_OS_UNIX
void CrashHandler::Private::posixSignalHandler(int sig)
{
    // This runs in a signal context. Be extremely careful.
    // Async-signal-safe functions only (e.g., write, close, sigprocmask, but NOT malloc, printf, Qt functions, C++ constructors/destructors).
    // Log to a pre-opened file or use low-level system calls.

    const char* sigName = "UNKNOWN";
    switch (sig) {
        case SIGSEGV: sigName = "SIGSEGV"; break;
        case SIGABRT: sigName = "SIGABRT"; break;
        case SIGFPE:  sigName = "SIGFPE";  break;
        case SIGILL:  sigName = "SIGILL";  break;
        default: break;
    }

    char msg[256];
    int len = snprintf(msg, sizeof(msg), "QuantilyxDoc received fatal signal: %s (%d)\n", sigName, sig);
    if (len > 0 && len < (int)sizeof(msg)) {
        write(STDERR_FILENO, msg, len); // Write directly to stderr
    }

    // Attempt to generate a core dump or log state if possible.
    // Generating a minidump equivalent on Unix is often done by external tools or libraries (e.g., Google Breakpad).
    // A simple approach might be to log stack trace using backtrace() if available and linked correctly.
    // For now, we just log the signal and terminate.
    // The OS might generate a core dump based on system settings (ulimit -c).

    // Raise the signal again with default handler to actually terminate
    signal(sig, SIG_DFL);
    raise(sig);
}
#endif

// --- Private Helper Implementation ---
class CrashHandler::Private {
public:
    Private(CrashHandler* q_ptr)
        : q(q_ptr), handlerInstalled(false), minidumpEnabled(false) {}

    CrashHandler* q;
    mutable QMutex mutex; // Protect access to state variables
    bool handlerInstalled;
    bool minidumpEnabled;
    QString crashDumpPathStr;

#ifdef Q_OS_WIN
    static LONG WINAPI windowsExceptionHandler(PEXCEPTION_POINTERS ExceptionInfo);
    static void generateMinidumpWin(PEXCEPTION_POINTERS ExceptionInfo);
#endif

#ifdef Q_OS_UNIX
    static void posixSignalHandler(int sig);
#endif
};

} // namespace QuantilyxDoc