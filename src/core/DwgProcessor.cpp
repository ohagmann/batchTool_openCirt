#include "windows_fix.h"  // CRITICAL: Qt 6.8+ fix - MUST be FIRST
/**
 * @file DwgProcessor.cpp
 * @brief Core DWG Processing Engine Implementation
 * @version 2.0.0 - Real BRX API Implementation
 */

#ifdef __linux__
#include "brx_platform_linux.h"
#else
#include "brx_platform_windows.h"
#endif

// Include global ProcessingOptions for LISP processing
#include "../data/ProcessingOptions.h"

// BRX/ARX Core includes
#include "aced.h"
#include "rxregsvc.h"
#include "dbents.h"
#include "dbsymtb.h"
#include "dbtrans.h"
#include "dbcolor.h"

// For LISP execution - BRX LISP support functions
// acedEvaluateLisp is undocumented and may not be exported from brx26.lib,
// so we load it at runtime via GetProcAddress for maximum compatibility.
extern "C" {
    // LISP function invocation (documented, in brx26.lib)
    int acedInvoke(const struct resbuf* args, struct resbuf** result);
    
    // Utility functions for resbuf management (documented, in brx26.lib)
    struct resbuf* acutBuildList(int rtype, ...);
    int acutRelRb(struct resbuf* rb);
}

// Runtime-loaded function pointer for acedEvaluateLisp
typedef int (*pfnAcedEvaluateLisp)(const ACHAR* lispExpr, struct resbuf** result);
static pfnAcedEvaluateLisp s_pAcedEvaluateLisp = nullptr;
static bool s_bEvaluateLispResolved = false;

static pfnAcedEvaluateLisp getAcedEvaluateLisp() {
    if (!s_bEvaluateLispResolved) {
        s_bEvaluateLispResolved = true;
        // Try to find acedEvaluateLisp in brx26.dll at runtime
        HMODULE hBrx = GetModuleHandleW(L"brx26.dll");
        if (!hBrx) hBrx = GetModuleHandleW(L"brx25.dll");
        if (!hBrx) hBrx = GetModuleHandleW(L"brx24.dll");
        if (hBrx) {
            s_pAcedEvaluateLisp = (pfnAcedEvaluateLisp)GetProcAddress(hBrx, "acedEvaluateLisp");
        }
    }
    return s_pAcedEvaluateLisp;
}

// Result codes if not defined
#ifndef RTNORM
#define RTNORM   5100
#endif
#ifndef RTERROR
#define RTERROR  -5001
#endif
#ifndef RTSTR
#define RTSTR    5005
#endif
#ifndef RTREAL
#define RTREAL   5001
#endif
#ifndef RTSHORT
#define RTSHORT  5003
#endif
#ifndef RTLONG
#define RTLONG   5010
#endif
#ifndef RTT
#define RTT      5021
#endif
#ifndef RTNIL
#define RTNIL    5019
#endif
#ifndef RTNONE
#define RTNONE   5000
#endif

#include "DwgProcessor.h"
#include "LispProcessExecutor.h"
#include <QFileInfo>
#include <QDir>
#include <QRegularExpression>
#include <QSettings>
#include <QCryptographicHash>
#include <QDateTime>
#include <QDebug>
#include <QTimer>
#include <QVariant>
#include <QVariantList>
#include <QVariantMap>
#include <vector>
#include <cstdio>  // For printf debugging

