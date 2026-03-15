#include "windows_fix.h"  // CRITICAL: Qt 6.8+ fix - MUST be FIRST
/**
 * @file LispProcessExecutor.cpp
 * @brief In-process LISP Execution via BricsCAD _.SCRIPT command
 * @version 5.1.0 - Fixed: system vars via acedSetVar, no blank lines in SCR
 * 
 * Standard CAD batch pattern: SCR-based sequential DWG processing.
 * 1. Set FILEDIA/CMDECHO/EXPERT via acedSetVar() in C++ (BEFORE script)
 * 2. Generate lean SCR file (only file operations, no system var setup)
 * 3. Execute _.SCRIPT in the CURRENT BricsCAD instance via acedCommand
 * 4. Restore system vars at end of SCR
 * 
 * SCR Structure (v5.1 - lean, no blank lines):
 *   _.OPEN "file1.dwg"
 *   (progn (load "sk1.lsp")(princ))
 *   (progn (sk1)(princ))
 *   _QSAVE
 *   _.CLOSE
 *   _.OPEN "file2.dwg"
 *   ...
 *   ; Cleanup: restore settings
 *   (progn (setvar "FILEDIA" 1)(princ))
 *   (progn (setvar "CMDECHO" 1)(princ))
 *   (progn (setvar "EXPERT" 0)(princ))
 *
 * Convention: LISP function name = filename without extension
 * Example: "sk1.lsp" → function call: (sk1)
 */

// BRX Platform headers FIRST
#ifdef __linux__
#include "brx_platform_linux.h"
#else
#include "brx_platform_windows.h"
#endif

// BRX API headers
#include "aced.h"
#include "AcApDMgr.h"   // acDocManager->sendStringToExecute

#include "LispProcessExecutor.h"
#include <QDir>
#include <QFileInfo>
#include <QDebug>
#include <QDateTime>
#include <QTextStream>
#include <QStandardPaths>

// BRX result codes (ensure they're defined)
#ifndef RTNORM
#define RTNORM   5100
#endif
#ifndef RTERROR
#define RTERROR  -5001
#endif
#ifndef RTSTR
#define RTSTR    5005
#endif
#ifndef RTNONE
#define RTNONE   5000
#endif
#ifndef RTSHORT
#define RTSHORT  5003
#endif

