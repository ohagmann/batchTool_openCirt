/**
 * @file LispProcessExecutor.h
 * @brief LISP Script Execution via BricsCAD _.SCRIPT command (in-process)
 * @version 5.0.0 - In-process architecture
 * 
 * Standard CAD batch pattern: SCR-based sequential DWG processing.
 * 1. Generate ONE .scr file:
 *    For each DWG: _.OPEN → (load "script.lsp") → (scriptName) → _QSAVE → _.CLOSE
 * 2. Execute _.SCRIPT command in the CURRENT BricsCAD instance
 * 3. BricsCAD processes all files sequentially, user watches live
 * 
 * NO external BricsCAD launch! Runs in the already-open instance.
 * 
 * Convention: LISP function name = filename without extension
 * Example: "sk1.lsp" → function call: (sk1)
 */

#ifndef LISPPROCESSEXECUTOR_H
#define LISPPROCESSEXECUTOR_H

#include <QString>
#include <QStringList>
#include <QObject>
#include <memory>

#include "../data/ProcessingOptions.h"

namespace BatchProcessing {

/**
 * @brief Configuration for LISP script execution (v5.1 in-process via _.SCRIPT)
 */
struct LispExecutionConfig {
    QStringList lispScripts;        ///< Full paths to .lsp files to execute
    QStringList dwgFiles;           ///< Full paths to DWG files to process
    QString workingDirectory;       ///< Working directory (for SCR file)
};

/**
 * @brief Result of LISP script execution
 */
struct LispExecutionResult {
    bool success = false;
    QString summary;
    QString output;
    int exitCode = -1;
    int filesProcessed = 0;
    int scriptsExecuted = 0;
    qint64 executionTimeMs = 0;
    QStringList errors;
    QString scrFilePath;            ///< Path to generated SCR (for debug)
};

/**
 * @brief In-process LISP executor via BricsCAD _.SCRIPT command
 * 
 * Generates ONE SCR file, then executes it in the current BricsCAD
 * instance via _.SCRIPT command (standard CAD batch pattern).
 * No external process launch needed.
 */
class LispProcessExecutor : public QObject {
    Q_OBJECT

public:
    explicit LispProcessExecutor(QObject* parent = nullptr);
    ~LispProcessExecutor() = default;

    /// Main execution: generates SCR, runs _.SCRIPT in current instance
    bool executeLispBatch(const LispExecutionConfig& config, LispExecutionResult& result);
    
    void setDebugMode(bool enabled) { m_debugMode = enabled; }
    bool debugMode() const { return m_debugMode; }
    
    static LispExecutionConfig createDefaultConfig();
    
    /// Create config from global ProcessingOptions (v5.1 simplified)
    static LispExecutionConfig createConfigFromOptions(const ::ProcessingOptions& options, const QList<QString>& dwgFiles) {
        LispExecutionConfig config = createDefaultConfig();
        config.lispScripts = options.lispScriptManager.getEnabledScriptPaths();
        config.dwgFiles = dwgFiles;
        return config;
    }

signals:
    void processStarted();
    void processFinished(int exitCode);
    void progressUpdate(const QString& currentFile, int fileIndex, int totalFiles);
    void fileProcessed(const QString& filePath, bool success, const QString& message);
    void logMessage(const QString& message);
    void errorOccurred(const QString& error);

private:
    /// Generate SCR file using standard CAD batch pattern
    /// Contains: setup → [_.OPEN → (load) → (func) → _QSAVE → _.CLOSE] × N → cleanup
    /// NO _QUIT - BricsCAD stays open!
    QString generateScript(const QStringList& dwgFiles, const QStringList& lispScripts);
    
    /// Execute _.SCRIPT command in the current BricsCAD instance via acedCommand
    bool executeScriptInProcess(const QString& scriptPath);
    
    /// Check that no DWG from the list is currently open
    bool validateNoFilesOpen(const QStringList& dwgFiles, LispExecutionResult& result);
    
    /// Set FILEDIA/CMDECHO/EXPERT via acedSetVar before script
    void setSystemVariablesForBatch();
    
    /// Restore system variables after script (or on error)
    void restoreSystemVariables();
    
    QString createTempDirectory();
    void cleanupTempFiles(const QString& tempDir);
    
    void logDebug(const QString& message) const;
    void logError(const QString& error) const;
    void logInfo(const QString& info) const;
    
    bool m_debugMode = false;
};

} // namespace BatchProcessing

#endif // LISPPROCESSEXECUTOR_H