namespace BatchProcessing {

// ============================================================================
// ProcessingOptions Implementation
// ============================================================================

ProcessingOptions::ProcessingOptions() {
    // Defaults sind bereits in der Header-Datei initialisiert
}

bool ProcessingOptions::isValid() const {
    if (sourceFolder.isEmpty()) {
        return false;
    }
    
    // GEÄNDERT: Multiple Replacements ODER Single Search Text
    if (enableTextReplacement) {
        if (searchText.isEmpty() && multipleReplacements.isEmpty()) {
            return false;  // Weder Single noch Multiple definiert
        }
    }
    
    // Attribute replacement: leeres Search-Feld = alle Attribute überschreiben
    // (gefiltert nach Block/Tag)
    
    if (maxConcurrentFiles < 1) {
        return false;
    }
    
    return true;
}

QString ProcessingOptions::getValidationError() const {
    if (sourceFolder.isEmpty()) {
        return "Source folder is not specified";
    }
    
    // GEÄNDERT: Multiple Replacements ODER Single Search Text
    if (enableTextReplacement) {
        if (searchText.isEmpty() && multipleReplacements.isEmpty()) {
            return "Text replacement enabled but no search text or replacements defined";
        }
    }
    
    // Attribute replacement: leeres Search-Feld = alle Attribute überschreiben
    
    if (maxConcurrentFiles < 1) {
        return "Invalid concurrent files setting";
    }
    
    return QString();
}

void ProcessingOptions::saveToSettings(const QString& group) const {
    QSettings settings;
    settings.beginGroup(group);
    
    // Text Processing
    settings.setValue("enableTextReplacement", enableTextReplacement);
    settings.setValue("searchText", searchText);
    settings.setValue("replaceText", replaceText);
    settings.setValue("useRegex", useRegex);
    settings.setValue("useWildcards", useWildcards);
    settings.setValue("caseSensitive", caseSensitive);
    settings.setValue("wholeWordsOnly", wholeWordsOnly);
    
    // Attribute Processing
    settings.setValue("enableAttributeReplacement", enableAttributeReplacement);
    settings.setValue("attributeSearchText", attributeSearchText);
    settings.setValue("attributeReplaceText", attributeReplaceText);
    settings.setValue("targetAttributes", targetAttributes);
    settings.setValue("targetBlocks", targetBlocks);
    
    // Layer Management
    settings.setValue("enableLayerOperations", enableLayerOperations);
    
    // Serialize layerOperations as QVariantList
    QVariantList operationsList;
    for (const LayerOperation& op : layerOperations) {
        QVariantMap opMap;
        opMap["type"] = op.type;
        opMap["layerName"] = op.layerName;
        opMap["newValue"] = op.newValue;
        opMap["isEnabled"] = op.isEnabled;
        operationsList.append(opMap);
    }
    settings.setValue("layerOperations", operationsList);
    
    // File Processing
    settings.setValue("sourceFolder", sourceFolder);
    settings.setValue("filePatterns", filePatterns);
    settings.setValue("includeSubfolders", includeSubfolders);
    settings.setValue("createBackups", createBackups);
    settings.setValue("backupSuffix", backupSuffix);
    
    // Advanced Options
    settings.setValue("skipReadOnlyFiles", skipReadOnlyFiles);
    settings.setValue("skipEmptyFiles", skipEmptyFiles);
    settings.setValue("maxConcurrentFiles", maxConcurrentFiles);
    settings.setValue("progressUpdateInterval", progressUpdateInterval);
    
    settings.endGroup();
}

void ProcessingOptions::loadFromSettings(const QString& group) {
    QSettings settings;
    settings.beginGroup(group);
    
    // Text Processing
    enableTextReplacement = settings.value("enableTextReplacement", false).toBool();
    searchText = settings.value("searchText", "").toString();
    replaceText = settings.value("replaceText", "").toString();
    useRegex = settings.value("useRegex", false).toBool();
    useWildcards = settings.value("useWildcards", false).toBool();
    caseSensitive = settings.value("caseSensitive", false).toBool();
    wholeWordsOnly = settings.value("wholeWordsOnly", false).toBool();
    
    // Attribute Processing
    enableAttributeReplacement = settings.value("enableAttributeReplacement", false).toBool();
    attributeSearchText = settings.value("attributeSearchText", "").toString();
    attributeReplaceText = settings.value("attributeReplaceText", "").toString();
    targetAttributes = settings.value("targetAttributes").toStringList();
    targetBlocks = settings.value("targetBlocks").toStringList();
    
    // Layer Management
    enableLayerOperations = settings.value("enableLayerOperations", false).toBool();
    
    // Deserialize layerOperations from QVariantList
    layerOperations.clear();
    QVariantList operationsList = settings.value("layerOperations").toList();
    for (const QVariant& var : operationsList) {
        QVariantMap opMap = var.toMap();
        LayerOperation op;
        op.type = opMap["type"].toString();
        op.layerName = opMap["layerName"].toString();
        op.newValue = opMap["newValue"].toString();
        op.isEnabled = opMap["isEnabled"].toBool();
        layerOperations.append(op);
    }
    
    // File Processing
    sourceFolder = settings.value("sourceFolder", "").toString();
    filePatterns = settings.value("filePatterns", QStringList() << "*.dwg").toStringList();
    includeSubfolders = settings.value("includeSubfolders", false).toBool();
    createBackups = settings.value("createBackups", true).toBool();
    backupSuffix = settings.value("backupSuffix", ".backup").toString();
    
    // Advanced Options
    skipReadOnlyFiles = settings.value("skipReadOnlyFiles", true).toBool();
    skipEmptyFiles = settings.value("skipEmptyFiles", true).toBool();
    maxConcurrentFiles = settings.value("maxConcurrentFiles", 4).toInt();
    progressUpdateInterval = settings.value("progressUpdateInterval", 100).toInt();
    
    settings.endGroup();
}

// ============================================================================
// ProcessingStatistics Implementation
// ============================================================================

ProcessingStatistics::ProcessingStatistics() {
    reset();
}

void ProcessingStatistics::reset() {
    totalFiles = 0;
    processedFiles = 0;
    skippedFiles = 0;
    errorFiles = 0;
    
    textReplacements = 0;
    attributeReplacements = 0;
    layerOperations = 0;
    lispScriptsExecuted = 0;
    backupsCreated = 0;
    
    startTime = 0;
    endTime = 0;
    totalProcessingTime = 0;
    
    errorMessages.clear();
    fileErrors.clear();
}

qint64 ProcessingStatistics::getElapsedTime() const {
    if (startTime == 0) return 0;
    
    if (endTime > 0) {
        return endTime - startTime;
    }
    
    return QDateTime::currentMSecsSinceEpoch() - startTime;
}

double ProcessingStatistics::getAverageTimePerFile() const {
    if (processedFiles == 0) return 0.0;
    return static_cast<double>(totalProcessingTime) / processedFiles;
}

QString ProcessingStatistics::getFormattedSummary() const {
    QString summary;
    summary += QString("Total Files: %1\n").arg(totalFiles);
    summary += QString("Processed: %1\n").arg(processedFiles);
    summary += QString("Skipped: %1\n").arg(skippedFiles);
    summary += QString("Errors: %1\n").arg(errorFiles);
    summary += QString("Text Replacements: %1\n").arg(textReplacements);
    summary += QString("Attribute Replacements: %1\n").arg(attributeReplacements);
    summary += QString("Layer Operations: %1\n").arg(layerOperations);
    summary += QString("LISP Scripts Executed: %1\n").arg(lispScriptsExecuted);
    summary += QString("Backups Created: %1\n").arg(backupsCreated);
    
    qint64 elapsed = getElapsedTime();
    if (elapsed > 0) {
        summary += QString("Processing Time: %1\n").arg(DwgUtils::formatElapsedTime(elapsed));
        summary += QString("Average Time per File: %1 ms\n").arg(getAverageTimePerFile(), 0, 'f', 2);
    }
    
    return summary;
}

QString ProcessingStatistics::getFormattedReport() const {
    QString report = "=== Batch Processing Report ===\n\n";
    report += getFormattedSummary();
    
    if (!errorMessages.isEmpty()) {
        report += "\n=== Errors ===\n";
        for (const QString& error : errorMessages) {
            report += error + "\n";
        }
    }
    
    if (!fileErrors.isEmpty()) {
        report += "\n=== File Errors ===\n";
        for (auto it = fileErrors.constBegin(); it != fileErrors.constEnd(); ++it) {
            report += QString("%1: %2\n").arg(it.key(), it.value());
        }
    }
    
    return report;
}

// ============================================================================
// BatchProcessingEngine Implementation
// ============================================================================

BatchProcessingEngine::BatchProcessingEngine(QObject* parent)
    : QObject(parent)
{
    m_progressTimer = new QTimer(this);
    connect(m_progressTimer, &QTimer::timeout, this, &BatchProcessingEngine::onProgressTimer);
}

BatchProcessingEngine::~BatchProcessingEngine() {
    if (m_isProcessing) {
        stopProcessing();
    }
}

bool BatchProcessingEngine::startProcessing(const ProcessingOptions& options) {
    if (m_isProcessing) {
        emit errorOccurred("Processing already in progress");
        return false;
    }
    
    m_options = options;
    
    // DEBUG: Log what we received from UI
    printf("[ENGINE-START] Received options from UI:\n");
    printf("  enableLispExecution: %s\n", m_options.enableLispExecution ? "TRUE" : "FALSE");
    printf("  lispScripts count: %lld\n", static_cast<long long>(m_options.lispScripts.size()));
    if (!m_options.lispScripts.isEmpty()) {
        for (const QString& script : m_options.lispScripts) {
            printf("    - %s\n", script.toStdString().c_str());
        }
    }
    fflush(stdout);
    
    // SPECIAL HANDLING: If LISP execution is enabled, convert to global ProcessingOptions and handle separately
    if (m_options.enableLispExecution && !m_options.lispScripts.isEmpty()) {
        // Create a global ProcessingOptions from our local one
        ::ProcessingOptions globalOptions;
        // Copy relevant fields (v5.1: simplified, removed UI options)
        globalOptions.enableLispExecution = m_options.enableLispExecution;
        // Add LISP scripts directly to lispScriptManager
        for (const QString& script : m_options.lispScripts) {
            globalOptions.lispScriptManager.addScript(script);
        }
        return startLispProcessing(&globalOptions);  // Pass as void*
    }
    
    if (!validateOptions()) {
        emit errorOccurred(m_options.getValidationError());
        return false;
    }
    
    initializeProcessing();
    
    // Verwende übergebene Dateiliste ODER suche selbst
    if (!m_options.filesToProcess.isEmpty()) {
        m_fileQueue = m_options.filesToProcess;
        qDebug() << "Using provided file list:" << m_fileQueue.size() << "files";
    } else {
        // Fallback: Suche selbst (wie bisher)
        m_fileQueue = findDwgFiles(m_options.sourceFolder, m_options.filePatterns, m_options.includeSubfolders);
    }
    
    if (m_fileQueue.isEmpty()) {
        emit errorOccurred("No DWG files found to process");
        return false;
    }
    
    m_statistics.totalFiles = m_fileQueue.size();
    m_statistics.startTime = QDateTime::currentMSecsSinceEpoch();
    
    emit processingStarted(m_statistics.totalFiles);
    
    // Start processing
    m_isProcessing = true;
    m_progressTimer->start(m_options.progressUpdateInterval);
    
    QTimer::singleShot(0, this, &BatchProcessingEngine::processNextFile);
    
    return true;
}

void BatchProcessingEngine::stopProcessing() {
    m_shouldAbort = true;
    m_progressTimer->stop();
    
    if (m_currentProcessor) {
        m_currentProcessor.reset();
    }
    
    finalizeProcessing(false);
}

void BatchProcessingEngine::pauseProcessing() {
    QMutexLocker locker(&m_mutex);
    m_isPaused = true;
    m_progressTimer->stop();
}

void BatchProcessingEngine::resumeProcessing() {
    QMutexLocker locker(&m_mutex);
    if (!m_isPaused) return;
    
    m_isPaused = false;
    m_progressTimer->start(m_options.progressUpdateInterval);
    
    QTimer::singleShot(0, this, &BatchProcessingEngine::processNextFile);
}

void BatchProcessingEngine::abortProcessing() {
    stopProcessing();
}

void BatchProcessingEngine::processNextFile() {
    QMutexLocker locker(&m_mutex);
    
    if (m_shouldAbort || !m_isProcessing) {
        finalizeProcessing(false);
        return;
    }
    
    if (m_isPaused) {
        return;
    }
    
    if (m_currentFileIndex >= m_fileQueue.size()) {
        finalizeProcessing(true);
        return;
    }
    
    QString currentFile = m_fileQueue[m_currentFileIndex];
    locker.unlock();
    
    processFileInternal(currentFile);
    
    m_currentFileIndex++;
    
    // Schedule next file
    if (m_currentFileIndex < m_fileQueue.size()) {
        QTimer::singleShot(10, this, &BatchProcessingEngine::processNextFile);
    } else {
        finalizeProcessing(true);
    }
}

void BatchProcessingEngine::processFileInternal(const QString& filePath) {
    qint64 fileStartTime = QDateTime::currentMSecsSinceEpoch();
    
    emit progressUpdated(m_currentFileIndex + 1, m_statistics.totalFiles, filePath);
    
    // Check if file should be skipped
    if (m_options.skipReadOnlyFiles && DwgUtils::isFileReadOnly(filePath)) {
        m_statistics.skippedFiles++;
        emit fileProcessed(filePath, false, "File is read-only");
        return;
    }
    
    if (m_options.skipEmptyFiles && DwgUtils::getFileSize(filePath) == 0) {
        m_statistics.skippedFiles++;
        emit fileProcessed(filePath, false, "File is empty");
        return;
    }
    
    // Process the file
    m_currentProcessor = std::make_unique<DwgFileProcessor>(filePath);
    
    // DEBUG: Log options before processing
    qDebug() << "[ENGINE] Processing file:" << filePath;
    qDebug() << "[ENGINE] enableLispExecution:" << m_options.enableLispExecution;
    qDebug() << "[ENGINE] lispScripts count:" << m_options.lispScripts.size();
    if (!m_options.lispScripts.isEmpty()) {
        qDebug() << "[ENGINE] LISP scripts:";
        for (const QString& script : m_options.lispScripts) {
            qDebug() << "  -" << script;
        }
    }
    
    bool success = m_currentProcessor->processFile(m_options, m_statistics);
    
    qint64 fileEndTime = QDateTime::currentMSecsSinceEpoch();
    m_statistics.totalProcessingTime += (fileEndTime - fileStartTime);
    
    if (success) {
        m_statistics.processedFiles++;
        emit fileProcessed(filePath, true, "File processed successfully");
    } else {
        m_statistics.errorFiles++;
        QString error = m_currentProcessor->getLastError();
        m_statistics.fileErrors[filePath] = error;
        emit fileProcessed(filePath, false, error);
    }
    
    m_currentProcessor.reset();
}

void BatchProcessingEngine::onProgressTimer() {
    updateProgress();
}

void BatchProcessingEngine::initializeProcessing() {
    m_statistics.reset();
    m_fileQueue.clear();
    m_currentFileIndex = 0;
    m_shouldAbort = false;
    m_isPaused = false;
}

void BatchProcessingEngine::finalizeProcessing(bool success) {
    m_progressTimer->stop();
    m_isProcessing = false;
    m_statistics.endTime = QDateTime::currentMSecsSinceEpoch();
    
    QString summary = m_statistics.getFormattedSummary();
    emit processingFinished(success, summary);
}

bool BatchProcessingEngine::validateOptions() const {
    return m_options.isValid();
}

void BatchProcessingEngine::updateProgress() {
    emitProgress();
}

void BatchProcessingEngine::emitProgress() {
    QString currentFile = (m_currentFileIndex < m_fileQueue.size()) 
                         ? m_fileQueue[m_currentFileIndex] 
                         : QString();
    emit progressUpdated(m_currentFileIndex + 1, m_statistics.totalFiles, currentFile);
}

QStringList BatchProcessingEngine::findDwgFiles(const QString& folder, 
                                                const QStringList& patterns, 
                                                bool includeSubfolders) const {
    QStringList files;
    QDir dir(folder);
    
    if (!dir.exists()) {
        return files;
    }
    
    // Get files in current directory
    for (const QString& pattern : patterns) {
        QStringList matchingFiles = dir.entryList(QStringList() << pattern, QDir::Files);
        for (const QString& file : matchingFiles) {
            files.append(dir.absoluteFilePath(file));
        }
    }
    
    // Process subdirectories if requested
    if (includeSubfolders) {
        QStringList subdirs = dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
        for (const QString& subdir : subdirs) {
            files.append(findDwgFiles(dir.absoluteFilePath(subdir), patterns, true));
        }
    }
    
    return files;
}

// ============================================================================
// NEW: LISP Batch Processing via Subprocess
// ============================================================================

bool BatchProcessingEngine::startLispProcessing(const void* globalOptionsPtr) {
    // Cast void* back to ::ProcessingOptions
    const ::ProcessingOptions& options = *static_cast<const ::ProcessingOptions*>(globalOptionsPtr);
    
    qDebug() << "[ENGINE] Starting LISP batch processing via script system";
    printf("[ENGINE-C] Starting LISP batch processing via script system\n");
    fflush(stdout);
    
    // Prepare file list - use m_options.filesToProcess from local ProcessingOptions
    QStringList filesToProcess;
    if (!m_options.filesToProcess.isEmpty()) {
        filesToProcess = m_options.filesToProcess;
    } else {
        filesToProcess = findDwgFiles(m_options.sourceFolder, m_options.filePatterns, m_options.includeSubfolders);
    }
    
    if (filesToProcess.isEmpty()) {
        emit errorOccurred("No DWG files found for LISP processing");
        return false;
    }
    
    // Initialize processing state
    initializeProcessing();
    m_statistics.totalFiles = filesToProcess.size();
    m_statistics.startTime = QDateTime::currentMSecsSinceEpoch();
    
    emit processingStarted(m_statistics.totalFiles);
    
    // Update UI state
    m_isProcessing = true;
    m_progressTimer->start(m_options.progressUpdateInterval);  // Use local options
    
    // Create LISP process executor configuration
    // Using global ::ProcessingOptions from ProcessingOptions.h
    LispExecutionConfig config = LispProcessExecutor::createConfigFromOptions(options, filesToProcess);
    
    // In-process architecture: no need to find BricsCAD executable
    // We ARE running inside BricsCAD as a BRX plugin
    qDebug() << "[ENGINE] In-process LISP execution via _.SCRIPT";
    qDebug() << "[ENGINE] Processing" << filesToProcess.size() << "files with" << config.lispScripts.size() << "scripts";
    
    // Create executor and connect signals
    m_lispExecutor = std::make_unique<LispProcessExecutor>(this);
    
    // Connect executor signals to engine signals
    connect(m_lispExecutor.get(), &LispProcessExecutor::processStarted,
            this, [this]() {
        emit progressUpdated(0, m_statistics.totalFiles, "Starting _.SCRIPT...");
    });
    
    connect(m_lispExecutor.get(), &LispProcessExecutor::progressUpdate,
            this, [this](const QString& currentFile, int fileIndex, int totalFiles) {
        emit progressUpdated(fileIndex + 1, totalFiles, currentFile);
        m_statistics.processedFiles = fileIndex;
    });
    
    connect(m_lispExecutor.get(), &LispProcessExecutor::processFinished,
            this, &BatchProcessingEngine::onLispProcessingFinished);
    
    // Note: processError signal removed in simplified system
    // Error handling is done internally and reported via processFinished
    
    // Execute LISP batch processing
    LispExecutionResult result;
    QTimer::singleShot(100, this, [this, config]() mutable {
        this->executeLispBatchAsync(config);
    });
    
    return true;
}

void BatchProcessingEngine::executeLispBatchAsync(const LispExecutionConfig& config) {
    if (!m_lispExecutor) {
        emit errorOccurred("LISP executor not initialized");
        finalizeProcessing(false);
        return;
    }
    
    LispExecutionResult result;
    bool success = m_lispExecutor->executeLispBatch(config, result);
    
    // Update statistics
    m_statistics.lispScriptsExecuted = result.scriptsExecuted;
    m_statistics.processedFiles = result.filesProcessed;
    m_statistics.totalProcessingTime = result.executionTimeMs;
    
    if (!result.errors.isEmpty()) {
        for (const QString& error : result.errors) {
            m_statistics.errorMessages.append(error);
        }
    }
    
    // Finalize processing
    finalizeProcessing(success);
}

void BatchProcessingEngine::onLispProcessingFinished(int exitCode) {
    // ExitStatus parameter removed in simplified version
    
    qDebug() << "[ENGINE] LISP processing finished with exit code:" << exitCode;
    
    // The actual finalization is handled by executeLispBatchAsync
    // This slot is mainly for logging and cleanup
}

// ============================================================================
// NEW: Layer Discovery Functions (replaces Mock functionality)
// ============================================================================

QStringList BatchProcessingEngine::scanLayersInFiles(const QStringList& filePaths) {
    QStringList allLayers;
    m_layerCache.clear();
    m_layerInfoCache.clear();
    
    for (const QString& filePath : filePaths) {
        scanFileForLayers(filePath);
        
        if (m_layerCache.contains(filePath)) {
            QStringList fileLayers = m_layerCache[filePath];
            for (const QString& layer : fileLayers) {
                if (!allLayers.contains(layer)) {
                    allLayers.append(layer);
                }
            }
        }
    }
    
    allLayers.sort();
    emit layersScanned(allLayers);
    return allLayers;
}

QMap<QString, QStringList> BatchProcessingEngine::getLayersByFile(const QStringList& filePaths) {
    QMap<QString, QStringList> result;
    
    for (const QString& filePath : filePaths) {
        if (!m_layerCache.contains(filePath)) {
            scanFileForLayers(filePath);
        }
        
        if (m_layerCache.contains(filePath)) {
            result[filePath] = m_layerCache[filePath];
        }
    }
    
    return result;
}

QMap<QString, LayerInfo> BatchProcessingEngine::getLayerStatistics(const QStringList& filePaths) {
    QMap<QString, LayerInfo> result = m_layerInfoCache;
    
    // Ensure all files are scanned
    for (const QString& filePath : filePaths) {
        if (!m_layerCache.contains(filePath)) {
            scanFileForLayers(filePath);
        }
    }
    
    return result;
}

void BatchProcessingEngine::scanFileForLayers(const QString& filePath) {
    QStringList layers = DwgProcessorFactory::scanLayersInFile(filePath);
    updateLayerCache(filePath, layers);
}

void BatchProcessingEngine::updateLayerCache(const QString& filePath, const QStringList& layers) {
    m_layerCache[filePath] = layers;
    
    // Update layer info cache
    for (const QString& layerName : layers) {
        if (!m_layerInfoCache.contains(layerName)) {
            LayerInfo info = DwgProcessorFactory::getLayerInfo(filePath, layerName);
            info.name = layerName;
            m_layerInfoCache[layerName] = info;
        }
    }
}

// ============================================================================
// DwgFileProcessor Implementation - Enhanced with Real BRX
// ============================================================================

DwgFileProcessor::DwgFileProcessor(const QString& filePath)
    : m_filePath(filePath)
{
    m_isValid = QFileInfo::exists(filePath);
    if (!m_isValid) {
        m_lastError = "File does not exist";
    }
}

DwgFileProcessor::~DwgFileProcessor() {
    closeDatabase();
}

bool DwgFileProcessor::processFile(const ProcessingOptions& options, ProcessingStatistics& stats) {
    if (!m_isValid) {
        return false;
    }

    // Speichere die Options für createBackup()
    m_currentOptions = options;
    
    // Create backup if requested
    if (options.createBackups) {
        if (!createBackup(options.backupSuffix)) {
            m_lastError = "Failed to create backup";
            return false;
        }
        stats.backupsCreated++;
    }
    
    // Open database
    if (!openDatabase()) {
        return false;
    }
    
    bool success = true;
    
    // Perform operations
    if (options.enableTextReplacement) {
        success = performTextReplacement(options, stats) && success;
    }
    
    if (options.enableAttributeReplacement) {
        success = performAttributeReplacement(options, stats) && success;
    }
    
    if (options.enableLayerOperations) {
        success = performLayerOperations(options, stats) && success;
    }
    
    // Perform LISP execution if enabled
    // Force output to see if we reach this point
    printf("[DWG-C] Before LISP check: enableLispExecution = %s, scripts = %lld\n", 
           options.enableLispExecution ? "true" : "false",
           static_cast<long long>(options.lispScripts.size()));
    fflush(stdout);
    
    qDebug() << "[DWG-PROCESS] Checking LISP execution: enabled =" << options.enableLispExecution;
    if (options.enableLispExecution) {
        printf("[DWG-C] Calling performLispExecution...\n");
        fflush(stdout);
        qDebug() << "[DWG-PROCESS] Calling performLispExecution...";
        success = performLispExecution(options, stats) && success;
        qDebug() << "[DWG-PROCESS] performLispExecution returned:" << success;
    } else {
        printf("[DWG-C] LISP execution is DISABLED!\n");
        fflush(stdout);
        qDebug() << "[DWG-PROCESS] LISP execution is DISABLED";
    }
    
    // Save and close
    if (success) {
        success = saveDatabase();
    }
    
    closeDatabase();
    
    return success;
}

bool DwgFileProcessor::openDatabase() {
    try {
        // Cleanup any existing database
        closeDatabase();
        
        // Create new database instance
        m_database = new AcDbDatabase(false, true);
        if (!m_database) {
            m_lastError = "Failed to create database instance";
            return false;
        }
        
        // Open the DWG file
        Acad::ErrorStatus es = m_database->readDwgFile(m_filePath.toStdWString().c_str());
        if (es != Acad::eOk) {
            delete m_database;
            m_database = nullptr;
            m_lastError = QString("Failed to open DWG file: error %1").arg(int(es));
            return false;
        }
        
        return true;
    } catch (...) {
        m_lastError = "Exception occurred while opening database";
        closeDatabase();
        return false;
    }
}

bool DwgFileProcessor::saveDatabase() {
    if (!m_database) {
        m_lastError = "No database to save";
        return false;
    }
    
    try {
        Acad::ErrorStatus es = m_database->saveAs(m_filePath.toStdWString().c_str());
        if (es != Acad::eOk) {
            m_lastError = QString("Failed to save DWG file: error %1").arg(int(es));
            return false;
        }
        return true;
    } catch (...) {
        m_lastError = "Exception occurred while saving database";
        return false;
    }
}

void DwgFileProcessor::closeDatabase() {
    // FIXED: BRX Stack-based Transaction Management (no parameters!)
    if (m_transaction && m_database) {
        try {
            AcDbTransactionManager* pTransMan = m_database->transactionManager();
            if (pTransMan) {
                pTransMan->abortTransaction();  // No parameters - works on current transaction
            }
        } catch (...) {
            // Ignore errors during cleanup
        }
        m_transaction = nullptr;
    }
    
    if (m_database) {
        delete m_database;
        m_database = nullptr;
    }
}

bool DwgFileProcessor::createBackup(const QString& backupSuffix) {
    QFileInfo fileInfo(m_filePath);
    
    // Bestimme Backup-Verzeichnis basierend auf den Settings
    QString backupDir;
    if (m_currentOptions.useCustomBackupLocation && !m_currentOptions.customBackupFolder.isEmpty()) {
        backupDir = m_currentOptions.customBackupFolder;
        
        // Stelle sicher, dass das Verzeichnis existiert
        QDir dir(backupDir);
        if (!dir.exists()) {
            dir.mkpath(".");
        }
    } else {
        // Same folder as DWG
        backupDir = fileInfo.absolutePath();
    }
    
    // Erstelle Backup-Dateiname
    QString backupFileName;
    if (m_currentOptions.useTimestampInBackup) {
        QString timestamp = QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss");
        backupFileName = QString("%1_%2.bak").arg(fileInfo.baseName()).arg(timestamp);
    } else {
        backupFileName = QString("%1.bak").arg(fileInfo.baseName());
    }
    
    QString backupPath = QDir(backupDir).absoluteFilePath(backupFileName);
    
    // Lösche alte Backups wenn gewünscht
    if (m_currentOptions.deleteOldBackups && !m_currentOptions.useTimestampInBackup) {
        QDir dir(backupDir);
        QString pattern = QString("%1*.bak").arg(fileInfo.baseName());
        QStringList oldBackups = dir.entryList(QStringList() << pattern, QDir::Files);
        for (const QString& oldBackup : oldBackups) {
            QFile::remove(dir.absoluteFilePath(oldBackup));
        }
    }
    
    // Erstelle neues Backup
    if (QFile::copy(m_filePath, backupPath)) {
        QFile::setPermissions(backupPath, 
            QFile::permissions(backupPath) & ~QFile::WriteOwner & ~QFile::WriteUser);
        return true;
    }
    
    return false;
}

// ============================================================================
// NEW: Real Layer Processing Functions
// ============================================================================

QStringList DwgFileProcessor::scanLayersInDatabase() {
    if (!m_database) {
        return QStringList();
    }
    
    return DwgUtils::extractLayersFromDatabase(m_database);
}

QMap<QString, LayerInfo> DwgFileProcessor::getLayerDetails() {
    if (!m_database) {
        return QMap<QString, LayerInfo>();
    }
    
    QMap<QString, LayerInfo> result;
    QStringList layers = scanLayersInDatabase();
    
    for (const QString& layerName : layers) {
        AcDbLayerTable* pLayerTable;
        if (m_database->getLayerTable(pLayerTable, AcDb::kForRead) == Acad::eOk) {
            AcDbLayerTableRecord* pLayerRecord;
            if (pLayerTable->getAt(layerName.toStdWString().c_str(), pLayerRecord, AcDb::kForRead) == Acad::eOk) {
                LayerInfo info = DwgUtils::getLayerInfoFromRecord(pLayerRecord);
                info.name = layerName;
                result[layerName] = info;
                pLayerRecord->close();
            }
            pLayerTable->close();
        }
    }
    
    return result;
}

bool DwgFileProcessor::performLayerOperations(const ProcessingOptions& options, ProcessingStatistics& stats) {
    if (!m_database) {
        qDebug() << "performLayerOperations: No database loaded";
        return false;
    }
    
    if (options.layerOperations.isEmpty()) {
        qDebug() << "performLayerOperations: No layer operations to perform";
        return true;
    }
    
    qDebug() << "Starting layer operations, count:" << options.layerOperations.size();
    
    int operationCount = 0;
    bool success = true;
    
    for (const ProcessingOptions::LayerOperation& operation : options.layerOperations) {
        if (!operation.isEnabled) continue;
        
        QString operationType = operation.type;
        QString layerName = operation.layerName;
        QString newValue = operation.newValue;
        
        if (operationType == "Create New Layer") {
            int colorIndex = 7; // Default white
            if (!newValue.isEmpty()) {
                // Extract color index from color string
                QRegularExpression colorRegex("\\((\\d+)\\)");
                QRegularExpressionMatch match = colorRegex.match(newValue);
                if (match.hasMatch()) {
                    colorIndex = match.captured(1).toInt();
                }
            }
            
            if (DwgUtils::createLayerInDatabase(m_database, layerName, colorIndex)) {
                operationCount++;
            } else {
                success = false;
                m_lastError = QString("Failed to create layer: %1").arg(layerName);
            }
            
        } else if (operationType == "Delete Layer") {
            qDebug() << "Processing Delete Layer operation for:" << layerName;
            if (DwgUtils::deleteLayerFromDatabase(m_database, layerName)) {
                operationCount++;
                qDebug() << "Successfully deleted layer:" << layerName;
            } else {
                // Don't mark as failed if layer doesn't exist - it might have been deleted already
                m_lastError = QString("Could not delete layer: %1 (may not exist or be in use)").arg(layerName);
                qDebug() << m_lastError;
            }
            
        } else if (operationType == "Rename Layer") {
            if (!newValue.isEmpty()) {
                qDebug() << "Processing Rename Layer operation:" << layerName << "->" << newValue;
                if (DwgUtils::renameLayerInDatabase(m_database, layerName, newValue)) {
                    operationCount++;
                    qDebug() << "Successfully renamed layer:" << layerName << "to" << newValue;
                } else {
                    // Check if the old layer doesn't exist (might have been renamed already)
                    QStringList existingLayers = DwgUtils::extractLayersFromDatabase(m_database);
                    if (!existingLayers.contains(layerName) && existingLayers.contains(newValue)) {
                        // Layer was probably already renamed
                        qDebug() << "Layer" << layerName << "not found, but" << newValue << "exists (probably already renamed)";
                    } else {
                        m_lastError = QString("Could not rename layer: %1 to %2").arg(layerName, newValue);
                        qDebug() << m_lastError;
                    }
                }
            }
            
        } else if (operationType == "Change Color") {
            if (!newValue.isEmpty()) {
                int colorIndex = 7;
                QRegularExpression colorRegex("\\((\\d+)\\)");
                QRegularExpressionMatch match = colorRegex.match(newValue);
                if (match.hasMatch()) {
                    colorIndex = match.captured(1).toInt();
                }
                
                if (setLayerColor(layerName, colorIndex)) {
                    operationCount++;
                } else {
                    success = false;
                    m_lastError = QString("Failed to set color for layer: %1").arg(layerName);
                }
            }
            
        } else if (operationType == "Freeze/Thaw") {
            bool freeze = (newValue.toLower() == "freeze");
            qDebug() << "Processing Freeze/Thaw operation for layer:" << layerName << "Action:" << newValue;
            if (setLayerFrozen(layerName, freeze)) {
                operationCount++;
                qDebug() << QString("Layer %1 %2 successfully").arg(layerName).arg(freeze ? "frozen" : "thawed");
            } else {
                success = false;
                m_lastError = QString("Failed to %1 layer: %2").arg(freeze ? "freeze" : "thaw", layerName);
                qDebug() << m_lastError;
            }
            
        } else if (operationType == "Set Visibility") {
            bool visible = !newValue.contains("Off", Qt::CaseInsensitive);
            if (setLayerVisibility(layerName, visible)) {
                operationCount++;
                qDebug() << QString("Layer %1 visibility set to %2").arg(layerName).arg(visible ? "on" : "off");
            } else {
                success = false;
                m_lastError = QString("Failed to set visibility for layer: %1").arg(layerName);
            }
            
        } else if (operationType == "Change Linetype") {
            if (!newValue.isEmpty()) {
                if (setLayerLinetype(layerName, newValue)) {
                    operationCount++;
                } else {
                    success = false;
                    m_lastError = QString("Failed to set linetype for layer: %1").arg(layerName);
                }
            }
            
        } else if (operationType == "Set Transparency") {
            if (!newValue.isEmpty()) {
                int transparency = newValue.toInt();
                if (setLayerTransparency(layerName, transparency)) {
                    operationCount++;
                } else {
                    success = false;
                    m_lastError = QString("Failed to set transparency for layer: %1").arg(layerName);
                }
            }
        }
        // Additional layer operations can be added here...
    }
    
    stats.layerOperations += operationCount;
    qDebug() << "Completed layer operations. Performed:" << operationCount << "Success:" << success;
    return success;
}

bool DwgFileProcessor::createLayer(const QString& layerName, int colorIndex) {
    if (!m_database) return false;
    return DwgUtils::createLayerInDatabase(m_database, layerName, colorIndex);
}

bool DwgFileProcessor::deleteLayer(const QString& layerName) {
    if (!m_database) return false;
    return DwgUtils::deleteLayerFromDatabase(m_database, layerName);
}

bool DwgFileProcessor::renameLayer(const QString& oldName, const QString& newName) {
    if (!m_database) return false;
    return DwgUtils::renameLayerInDatabase(m_database, oldName, newName);
}

bool DwgFileProcessor::setLayerColor(const QString& layerName, int colorIndex) {
    if (!m_database) return false;
    
    LayerInfo info;
    info.colorIndex = colorIndex;
    return DwgUtils::setLayerPropertiesInDatabase(m_database, layerName, info);
}

bool DwgFileProcessor::setLayerFrozen(const QString& layerName, bool freeze) {
    if (!m_database) {
        qDebug() << "setLayerFrozen: No database";
        return false;
    }
    
    qDebug() << "Attempting to" << (freeze ? "freeze" : "thaw") << "layer:" << layerName;
    
    try {
        AcDbLayerTable* pLayerTable;
        Acad::ErrorStatus es = m_database->getLayerTable(pLayerTable, AcDb::kForRead);
        if (es != Acad::eOk) {
            qDebug() << "Failed to get layer table, error:" << int(es);
            return false;
        }
        
        // Check if layer exists
        if (!pLayerTable->has(layerName.toStdWString().c_str())) {
            qDebug() << "Layer does not exist:" << layerName;
            pLayerTable->close();
            return false;
        }
        
        AcDbLayerTableRecord* pLayerRecord;
        es = pLayerTable->getAt(layerName.toStdWString().c_str(), pLayerRecord, AcDb::kForWrite);
        if (es == Acad::eOk) {
            // Check current state
            bool currentlyFrozen = pLayerRecord->isFrozen();
            qDebug() << "Layer" << layerName << "is currently" << (currentlyFrozen ? "frozen" : "thawed");
            
            // Set new state
            es = pLayerRecord->setIsFrozen(freeze);
            if (es == Acad::eOk) {
                qDebug() << "Successfully" << (freeze ? "froze" : "thawed") << "layer:" << layerName;
            } else {
                qDebug() << "Failed to" << (freeze ? "freeze" : "thaw") << "layer:" << layerName << "Error:" << int(es);
            }
            
            pLayerRecord->close();
            pLayerTable->close();
            return (es == Acad::eOk);
        } else {
            qDebug() << "Failed to get layer record for:" << layerName << "Error:" << int(es);
        }
        
        pLayerTable->close();
        return false;
    } catch (const std::exception& e) {
        qDebug() << "Exception in setLayerFrozen:" << e.what();
        return false;
    } catch (...) {
        qDebug() << "Unknown exception in setLayerFrozen";
        return false;
    }
}

bool DwgFileProcessor::setLayerVisibility(const QString& layerName, bool visible) {
    if (!m_database) return false;
    
    try {
        AcDbLayerTable* pLayerTable;
        if (m_database->getLayerTable(pLayerTable, AcDb::kForRead) != Acad::eOk) {
            return false;
        }
        
        AcDbLayerTableRecord* pLayerRecord;
        if (pLayerTable->getAt(layerName.toStdWString().c_str(), pLayerRecord, AcDb::kForWrite) == Acad::eOk) {
            pLayerRecord->setIsOff(!visible);  // Note: setIsOff uses inverted logic
            pLayerRecord->close();
            pLayerTable->close();
            return true;
        }
        
        pLayerTable->close();
        return false;
    } catch (...) {
        return false;
    }
}

bool DwgFileProcessor::setLayerLinetype(const QString& layerName, const QString& linetype) {
    if (!m_database) return false;
    
    try {
        // First, get the linetype ID
        AcDbObjectId linetypeId;
        AcDbLinetypeTable* pLinetypeTable;
        if (m_database->getLinetypeTable(pLinetypeTable, AcDb::kForRead) == Acad::eOk) {
            if (!pLinetypeTable->has(linetype.toStdWString().c_str())) {
                // If linetype doesn't exist, try to load it or use Continuous
                pLinetypeTable->getAt(L"Continuous", linetypeId);
            } else {
                pLinetypeTable->getAt(linetype.toStdWString().c_str(), linetypeId);
            }
            pLinetypeTable->close();
        } else {
            return false;
        }
        
        // Now set the linetype on the layer
        AcDbLayerTable* pLayerTable;
        if (m_database->getLayerTable(pLayerTable, AcDb::kForRead) != Acad::eOk) {
            return false;
        }
        
        AcDbLayerTableRecord* pLayerRecord;
        if (pLayerTable->getAt(layerName.toStdWString().c_str(), pLayerRecord, AcDb::kForWrite) == Acad::eOk) {
            pLayerRecord->setLinetypeObjectId(linetypeId);
            pLayerRecord->close();
            pLayerTable->close();
            return true;
        }
        
        pLayerTable->close();
        return false;
    } catch (...) {
        return false;
    }
}

bool DwgFileProcessor::setLayerTransparency(const QString& layerName, int transparency) {
    if (!m_database) return false;
    
    try {
        // Transparency in AutoCAD: 0 = opaque, 90 = 90% transparent
        // Convert percentage to AcCmTransparency
        if (transparency < 0) transparency = 0;
        if (transparency > 90) transparency = 90;
        
        AcDbLayerTable* pLayerTable;
        if (m_database->getLayerTable(pLayerTable, AcDb::kForRead) != Acad::eOk) {
            return false;
        }
        
        AcDbLayerTableRecord* pLayerRecord;
        if (pLayerTable->getAt(layerName.toStdWString().c_str(), pLayerRecord, AcDb::kForWrite) == Acad::eOk) {
            // Create transparency object
            AcCmTransparency trans;
            trans.setAlphaPercent(transparency);
            
            pLayerRecord->setTransparency(trans);
            pLayerRecord->close();
            pLayerTable->close();
            return true;
        }
        
        pLayerTable->close();
        return false;
    } catch (...) {
        return false;
    }
}

// ============================================================================
// LISP Processing Implementation
// ============================================================================

bool DwgFileProcessor::performLispExecution(const ProcessingOptions& options, ProcessingStatistics& stats) {
    // NOTE: LISP execution is now handled at BatchProcessingEngine level via subprocess
    // This method should not be called for LISP processing in the new architecture
    
    qDebug() << "[LISP-DEPRECATED] WARNING: performLispExecution called on file level";
    qDebug() << "[LISP-DEPRECATED] LISP processing is now handled via BricsCAD subprocess";
    
    // If we reach here, it means the architecture routing failed
    // Log debug info and skip LISP processing for this individual file
    if (options.enableLispExecution && !options.lispScripts.isEmpty()) {
        logLispDebug("LISP execution skipped - handled at batch level via subprocess");
        
        // Don't update statistics here as they will be updated at batch level
        // Just return true to not interfere with other processing
        return true;
    }
    
    // No LISP scripts to execute
    return true;
}

bool DwgFileProcessor::executeLispScript(const QString& scriptPath, QString& output) {
    if (!m_database) {
        output = "No database loaded";
        return false;
    }
    
    // Load LISP file content
    QString lispCode;
    if (!DwgUtils::loadLispFile(scriptPath, lispCode)) {
        output = "Failed to load LISP file";
        return false;
    }
    
    // Execute LISP code in the database
    return DwgUtils::executeLispInDatabase(m_database, lispCode, output);
}

bool DwgFileProcessor::validateLispSyntax(const QString& scriptPath, QString& errorMsg) {
    // Load LISP file content
    QString lispCode;
    if (!DwgUtils::loadLispFile(scriptPath, lispCode)) {
        errorMsg = "Failed to load file";
        return false;
    }
    
    // Parse LISP syntax
    QStringList errors;
    if (!DwgUtils::parseLispSyntax(lispCode, errors)) {
        errorMsg = errors.join("; ");
        return false;
    }
    
    return true;
}

void DwgFileProcessor::logLispDebug(const QString& message, const QString& scriptName) {
    QString debugMsg;
    if (!scriptName.isEmpty()) {
        debugMsg = QString("[LISP:%1] %2").arg(scriptName, message);
    } else {
        debugMsg = QString("[LISP] %1").arg(message);
    }
    qDebug() << debugMsg;
}

// Rest of the existing implementation remains the same...
bool DwgFileProcessor::performTextReplacement(const ProcessingOptions& options, ProcessingStatistics& stats) { 
    // Check ob Datei schreibbar ist
    QFileInfo fileInfo(m_filePath);
    if (!fileInfo.isWritable()) {
        m_lastError = QString("File is locked or read-only: %1").arg(m_filePath);
        qDebug() << m_lastError;
        return false;  // Datei ist gesperrt!
    }    
    
    // Verwende bereits geöffnete Database
    if (!m_database) {
        m_lastError = "Database not opened";
        return false;
    }
    
    int replacementCount = 0;
    
    // Start transaction
    if (!beginTransaction()) {
        m_lastError = "Failed to start transaction";
        return false;
    }
    
    try {
        // Get block table
        AcDbBlockTable* pBlockTable;
        Acad::ErrorStatus es = m_database->getBlockTable(pBlockTable, AcDb::kForRead);
        if (es != Acad::eOk) {
            abortTransaction();
            m_lastError = "Failed to access block table";
            return false;
        }
        
        // Get model space
        AcDbBlockTableRecord* pModelSpace;
        es = pBlockTable->getAt(ACDB_MODEL_SPACE, pModelSpace, AcDb::kForRead);
        pBlockTable->close();
        
        if (es != Acad::eOk) {
            abortTransaction();
            m_lastError = "Failed to access model space";
            return false;
        }
        
        // Iterate through entities
        AcDbBlockTableRecordIterator* pIter;
        es = pModelSpace->newIterator(pIter);
        pModelSpace->close();
        
        if (es != Acad::eOk) {
            abortTransaction();
            m_lastError = "Failed to create iterator";
            return false;
        }
        
        // Process all entities
        for (pIter->start(); !pIter->done(); pIter->step()) {
            AcDbEntity* pEnt;
            es = pIter->getEntity(pEnt, AcDb::kForWrite);
            
            if (es != Acad::eOk) continue;
            
            // Process DBText
            if (options.processSingleLineText && pEnt->isKindOf(AcDbText::desc())) {
                AcDbText* pText = AcDbText::cast(pEnt);
                if (pText) {
                    QString currentText = QString::fromWCharArray(pText->textString());
                    QString newText = currentText;
                    
                    // Multiple replacements OR single replacement
                    if (!options.multipleReplacements.isEmpty()) {
                        for (const auto& repl : options.multipleReplacements) {
                            if (repl.isEnabled && !repl.searchText.isEmpty()) {
                                ProcessingOptions tempOpt = options;
                                tempOpt.searchText = repl.searchText;
                                tempOpt.replaceText = repl.replaceText;
                                newText = performTextReplacement(newText, tempOpt);
                            }
                        }
                    } else if (!options.searchText.isEmpty()) {
                        newText = performTextReplacement(currentText, options);
                    }
                    
                    if (currentText != newText) {
                        pText->setTextString(newText.toStdWString().c_str());
                        replacementCount++;
                    }
                }
            }
            
            // Process MText
            if (options.processMultiLineText && pEnt->isKindOf(AcDbMText::desc())) {
                AcDbMText* pMText = AcDbMText::cast(pEnt);
                if (pMText) {
                    QString currentText = QString::fromWCharArray(pMText->contents());
                    QString newText = currentText;
                    
                    // Multiple replacements OR single replacement
                    if (!options.multipleReplacements.isEmpty()) {
                        for (const auto& repl : options.multipleReplacements) {
                            if (repl.isEnabled && !repl.searchText.isEmpty()) {
                                ProcessingOptions tempOpt = options;
                                tempOpt.searchText = repl.searchText;
                                tempOpt.replaceText = repl.replaceText;
                                newText = performTextReplacement(newText, tempOpt);
                            }
                        }
                    } else if (!options.searchText.isEmpty()) {
                        newText = performTextReplacement(currentText, options);
                    }
                    
                    if (currentText != newText) {
                        pMText->setContents(newText.toStdWString().c_str());
                        replacementCount++;
                    }
                }
            }
            
            pEnt->close();
        }
        
        delete pIter;
        
        // Commit transaction
        if (!commitTransaction()) {
            m_lastError = "Failed to commit transaction";
            return false;
        }
        
        // Update statistics
        stats.textReplacements += replacementCount;
        
        if (replacementCount > 0) {
            qDebug() << QString("Replaced text in %1 entities in file: %2")
                        .arg(replacementCount)
                        .arg(m_filePath);
        }
        
        return true;
        
    } catch (...) {
        abortTransaction();
        m_lastError = "Exception occurred during text replacement";
        return false;
    }
}

bool DwgFileProcessor::performAttributeReplacement(const ProcessingOptions& options, ProcessingStatistics& stats) {
    if (!m_database) {
        m_lastError = "Database not opened";
        return false;
    }
    
    int replacementCount = 0;
    
    // Start transaction
    if (!beginTransaction()) {
        m_lastError = "Failed to start transaction";
        return false;
    }
    
    try {
        // Get block table
        AcDbBlockTable* pBlockTable;
        Acad::ErrorStatus es = m_database->getBlockTable(pBlockTable, AcDb::kForRead);
        if (es != Acad::eOk) {
            abortTransaction();
            m_lastError = "Failed to access block table";
            return false;
        }
        
        // Get model space
        AcDbBlockTableRecord* pModelSpace;
        es = pBlockTable->getAt(ACDB_MODEL_SPACE, pModelSpace, AcDb::kForRead);
        pBlockTable->close();
        
        if (es != Acad::eOk) {
            abortTransaction();
            m_lastError = "Failed to access model space";
            return false;
        }
        
        // Iterate through entities
        AcDbBlockTableRecordIterator* pIter;
        es = pModelSpace->newIterator(pIter);
        pModelSpace->close();
        
        if (es != Acad::eOk) {
            abortTransaction();
            m_lastError = "Failed to create iterator";
            return false;
        }
        
        // Process all entities
        for (pIter->start(); !pIter->done(); pIter->step()) {
            AcDbEntity* pEnt;
            es = pIter->getEntity(pEnt, AcDb::kForRead);
            
            if (es != Acad::eOk) continue;
            
            // Process Block References
            if (pEnt->isKindOf(AcDbBlockReference::desc())) {
                AcDbBlockReference* pBlockRef = AcDbBlockReference::cast(pEnt);
                
                if (pBlockRef) {
                    // Get block name for filtering
                    AcDbBlockTableRecord* pBlockDef;
                    es = acdbOpenObject(pBlockDef, pBlockRef->blockTableRecord(), AcDb::kForRead);
                    
                    QString blockName;
                    if (es == Acad::eOk) {
                        const ACHAR* name;
                        pBlockDef->getName(name);
                        blockName = QString::fromWCharArray(name);
                        pBlockDef->close();
                    }
                    
                    // Check block filter
                    if (!options.targetBlocks.isEmpty()) {
                        bool blockMatches = false;
                        for (const QString& targetBlock : options.targetBlocks) {
                            if (blockName.contains(targetBlock, Qt::CaseInsensitive)) {
                                blockMatches = true;
                                break;
                            }
                        }
                        
                        if (!blockMatches) {
                            pEnt->close();
                            continue;
                        }
                    }
                    
                    // Process attributes
                    AcDbObjectIterator* pAttrIter = pBlockRef->attributeIterator();
                    
                    if (pAttrIter) {
                        for (pAttrIter->start(); !pAttrIter->done(); pAttrIter->step()) {
                            AcDbObjectId attrId = pAttrIter->objectId();
                            AcDbAttribute* pAttr;
                            
                            es = acdbOpenObject(pAttr, attrId, AcDb::kForWrite);
                            if (es != Acad::eOk) continue;
                            
                            // Check attribute visibility/type filters
                            if (pAttr->isInvisible() && !options.invisibleAttributesCheck) {
                                pAttr->close();
                                continue;
                            }
                            
                            if (pAttr->isConstant() && !options.constantAttributesCheck) {
                                pAttr->close();
                                continue;
                            }
                            
                            // Get attribute tag and value
                            QString attrTag = QString::fromWCharArray(pAttr->tag());
                            QString currentValue = QString::fromWCharArray(pAttr->textString());
                            
                            // Check attribute tag filter
                            if (!options.targetAttributes.isEmpty()) {
                                bool tagMatches = false;
                                for (const QString& targetTag : options.targetAttributes) {
                                    if (attrTag.compare(targetTag, Qt::CaseInsensitive) == 0) {
                                        tagMatches = true;
                                        break;
                                    }
                                }
                                if (!tagMatches) {
                                    pAttr->close();
                                    continue;
                                }
                            }
                            
                            // Perform replacement
                            QString newValue = performAttributeTextReplacement(currentValue, options);
                            
                            if (currentValue != newValue) {
                                pAttr->setTextString(newValue.toStdWString().c_str());
                                replacementCount++;
                                
                                qDebug() << QString("Block: %1, Attribute: %2, Old: '%3', New: '%4'")
                                            .arg(blockName)
                                            .arg(attrTag)
                                            .arg(currentValue)
                                            .arg(newValue);
                            }
                            
                            pAttr->close();
                        }
                        
                        delete pAttrIter;
                    }
                }
            }
            
            pEnt->close();
        }
        
        delete pIter;
        
        // Commit transaction
        if (!commitTransaction()) {
            m_lastError = "Failed to commit transaction";
            return false;
        }
        
        // Update statistics
        stats.attributeReplacements += replacementCount;
        
        if (replacementCount > 0) {
            qDebug() << QString("Replaced attributes in %1 entities in file: %2")
                        .arg(replacementCount)
                        .arg(m_filePath);
        }
        
        return true;
        
    } catch (...) {
        abortTransaction();
        m_lastError = "Exception occurred during attribute replacement";
        return false;
    }
}

// ============================================================================
// Transaction Management - FIXED for BRX Stack-based API
// ============================================================================

bool DwgFileProcessor::beginTransaction() {
    if (!m_database) return false;
    
    try {
        AcDbTransactionManager* pTransMan = m_database->transactionManager();
        if (pTransMan) {
            m_transaction = pTransMan->startTransaction();  // Push to stack
            return (m_transaction != nullptr);
        }
        return false;
    } catch (...) {
        return false;
    }
}

bool DwgFileProcessor::commitTransaction() {
    if (!m_transaction || !m_database) return false;
    
    try {
        AcDbTransactionManager* pTransMan = m_database->transactionManager();
        if (pTransMan) {
            Acad::ErrorStatus es = pTransMan->endTransaction();  // Pop from stack, no parameter
            m_transaction = nullptr;
            return (es == Acad::eOk);
        }
        return false;
    } catch (...) {
        m_transaction = nullptr;
        return false;
    }
}

void DwgFileProcessor::abortTransaction() {
    if (m_transaction && m_database) {
        try {
            AcDbTransactionManager* pTransMan = m_database->transactionManager();
            if (pTransMan) {
                pTransMan->abortTransaction();  // Pop from stack, no parameter
            }
        } catch (...) {
            // Ignore errors during abort
        }
        m_transaction = nullptr;
    }
}

// ============================================================================
// Helper Functions - FIXED for Qt6 API
// ============================================================================

QString DwgFileProcessor::performTextReplacement(const QString& text, const ProcessingOptions& options) {
    QString result = text;
    
    if (options.useRegex) {
        QRegularExpression regex(options.searchText);
        if (!options.caseSensitive) {
            regex.setPatternOptions(QRegularExpression::CaseInsensitiveOption);
        }
        result = result.replace(regex, options.replaceText);
    } else if (options.wholeWordsOnly) {
        QString pattern = QString("\\b%1\\b").arg(QRegularExpression::escape(options.searchText));
        QRegularExpression regex(pattern);
        if (!options.caseSensitive) {
            regex.setPatternOptions(QRegularExpression::CaseInsensitiveOption);
        }
        result = result.replace(regex, options.replaceText);
    } else {
        Qt::CaseSensitivity cs = options.caseSensitive ? Qt::CaseSensitive : Qt::CaseInsensitive;
        result = result.replace(options.searchText, options.replaceText, cs);
    }
    
    return result;
}

QString DwgFileProcessor::performAttributeTextReplacement(const QString& text, const ProcessingOptions& options) {
    QString result = text;
    
    if (options.attributeSearchText.isEmpty()) {
        // Leeres Suchfeld = Inhalt komplett überschreiben
        result = options.attributeReplaceText;
    } else if (options.useRegex) {
        QRegularExpression regex(options.attributeSearchText);
        if (!options.caseSensitive) {
            regex.setPatternOptions(QRegularExpression::CaseInsensitiveOption);
        }
        result = result.replace(regex, options.attributeReplaceText);
    } else if (options.wholeWordsOnly) {
        QString pattern = QString("\\b%1\\b").arg(QRegularExpression::escape(options.attributeSearchText));
        QRegularExpression regex(pattern);
        if (!options.caseSensitive) {
            regex.setPatternOptions(QRegularExpression::CaseInsensitiveOption);
        }
        result = result.replace(regex, options.attributeReplaceText);
    } else {
        Qt::CaseSensitivity cs = options.caseSensitive ? Qt::CaseSensitive : Qt::CaseInsensitive;
        result = result.replace(options.attributeSearchText, options.attributeReplaceText, cs);
    }
    
    return result;
}

QString DwgFileProcessor::getFileName() const {
    return QFileInfo(m_filePath).fileName();
}

qint64 DwgFileProcessor::getFileSize() const {
    return QFileInfo(m_filePath).size();
}

bool DwgFileProcessor::isReadOnly() const {
    QFileInfo info(m_filePath);
    return !info.isWritable();
}

// ============================================================================
// DwgProcessorFactory Implementation - Enhanced
// ============================================================================

ProcessingOptions DwgProcessorFactory::s_defaultOptions;

std::unique_ptr<BatchProcessingEngine> DwgProcessorFactory::createBatchEngine(QObject* parent) {
    return std::make_unique<BatchProcessingEngine>(parent);
}

std::unique_ptr<DwgFileProcessor> DwgProcessorFactory::createFileProcessor(const QString& filePath) {
    return std::make_unique<DwgFileProcessor>(filePath);
}

void DwgProcessorFactory::setDefaultOptions(const ProcessingOptions& options) {
    s_defaultOptions = options;
}

ProcessingOptions DwgProcessorFactory::getDefaultOptions() {
    return s_defaultOptions;
}

bool DwgProcessorFactory::isValidDwgFile(const QString& filePath) {
    return DwgUtils::isValidDwgFile(filePath);
}

QStringList DwgProcessorFactory::getSupportedFileExtensions() {
    return QStringList() << "*.dwg" << "*.dxf";
}

QString DwgProcessorFactory::getVersionString() {
    return "2.0.0 - Real BRX API";
}

// ============================================================================
// NEW: Layer Utility Functions (replaces Mock functionality)
// ============================================================================

QStringList DwgProcessorFactory::scanLayersInFile(const QString& filePath) {
    QStringList layers;
    
    try {
        // Open database
        AcDbDatabase* pDb = new AcDbDatabase(false, true);
        Acad::ErrorStatus es = pDb->readDwgFile(filePath.toStdWString().c_str());
        
        if (es == Acad::eOk) {
            layers = DwgUtils::extractLayersFromDatabase(pDb);
        }
        
        delete pDb;
    } catch (...) {
        // Return empty list on error
    }
    
    return layers;
}

LayerInfo DwgProcessorFactory::getLayerInfo(const QString& filePath, const QString& layerName) {
    LayerInfo info;
    info.name = layerName;
    
    try {
        // Open database
        AcDbDatabase* pDb = new AcDbDatabase(false, true);
        Acad::ErrorStatus es = pDb->readDwgFile(filePath.toStdWString().c_str());
        
        if (es == Acad::eOk) {
            AcDbLayerTable* pLayerTable;
            if (pDb->getLayerTable(pLayerTable, AcDb::kForRead) == Acad::eOk) {
                AcDbLayerTableRecord* pLayerRecord;
                if (pLayerTable->getAt(layerName.toStdWString().c_str(), pLayerRecord, AcDb::kForRead) == Acad::eOk) {
                    info = DwgUtils::getLayerInfoFromRecord(pLayerRecord);
                    info.name = layerName;
                    pLayerRecord->close();
                }
                pLayerTable->close();
            }
        }
        
        delete pDb;
    } catch (...) {
        // Return default info on error
    }
    
    return info;
}

bool DwgProcessorFactory::testBrxConnection() {
    return DwgUtils::isBrxAvailable();
}

// ============================================================================
// DwgUtils Namespace Implementation - Enhanced with Real BRX
// ============================================================================

namespace DwgUtils {

bool isValidDwgFile(const QString& filePath) {
    QFileInfo info(filePath);
    if (!info.exists() || !info.isFile()) {
        return false;
    }
    
    QString suffix = info.suffix().toLower();
    return (suffix == "dwg" || suffix == "dxf");
}

bool isFileReadOnly(const QString& filePath) {
    QFileInfo info(filePath);
    return info.exists() && !info.isWritable();
}

qint64 getFileSize(const QString& filePath) {
    return QFileInfo(filePath).size();
}

QString getFileChecksum(const QString& filePath) {
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        return QString();
    }
    