namespace BatchProcessing {

// ============================================================================
// Constructor
// ============================================================================

LispProcessExecutor::LispProcessExecutor(QObject* parent) : QObject(parent) {
    logDebug("LispProcessExecutor v5.1 - In-process via _.SCRIPT");
}

// ============================================================================
// MAIN EXECUTION
// ============================================================================

bool LispProcessExecutor::executeLispBatch(const LispExecutionConfig& config, LispExecutionResult& result) {
    qint64 startTime = QDateTime::currentMSecsSinceEpoch();
    
    logInfo("=== Starting LISP Batch Execution (In-Process v5.1) ===");
    logDebug(QString("Files: %1, Scripts: %2").arg(config.dwgFiles.size()).arg(config.lispScripts.size()));
    
    result = LispExecutionResult();
    
    // --- Validate inputs ---
    if (config.lispScripts.isEmpty()) {
        QString error = "No LISP scripts specified";
        result.errors << error;
        result.summary = "Failed: " + error;
        logError(error);
        emit errorOccurred(error);
        return false;
    }
    if (config.dwgFiles.isEmpty()) {
        QString error = "No DWG files specified";
        result.errors << error;
        result.summary = "Failed: " + error;
        logError(error);
        emit errorOccurred(error);
        return false;
    }
    
    // Validate LISP scripts exist
    for (const QString& lsp : config.lispScripts) {
        if (!QFileInfo::exists(lsp)) {
            QString error = QString("LISP script not found: %1").arg(lsp);
            result.errors << error;
            logError(error);
            emit errorOccurred(error);
            return false;
        }
    }
    
    // Validate: no DWG from the batch list should be currently open
    if (!validateNoFilesOpen(config.dwgFiles, result)) {
        return false;
    }
    
    emit processStarted();
    emit logMessage("Starting batch processing (in-process)...");
    
    // Create temp directory for the generated SCR
    QString tempDir = createTempDirectory();
    if (tempDir.isEmpty()) {
        QString error = "Failed to create temporary directory";
        result.errors << error;
        result.summary = "Failed: " + error;
        logError(error);
        emit errorOccurred(error);
        return false;
    }
    
    try {
        // ============================================================
        // STEP 1: Generate lean SCR file (no system var setup!)
        // ============================================================
        QString scriptPath = tempDir + "/combined_batch.scr";
        QString scrContent = generateScript(config.dwgFiles, config.lispScripts);
        
        QFile scriptFile(scriptPath);
        if (!scriptFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QString error = "Failed to write SCR file: " + scriptPath;
            result.errors << error;
            logError(error);
            emit errorOccurred(error);
            cleanupTempFiles(tempDir);
            return false;
        }
        
        QTextStream stream(&scriptFile);
        stream << scrContent;
        scriptFile.close();
        
        result.scrFilePath = scriptPath;
        
        logDebug(QString("Generated SCR: %1 files x %2 scripts, %3 bytes")
                 .arg(config.dwgFiles.size())
                 .arg(config.lispScripts.size())
                 .arg(scrContent.size()));
        
        // Log scripts for debugging
        for (const QString& lsp : config.lispScripts) {
            QString funcName = QFileInfo(lsp).completeBaseName();
            logDebug(QString("  Script: %1 -> function: (%2)").arg(QFileInfo(lsp).fileName()).arg(funcName));
        }
        
        // Emit progress for UI
        for (int i = 0; i < config.dwgFiles.size(); ++i) {
            emit progressUpdate(config.dwgFiles[i], i, config.dwgFiles.size());
        }
        
        // ============================================================
        // STEP 2: Set system variables BEFORE script execution
        // ============================================================
        logInfo("Setting system variables before script...");
        setSystemVariablesForBatch();
        
        // ============================================================
        // STEP 3: Execute _.SCRIPT in CURRENT BricsCAD instance
        // ============================================================
        logInfo(QString("Executing _.SCRIPT in current instance: %1 files, %2 scripts...")
                .arg(config.dwgFiles.size()).arg(config.lispScripts.size()));
        emit logMessage(QString("Running _.SCRIPT for %1 files...").arg(config.dwgFiles.size()));
        
        bool success = executeScriptInProcess(scriptPath);
        
        // ============================================================
        // STEP 4: Finalize results
        // Note: Script runs ASYNCHRONOUSLY after acedCommand returns!
        // System vars are restored at the end of the SCR file.
        // ============================================================
        result.success = success;
        result.filesProcessed = success ? config.dwgFiles.size() : 0;
        result.scriptsExecuted = config.lispScripts.size();
        result.executionTimeMs = QDateTime::currentMSecsSinceEpoch() - startTime;
        result.exitCode = success ? 0 : -1;
        
        if (success) {
            result.summary = QString("Script started: %1 files with %2 scripts")
                .arg(config.dwgFiles.size())
                .arg(config.lispScripts.size());
            
            for (const QString& dwg : config.dwgFiles) {
                emit fileProcessed(dwg, true, "Script processing");
            }
        } else {
            result.summary = "Failed to execute _.SCRIPT command";
        }
        
        logInfo(result.summary);
        
        // Don't cleanup temp files - SCR must exist while SCRIPT runs!
        logDebug(QString("SCR file kept at: %1 (needed while SCRIPT runs)").arg(scriptPath));
        
        emit processFinished(result.exitCode);
        emit logMessage(result.summary);
        
        return result.success;
        
    } catch (const std::exception& e) {
        QString error = QString("Exception: %1").arg(e.what());
        result.errors << error;
        result.summary = "Failed: " + error;
        logError(error);
        emit errorOccurred(error);
        restoreSystemVariables();
        return false;
    } catch (...) {
        QString error = "Unknown exception";
        result.errors << error;
        result.summary = "Failed: " + error;
        logError(error);
        emit errorOccurred(error);
        restoreSystemVariables();
        return false;
    }
}

// ============================================================================
// SYSTEM VARIABLE MANAGEMENT
// ============================================================================
// Set vars BEFORE script to suppress dialogs and prompts
// Restore vars at END of SCR file (async, after all files processed)

void LispProcessExecutor::setSystemVariablesForBatch() {
    struct resbuf rb;
    
    // FILEDIA = 0 (suppress file dialogs)
    rb.restype = RTSHORT;
    rb.resval.rint = 0;
    acedSetVar(_T("FILEDIA"), &rb);
    logDebug("Set FILEDIA = 0");
    
    // CMDECHO = 0 (suppress command echo)
    rb.resval.rint = 0;
    acedSetVar(_T("CMDECHO"), &rb);
    logDebug("Set CMDECHO = 0");
    
    // EXPERT = 5 (suppress all Y/N prompts)
    rb.resval.rint = 5;
    acedSetVar(_T("EXPERT"), &rb);
    logDebug("Set EXPERT = 5");
}

void LispProcessExecutor::restoreSystemVariables() {
    struct resbuf rb;
    rb.restype = RTSHORT;
    
    rb.resval.rint = 1;
    acedSetVar(_T("FILEDIA"), &rb);
    
    rb.resval.rint = 1;
    acedSetVar(_T("CMDECHO"), &rb);
    
    rb.resval.rint = 0;
    acedSetVar(_T("EXPERT"), &rb);
    
    logDebug("Restored system variables");
}

// ============================================================================
// SCR GENERATION - Standard CAD batch pattern
// ============================================================================
// 
// LEAN SCR - no system var setup (done in C++ before script)
// NO BLANK LINES (blank lines = ENTER = interferes with commands)
// NO COMMENTS (keep it minimal)
//
// Structure per file:
//   _.OPEN "file.dwg"
//   (progn (load "script.lsp")(princ))
//   (progn (scriptFunc)(princ))
//   _QSAVE
//   _.CLOSE
//
// End of SCR: restore system variables

QString LispProcessExecutor::generateScript(const QStringList& dwgFiles, const QStringList& lispScripts) {
    QString scr;
    
    // NO system var setup here - done in C++ via acedSetVar
    // NO blank lines - they send ENTER to command line
    // NO comments - keep it lean
    
    // === Process each DWG file ===
    for (int i = 0; i < dwgFiles.size(); ++i) {
        QString dwgPath = dwgFiles[i];
        dwgPath.replace("\\", "/");
        
        // Open file
        scr += QString("_.OPEN \"%1\"\n").arg(dwgPath);
        
        // Load and call each LISP script
        for (const QString& lspPath : lispScripts) {
            QString lspForward = lspPath;
            lspForward.replace("\\", "/");
            QString funcName = QFileInfo(lspPath).completeBaseName();
            
            scr += QString("(progn (load \"%1\")(princ))\n").arg(lspForward);
            scr += QString("(progn (%1)(princ))\n").arg(funcName);
        }
        
        // Save and close
        scr += "_QSAVE\n";
        scr += "_.CLOSE\n";
    }
    
    // Restore system variables at end of script
    // (async - runs after all files are processed)
    scr += "(progn (setvar \"FILEDIA\" 1)(princ))\n";
    scr += "(progn (setvar \"CMDECHO\" 1)(princ))\n";
    scr += "(progn (setvar \"EXPERT\" 0)(princ))\n";
    
    return scr;
}

// ============================================================================
// IN-PROCESS SCRIPT EXECUTION via acedCommand
// ============================================================================

bool LispProcessExecutor::executeScriptInProcess(const QString& scriptPath) {
    // Convert to forward slashes (LISP/SCR convention)
    QString path = scriptPath;
    path.replace("\\", "/");
    
    logInfo(QString("Calling _.SCRIPT: %1").arg(path));
    
    // Execute _.SCRIPT in the current BricsCAD instance
    // This is the same as (command "_.SCRIPT" "path") in LISP
    // After this call, BricsCAD takes over and processes the SCR line by line
#ifdef _UNICODE
    int rc = acedCommand(
        RTSTR, _T("_.SCRIPT"),
        RTSTR, path.toStdWString().c_str(),
        RTNONE
    );
#else
    int rc = acedCommand(
        RTSTR, _T("_.SCRIPT"),
        RTSTR, path.toLocal8Bit().constData(),
        RTNONE
    );
#endif
    
    if (rc != RTNORM) {
        logError(QString("acedCommand(_.SCRIPT) failed with code: %1").arg(rc));
        restoreSystemVariables();
        return false;
    }
    
    logInfo("_.SCRIPT command accepted - BricsCAD is processing...");
    return true;
}

// ============================================================================
// VALIDATION
// ============================================================================

bool LispProcessExecutor::validateNoFilesOpen(const QStringList& dwgFiles, LispExecutionResult& result) {
    // Get current drawing path from BricsCAD
    resbuf rbPrefix, rbName;
    
    QString currentDwg;
    if (acedGetVar(_T("DWGPREFIX"), &rbPrefix) == RTNORM &&
        acedGetVar(_T("DWGNAME"), &rbName) == RTNORM) {
#ifdef _UNICODE
        currentDwg = QString::fromWCharArray(rbPrefix.resval.rstring) +
                     QString::fromWCharArray(rbName.resval.rstring);
#else
        currentDwg = QString::fromLocal8Bit(rbPrefix.resval.rstring) +
                     QString::fromLocal8Bit(rbName.resval.rstring);
#endif
        currentDwg.replace("\\", "/");
        logDebug(QString("Current drawing: %1").arg(currentDwg));
    }
    
    // Check if any batch file matches the current drawing
    if (!currentDwg.isEmpty()) {
        for (const QString& dwg : dwgFiles) {
            QString normalized = dwg;
            normalized.replace("\\", "/");
            if (normalized.compare(currentDwg, Qt::CaseInsensitive) == 0) {
                QString error = QString("Die aktuelle Zeichnung '%1' ist in der Batch-Liste. "
                                       "Bitte schliessen Sie die Datei zuerst oder entfernen Sie sie aus der Liste.")
                                .arg(QFileInfo(dwg).fileName());
                result.errors << error;
                result.summary = "Failed: " + error;
                logError(error);
                emit errorOccurred(error);
                return false;
            }
        }
    }
    
    return true;
}

// ============================================================================
// UTILITY
// ============================================================================

QString LispProcessExecutor::createTempDirectory() {
    QString tempPath = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    QString dirName = QString("BatchProcessing_%1").arg(QDateTime::currentMSecsSinceEpoch());
    QString fullPath = tempPath + "/" + dirName;
    
    QDir dir;
    if (!dir.mkpath(fullPath)) {
        logError("Failed to create temp dir: " + fullPath);
        return QString();
    }
    return fullPath;
}

void LispProcessExecutor::cleanupTempFiles(const QString& tempDir) {
    if (tempDir.isEmpty()) return;
    QDir dir(tempDir);
    if (dir.exists()) {
        dir.removeRecursively();
    }
}

LispExecutionConfig LispProcessExecutor::createDefaultConfig() {
    LispExecutionConfig config;
    return config;
}

void LispProcessExecutor::logDebug(const QString& message) const {
    if (m_debugMode) {
        qDebug() << "[LispExecutor]" << message;
        emit const_cast<LispProcessExecutor*>(this)->logMessage("[DEBUG] " + message);
    }
}

void LispProcessExecutor::logError(const QString& error) const {
    qWarning() << "[LispExecutor] ERROR:" << error;
    emit const_cast<LispProcessExecutor*>(this)->errorOccurred(error);
}

void LispProcessExecutor::logInfo(const QString& info) const {
    qInfo() << "[LispExecutor]" << info;
    emit const_cast<LispProcessExecutor*>(this)->logMessage(info);
}

} // namespace BatchProcessing