    QCryptographicHash hash(QCryptographicHash::Md5);
    hash.addData(&file);
    return hash.result().toHex();
}

QString generateBackupFileName(const QString& originalFile, const QString& suffix) {
    QFileInfo info(originalFile);
    QString timestamp = QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss");
    return QString("%1/%2_%3%4.%5")
           .arg(info.absolutePath())
           .arg(info.baseName())
           .arg(timestamp)
           .arg(suffix)
           .arg(info.suffix());
}

bool createFileBackup(const QString& sourceFile, const QString& backupFile) {
    return QFile::copy(sourceFile, backupFile);
}

bool restoreFromBackup(const QString& backupFile, const QString& targetFile) {
    if (QFile::exists(targetFile)) {
        if (!QFile::remove(targetFile)) {
            return false;
        }
    }
    
    return QFile::copy(backupFile, targetFile);
}

QString performRegexReplacement(const QString& source, const QString& pattern, const QString& replacement) {
    QRegularExpression regex(pattern);
    return QString(source).replace(regex, replacement);
}

QString performWildcardReplacement(const QString& source, const QString& pattern, const QString& replacement) {
    QString regexPattern = QRegularExpression::wildcardToRegularExpression(pattern);
    return performRegexReplacement(source, regexPattern, replacement);
}

// FIXED: Qt6 API compatibility
bool matchesPattern(const QString& text, const QString& pattern, bool useRegex, bool caseSensitive) {
    if (useRegex) {
        QRegularExpression regex(pattern, 
            caseSensitive ? QRegularExpression::NoPatternOption 
                         : QRegularExpression::CaseInsensitiveOption);
        return regex.match(text).hasMatch();
    } else {
        return text.contains(pattern, caseSensitive ? Qt::CaseSensitive : Qt::CaseInsensitive);
    }
}

QString formatElapsedTime(qint64 milliseconds) {
    qint64 seconds = milliseconds / 1000;
    qint64 minutes = seconds / 60;
    qint64 hours = minutes / 60;
    
    if (hours > 0) {
        return QString("%1h %2m %3s")
               .arg(hours)
               .arg(minutes % 60)
               .arg(seconds % 60);
    } else if (minutes > 0) {
        return QString("%1m %2s")
               .arg(minutes)
               .arg(seconds % 60);
    } else {
        return QString("%1.%2s")
               .arg(seconds)
               .arg((milliseconds % 1000) / 100);
    }
}

QString formatFileSize(qint64 bytes) {
    const qint64 KB = 1024;
    const qint64 MB = KB * 1024;
    const qint64 GB = MB * 1024;
    
    if (bytes >= GB) {
        return QString("%1 GB").arg(bytes / double(GB), 0, 'f', 2);
    } else if (bytes >= MB) {
        return QString("%1 MB").arg(bytes / double(MB), 0, 'f', 2);
    } else if (bytes >= KB) {
        return QString("%1 KB").arg(bytes / double(KB), 0, 'f', 2);
    } else {
        return QString("%1 bytes").arg(bytes);
    }
}

QString formatProcessingRate(int filesProcessed, qint64 elapsedTime) {
    if (elapsedTime == 0) return "N/A";
    
    double filesPerSecond = (filesProcessed * 1000.0) / elapsedTime;
    return QString("%1 files/sec").arg(filesPerSecond, 0, 'f', 2);
}

// ============================================================================
// NEW: Real BRX Integration Functions
// ============================================================================

bool initializeBrxEnvironment() {
    // Initialize BRX environment if needed
    return true;
}

void cleanupBrxEnvironment() {
    // Cleanup BRX environment if needed
}

QString getBrxVersion() {
    // Return BRX version info
    return "BRX 25.0";
}

bool isBrxAvailable() {
    try {
        // Test if BRX API is available
        AcDbDatabase testDb(false, true);
        return true;
    } catch (...) {
        return false;
    }
}

// ============================================================================
// NEW: Real Layer Processing Functions - FIXED for BRX API
// ============================================================================

QStringList extractLayersFromDatabase(AcDbDatabase* database) {
    QStringList layers;
    
    if (!database) return layers;
    
    try {
        AcDbLayerTable* pLayerTable;
        if (database->getLayerTable(pLayerTable, AcDb::kForRead) == Acad::eOk) {
            AcDbLayerTableIterator* pIterator;
            if (pLayerTable->newIterator(pIterator) == Acad::eOk) {
                for (pIterator->start(); !pIterator->done(); pIterator->step()) {
                    AcDbLayerTableRecord* pLayerRecord;
                    if (pIterator->getRecord(pLayerRecord, AcDb::kForRead) == Acad::eOk) {
                        const ACHAR* layerName;
                        if (pLayerRecord->getName(layerName) == Acad::eOk) {
                            layers.append(QString::fromWCharArray(layerName));
                        }
                        pLayerRecord->close();
                    }
                }
                delete pIterator;
            }
            pLayerTable->close();
        }
    } catch (...) {
        // Return what we have so far
    }
    
    return layers;
}

// FIXED: AcDbLayerTableRecord::color() method signature
LayerInfo getLayerInfoFromRecord(AcDbLayerTableRecord* layerRecord) {
    LayerInfo info;
    
    if (!layerRecord) return info;
    
    try {
        // Get layer name
        const ACHAR* name;
        if (layerRecord->getName(name) == Acad::eOk) {
            info.name = QString::fromWCharArray(name);
        }
        
        // Get color - FIXED: Use the correct method signature
        AcCmColor color = layerRecord->color();
        info.colorIndex = color.colorIndex();
        
        // Get frozen state
        info.isFrozen = layerRecord->isFrozen();
        
        // Get off state
        info.isOff = layerRecord->isOff();
        
        // Get locked state
        info.isLocked = layerRecord->isLocked();
        
        // Get linetype name
        AcDbObjectId linetypeId = layerRecord->linetypeObjectId();
        if (!linetypeId.isNull()) {
            AcDbLinetypeTableRecord* pLinetypeRecord;
            if (acdbOpenObject(pLinetypeRecord, linetypeId, AcDb::kForRead) == Acad::eOk) {
                const ACHAR* linetypeName;
                if (pLinetypeRecord->getName(linetypeName) == Acad::eOk) {
                    info.linetypeName = QString::fromWCharArray(linetypeName);
                }
                pLinetypeRecord->close();
            }
        }
        
        // TODO: Count entities on this layer (requires iteration through model space)
        info.entityCount = 0;
        
    } catch (...) {
        // Return default info on error
    }
    
    return info;
}

bool createLayerInDatabase(AcDbDatabase* database, const QString& layerName, int colorIndex) {
    if (!database || layerName.isEmpty()) return false;
    
    try {
        AcDbLayerTable* pLayerTable;
        if (database->getLayerTable(pLayerTable, AcDb::kForWrite) != Acad::eOk) {
            return false;
        }
        
        // Check if layer already exists
        if (pLayerTable->has(layerName.toStdWString().c_str())) {
            pLayerTable->close();
            return true; // Layer already exists
        }
        
        // Create new layer record
        AcDbLayerTableRecord* pLayerRecord = new AcDbLayerTableRecord();
        pLayerRecord->setName(layerName.toStdWString().c_str());
        
        // Set color
        AcCmColor color;
        color.setColorIndex(colorIndex);
        pLayerRecord->setColor(color);
        
        // Add to layer table
        AcDbObjectId layerId;
        Acad::ErrorStatus es = pLayerTable->add(layerId, pLayerRecord);
        
        pLayerTable->close();
        pLayerRecord->close();
        
        return (es == Acad::eOk);
        
    } catch (...) {
        return false;
    }
}

bool deleteLayerFromDatabase(AcDbDatabase* database, const QString& layerName) {
    if (!database || layerName.isEmpty()) return false;
    
    qDebug() << "Attempting to delete layer:" << layerName;
    
    try {
        // Cannot delete layer "0" 
        if (layerName.compare("0", Qt::CaseInsensitive) == 0) {
            qDebug() << "Cannot delete layer 0";
            return false;
        }
        
        // Start a transaction
        AcDbTransactionManager* pTransMan = database->transactionManager();
        if (!pTransMan) {
            qDebug() << "Failed to get transaction manager";
            return false;
        }
        
        AcTransaction* pTrans = pTransMan->startTransaction();
        if (!pTrans) {
            qDebug() << "Failed to start transaction";
            return false;
        }
        
        // Get the layer ID first
        AcDbObjectId layerIdToDelete;
        {
            AcDbLayerTable* pLayerTable;
            if (database->getLayerTable(pLayerTable, AcDb::kForRead) != Acad::eOk) {
                qDebug() << "Failed to get layer table for read";
                pTransMan->abortTransaction();
                return false;
            }
            
            if (!pLayerTable->has(layerName.toStdWString().c_str())) {
                qDebug() << "Layer does not exist:" << layerName;
                pLayerTable->close();
                pTransMan->abortTransaction();
                return false;
            }
            
            Acad::ErrorStatus es = pLayerTable->getAt(layerName.toStdWString().c_str(), layerIdToDelete);
            if (es != Acad::eOk) {
                qDebug() << "Failed to get layer ID for:" << layerName << "Error:" << int(es);
                pLayerTable->close();
                pTransMan->abortTransaction();
                return false;
            }
            pLayerTable->close();
        }
        
        // Check if this is the current layer and switch if needed
        AcDbObjectId currentLayerId = database->clayer();
        if (currentLayerId == layerIdToDelete) {
            qDebug() << "Switching from current layer:" << layerName << "to layer 0";
            AcDbLayerTable* pLayerTable;
            if (database->getLayerTable(pLayerTable, AcDb::kForRead) == Acad::eOk) {
                AcDbObjectId layer0Id;
                if (pLayerTable->getAt(L"0", layer0Id) == Acad::eOk) {
                    database->setClayer(layer0Id);
                }
                pLayerTable->close();
            }
        }
        
        // STEP 1: Delete all entities on this layer
        int deletedEntities = 0;
        int failedEntities = 0;
        
        // Scan and delete entities in Model Space
        {
            AcDbBlockTable* pBlockTable;
            if (database->getBlockTable(pBlockTable, AcDb::kForRead) == Acad::eOk) {
                AcDbBlockTableRecord* pModelSpace;
                if (pBlockTable->getAt(ACDB_MODEL_SPACE, pModelSpace, AcDb::kForWrite) == Acad::eOk) {
                    AcDbBlockTableRecordIterator* pIter;
                    if (pModelSpace->newIterator(pIter) == Acad::eOk) {
                        
                        // Collect entities to delete (can't delete during iteration)
                        std::vector<AcDbObjectId> entitiesToDelete;
                        
                        for (pIter->start(); !pIter->done(); pIter->step()) {
                            AcDbEntity* pEnt;
                            if (pIter->getEntity(pEnt, AcDb::kForRead) == Acad::eOk) {
                                if (pEnt->layerId() == layerIdToDelete) {
                                    entitiesToDelete.push_back(pEnt->objectId());
                                }
                                pEnt->close();
                            }
                        }
                        delete pIter;
                        
                        qDebug() << "Found" << entitiesToDelete.size() << "entities on layer" << layerName;
                        
                        // Now delete the collected entities
                        for (const auto& entId : entitiesToDelete) {
                            AcDbEntity* pEnt;
                            if (acdbOpenObject(pEnt, entId, AcDb::kForWrite) == Acad::eOk) {
                                Acad::ErrorStatus es = pEnt->erase();
                                if (es == Acad::eOk) {
                                    deletedEntities++;
                                } else {
                                    failedEntities++;
                                    qDebug() << "Failed to delete entity, error:" << int(es);
                                }
                                pEnt->close();
                            } else {
                                failedEntities++;
                                qDebug() << "Failed to open entity for deletion";
                            }
                        }
                        
                        if (deletedEntities > 0) {
                            qDebug() << "Deleted" << deletedEntities << "entities from layer" << layerName;
                        }
                        if (failedEntities > 0) {
                            qDebug() << "Failed to delete" << failedEntities << "entities from layer" << layerName;
                        }
                    }
                    pModelSpace->close();
                }
                
                // Also check Paper Space (simplified without AcDbLayout)
                // Paper space is typically named "*Paper_Space" or "*Paper_Space0", "*Paper_Space1", etc.
                AcDbBlockTableIterator* pTableIter;
                if (pBlockTable->newIterator(pTableIter) == Acad::eOk) {
                    for (pTableIter->start(); !pTableIter->done(); pTableIter->step()) {
                        AcDbBlockTableRecord* pBlock;
                        if (pTableIter->getRecord(pBlock, AcDb::kForWrite) == Acad::eOk) {
                            const ACHAR* blockName;
                            pBlock->getName(blockName);
                            QString qBlockName = QString::fromWCharArray(blockName);
                            
                            // Check if it's a paper space block
                            if (qBlockName.startsWith("*Paper_Space") || pBlock->isLayout()) {
                                AcDbBlockTableRecordIterator* pBlockIter;
                                if (pBlock->newIterator(pBlockIter) == Acad::eOk) {
                                    
                                    std::vector<AcDbObjectId> paperEntitiesToDelete;
                                    
                                    for (pBlockIter->start(); !pBlockIter->done(); pBlockIter->step()) {
                                        AcDbEntity* pEnt;
                                        if (pBlockIter->getEntity(pEnt, AcDb::kForRead) == Acad::eOk) {
                                            if (pEnt->layerId() == layerIdToDelete) {
                                                paperEntitiesToDelete.push_back(pEnt->objectId());
                                            }
                                            pEnt->close();
                                        }
                                    }
                                    delete pBlockIter;
                                    
                                    // Delete entities in paper space
                                    for (const auto& entId : paperEntitiesToDelete) {
                                        AcDbEntity* pEnt;
                                        if (acdbOpenObject(pEnt, entId, AcDb::kForWrite) == Acad::eOk) {
                                            if (pEnt->erase() == Acad::eOk) {
                                                deletedEntities++;
                                            }
                                            pEnt->close();
                                        }
                                    }
                                }
                            }
                            pBlock->close();
                        }
                    }
                    delete pTableIter;
                }
                
                pBlockTable->close();
            }
        }
        
        // STEP 2: Now delete the layer itself
        bool layerDeleted = false;
        {
            AcDbLayerTable* pLayerTable;
            if (database->getLayerTable(pLayerTable, AcDb::kForWrite) == Acad::eOk) {
                AcDbLayerTableRecord* pLayerRecord;
                if (pLayerTable->getAt(layerName.toStdWString().c_str(), pLayerRecord, AcDb::kForWrite) == Acad::eOk) {
                    
                    // Check if layer is frozen or locked
                    if (pLayerRecord->isFrozen()) {
                        qDebug() << "Layer" << layerName << "is frozen, unfreezing first";
                        pLayerRecord->setIsFrozen(false);
                    }
                    if (pLayerRecord->isLocked()) {
                        qDebug() << "Layer" << layerName << "is locked, unlocking first";
                        pLayerRecord->setIsLocked(false);
                    }
                    
                    Acad::ErrorStatus es = pLayerRecord->erase();
                    if (es == Acad::eOk) {
                        layerDeleted = true;
                        qDebug() << "Successfully deleted layer:" << layerName;
                    } else {
                        qDebug() << "Failed to delete layer:" << layerName << "Error code:" << int(es);
                        // Provide more detailed error information
                        switch(es) {
                            case Acad::eWasErased:
                                qDebug() << "  -> Layer was already erased";
                                break;
                            case Acad::eWasOpenForWrite:
                                qDebug() << "  -> Layer is open for write by another operation";
                                break;
                            case Acad::eWasOpenForRead:
                                qDebug() << "  -> Layer is open for read by another operation";
                                break;
                            case Acad::eObjectToBeDeleted:
                                qDebug() << "  -> Layer is already marked for deletion";
                                break;
                            default:
                                qDebug() << "  -> Error description: Layer may be referenced by blocks or other objects";
                                break;
                        }
                    }
                    pLayerRecord->close();
                }
                pLayerTable->close();
            }
        }
        
        // Commit or abort transaction
        if (layerDeleted) {
            pTransMan->endTransaction();
            qDebug() << "Layer" << layerName << "deleted with" << deletedEntities << "entities";
            return true;
        } else {
            pTransMan->abortTransaction();
            qDebug() << "Transaction aborted for layer" << layerName;
            return false;
        }
        
    } catch (const std::exception& e) {
        qDebug() << "Exception in deleteLayerFromDatabase:" << e.what();
        return false;
    } catch (...) {
        qDebug() << "Unknown exception in deleteLayerFromDatabase";
        return false;
    }
}

bool renameLayerInDatabase(AcDbDatabase* database, const QString& oldName, const QString& newName) {
    if (!database || oldName.isEmpty() || newName.isEmpty()) return false;
    
    // Cannot rename layer "0"
    if (oldName.compare("0", Qt::CaseInsensitive) == 0) {
        return false;
    }
    
    try {
        AcDbLayerTable* pLayerTable;
        if (database->getLayerTable(pLayerTable, AcDb::kForWrite) != Acad::eOk) {
            return false;
        }
        
        // Check if new name already exists
        if (pLayerTable->has(newName.toStdWString().c_str())) {
            pLayerTable->close();
            return false;
        }
        
        // Get layer record
        AcDbLayerTableRecord* pLayerRecord;
        if (pLayerTable->getAt(oldName.toStdWString().c_str(), pLayerRecord, AcDb::kForWrite) == Acad::eOk) {
            Acad::ErrorStatus es = pLayerRecord->setName(newName.toStdWString().c_str());
            pLayerRecord->close();
            pLayerTable->close();
            return (es == Acad::eOk);
        }
        
        pLayerTable->close();
        return false;
        
    } catch (...) {
        return false;
    }
}

bool setLayerPropertiesInDatabase(AcDbDatabase* database, const QString& layerName, const LayerInfo& info) {
    if (!database || layerName.isEmpty()) return false;
    
    try {
        AcDbLayerTable* pLayerTable;
        if (database->getLayerTable(pLayerTable, AcDb::kForRead) != Acad::eOk) {
            return false;
        }
        
        AcDbLayerTableRecord* pLayerRecord;
        if (pLayerTable->getAt(layerName.toStdWString().c_str(), pLayerRecord, AcDb::kForWrite) == Acad::eOk) {
            
            // Set color if specified
            if (info.colorIndex > 0) {
                AcCmColor color;
                color.setColorIndex(info.colorIndex);
                pLayerRecord->setColor(color);
            }
            
            // Set frozen state
            if (info.isFrozen) {
                pLayerRecord->setIsFrozen(info.isFrozen);
            }
            
            // Set off state
            if (info.isOff) {
                pLayerRecord->setIsOff(info.isOff);
            }
            
            // Set locked state
            if (info.isLocked) {
                pLayerRecord->setIsLocked(info.isLocked);
            }
            
            pLayerRecord->close();
            pLayerTable->close();
            return true;
        }
        
        pLayerTable->close();
        return false;
        
    } catch (...) {
        return false;
    }
}

// ============================================================================
// LISP Processing Utilities Implementation
// ============================================================================

bool executeLispInDatabase(AcDbDatabase* database, const QString& lispCode, QString& result) {
    if (!database || lispCode.isEmpty()) {
        result = "Invalid parameters";
        return false;
    }
    
    try {
        // BRX supports acedEvaluateLisp as an undocumented but popular ARX function
        // We'll try multiple approaches for maximum compatibility
        
        // Method 1: Direct LISP evaluation using acedEvaluateLisp
        // This function is mentioned in BRX documentation as supported
        struct resbuf* pResult = nullptr;
        
        // Convert QString to ACHAR* for BRX API
        std::wstring wLispCode = lispCode.toStdWString();
        const ACHAR* achLispCode = wLispCode.c_str();
        
        // Try to evaluate the LISP expression
        // Note: acedEvaluateLisp signature in BRX/ARX:
        // int acedEvaluateLisp(const ACHAR* lispExpr, struct resbuf** result)
        int retVal = RTERROR;
        
        // First attempt: Try acedEvaluateLisp if available at runtime
        // This is an undocumented function - loaded via GetProcAddress
        pfnAcedEvaluateLisp pEvalLisp = getAcedEvaluateLisp();
        if (pEvalLisp) {
            retVal = pEvalLisp(achLispCode, &pResult);
        }
        
        if (retVal == RTNORM) {
            // Success - convert result to QString
            if (pResult) {
                switch (pResult->restype) {
                    case RTSTR:
                        result = QString::fromWCharArray(pResult->resval.rstring);
                        break;
                    case RTREAL:
                        result = QString::number(pResult->resval.rreal);
                        break;
                    case RTSHORT:
                        result = QString::number(pResult->resval.rint);
                        break;
                    case RTLONG:
                        result = QString::number(pResult->resval.rlong);
                        break;
                    case RTT:
                        result = "T"; // True
                        break;
                    case RTNIL:
                        result = "nil"; // Nil/False
                        break;
                    default:
                        result = QString("Result type: %1").arg(pResult->restype);
                        break;
                }
                
                // Free result buffer
                acutRelRb(pResult);
            } else {
                result = "LISP evaluation successful (no return value)";
            }
            
            return true;
        }
        
        // Method 2: Fallback using acedInvoke for LISP functions
        // This is useful if the LISP code is a function call
        if (retVal != RTNORM && lispCode.startsWith("(") && lispCode.contains(" ")) {
            // Parse function name from LISP expression
            QString funcName = lispCode.mid(1, lispCode.indexOf(' ') - 1);
            
            // Build resbuf chain for function and arguments
            struct resbuf* rbArgs = acutBuildList(
                RTSTR, funcName.toStdWString().c_str(),
                RTNONE);
            
            if (rbArgs) {
                struct resbuf* rbResult = nullptr;
                
                // Try acedInvoke
                retVal = acedInvoke(rbArgs, &rbResult);
                
                if (retVal == RTNORM) {
                    if (rbResult) {
                        // Process result
                        switch (rbResult->restype) {
                            case RTSTR:
                                result = QString::fromWCharArray(rbResult->resval.rstring);
                                break;
                            case RTREAL:
                                result = QString::number(rbResult->resval.rreal);
                                break;
                            case RTSHORT:
                            case RTLONG:
                                result = QString::number(rbResult->resval.rint);
                                break;
                            default:
                                result = "Function executed successfully";
                                break;
                        }
                        acutRelRb(rbResult);
                    }
                    
                    acutRelRb(rbArgs);
                    return true;
                }
                
                acutRelRb(rbArgs);
            }
        }
        
        // Method 3: Final fallback - use command line execution
        // This works for simple LISP expressions
        if (retVal != RTNORM) {
            // Send LISP expression via command line
            // Note: This is asynchronous and less reliable
            QString cmdString = QString("(princ %1)").arg(lispCode);
            std::wstring wCmdString = cmdString.toStdWString();
            
            // Use acedCommand to send the LISP expression
            retVal = acedCommand(RTSTR, wCmdString.c_str(), RTNONE);
            
            if (retVal == RTNORM) {
                result = "LISP expression sent to command line";
                return true;
            }
        }
        
        // If all methods failed
        result = QString("LISP evaluation failed with code: %1").arg(retVal);
        return false;
        
    } catch (const std::exception& e) {
        result = QString("Exception: %1").arg(e.what());
        return false;
    } catch (...) {
        result = "Unknown exception during LISP execution";
        return false;
    }
}

bool loadLispFile(const QString& filePath, QString& lispCode) {
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return false;
    }
    
    QTextStream stream(&file);
    lispCode = stream.readAll();
    file.close();
    
    return !lispCode.isEmpty();
}

bool parseLispSyntax(const QString& lispCode, QStringList& errors) {
    errors.clear();
    
    if (lispCode.isEmpty()) {
        errors << "Empty LISP code";
        return false;
    }
    
    // Basic syntax validation
    int openParens = 0;
    int closeParens = 0;
    int lineNumber = 1;
    bool inString = false;
    bool inComment = false;
    QChar prevChar;
    
    for (const QChar& ch : lispCode) {
        // Handle line counting
        if (ch == '\n') {
            lineNumber++;
            inComment = false; // Comments end at line break
            continue;
        }
        
        // Skip comment content
        if (inComment) {
            continue;
        }
        
        // Check for comment start
        if (ch == ';' && !inString) {
            inComment = true;
            continue;
        }
        
        // Handle string literals
        if (ch == '"' && prevChar != '\\') {
            inString = !inString;
        }
        
        // Count parentheses outside of strings
        if (!inString) {
            if (ch == '(') {
                openParens++;
            } else if (ch == ')') {
                closeParens++;
                if (closeParens > openParens) {
                    errors << formatLispError("Unexpected closing parenthesis", lineNumber);
                }
            }
        }
        
        prevChar = ch;
    }
    
    // Check for unclosed string
    if (inString) {
        errors << "Unclosed string literal";
    }
    
    // Check parentheses balance
    if (openParens != closeParens) {
        if (openParens > closeParens) {
            errors << formatLispError(QString("Missing %1 closing parenthesis(es)")
                                     .arg(openParens - closeParens), -1);
        } else {
            errors << formatLispError(QString("Extra %1 closing parenthesis(es)")
                                     .arg(closeParens - openParens), -1);
        }
    }
    
    // Check for common LISP syntax errors
    QStringList lines = lispCode.split('\n');
    lineNumber = 0;
    
    for (const QString& line : lines) {
        lineNumber++;
        QString trimmedLine = line.trimmed();
        
        // Skip empty lines and comments
        if (trimmedLine.isEmpty() || trimmedLine.startsWith(';')) {
            continue;
        }
        
        // Check for common function typos (case-insensitive in LISP)
        QRegularExpression invalidFuncRegex("\\b(dfun|deffun|defn|seteq|letq)\\b", 
                                           QRegularExpression::CaseInsensitiveOption);
        QRegularExpressionMatch match = invalidFuncRegex.match(trimmedLine);
        if (match.hasMatch()) {
            QString badFunc = match.captured(1);
            QString suggestion;
            if (badFunc.toLower() == "dfun" || badFunc.toLower() == "deffun" || badFunc.toLower() == "defn") {
                suggestion = "defun";
            } else if (badFunc.toLower() == "seteq") {
                suggestion = "setq";
            } else if (badFunc.toLower() == "letq") {
                suggestion = "let";
            }
            
            errors << formatLispError(QString("Possible typo: '%1' (did you mean '%2'?)")
                                     .arg(badFunc, suggestion), lineNumber);
        }
        
        // Check for multiple expressions on one line without proper grouping
        if (!trimmedLine.startsWith('(') && trimmedLine.contains(QRegularExpression("\\)\\s*\\("))) {
            errors << formatLispError("Multiple expressions should be properly grouped", lineNumber);
        }
    }
    
    // Additional validation for common LISP constructs
    // Allow function names with colons (c:functionname) and other LISP-valid characters
    if (lispCode.contains("(defun") && !lispCode.contains(QRegularExpression("\\(defun\\s+[\\w:_-]+\\s*\\("))) {
        errors << "Invalid defun syntax - function name or parameter list missing";
    }
    
    return errors.isEmpty();
}

QString formatLispError(const QString& error, int lineNumber) {
    if (lineNumber > 0) {
        return QString("Line %1: %2").arg(lineNumber).arg(error);
    } else {
        return error;
    }
}

} // namespace DwgUtils

} // namespace BatchProcessing
