/**
 * @file ProcessingOptionsImpl.cpp
 * @brief Implementation of global ProcessingOptions and LispScriptManager
 * 
 * These are the global-scope classes from ProcessingOptions.h,
 * NOT the BatchProcessing::ProcessingOptions in DwgProcessor.h
 */

#include "ProcessingOptions.h"
#include <QDir>
#include <QFile>
#include <QTextStream>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

// =============================================================================
// LispScript Implementation
// =============================================================================

void LispScript::updateMetadata() {
    QFileInfo info(filePath);
    fileExists = info.exists();
    
    if (fileExists) {
        qint64 size = info.size();
        if (size >= 1024 * 1024) {
            fileSize = QString("%1 MB").arg(size / (1024.0 * 1024.0), 0, 'f', 2);
        } else if (size >= 1024) {
            fileSize = QString("%1 KB").arg(size / 1024.0, 0, 'f', 1);
        } else {
            fileSize = QString("%1 B").arg(size);
        }
        
        lastModified = info.lastModified();
        status = "OK";
    } else {
        fileSize = "N/A";
        status = "Missing";
    }
    
    fileName = info.fileName();
}

void LispScript::saveToSettings(QSettings& settings, const QString& prefix) const {
    settings.setValue(prefix + "/filePath", filePath);
    settings.setValue(prefix + "/enabled", enabled);
    settings.setValue(prefix + "/executionOrder", executionOrder);
    settings.setValue(prefix + "/description", description);
}

void LispScript::loadFromSettings(const QSettings& settings, const QString& prefix) {
    filePath = settings.value(prefix + "/filePath").toString();
    enabled = settings.value(prefix + "/enabled", true).toBool();
    executionOrder = settings.value(prefix + "/executionOrder", 0).toInt();
    description = settings.value(prefix + "/description").toString();
    fileName = QFileInfo(filePath).fileName();
    updateMetadata();
}

// =============================================================================
// LispScriptManager Implementation
// =============================================================================

void LispScriptManager::addScript(const QString& filePath) {
    if (filePath.isEmpty()) return;
    
    // Check for duplicates
    for (const auto& script : m_scripts) {
        if (script.filePath == filePath) return;
    }
    
    LispScript script(filePath, true);
    script.executionOrder = m_scripts.size();
    m_scripts.append(script);
}

void LispScriptManager::addScript(const LispScript& script) {
    // Check for duplicates
    for (const auto& existing : m_scripts) {
        if (existing.filePath == script.filePath) return;
    }
    
    m_scripts.append(script);
    updateExecutionOrder();
}

void LispScriptManager::removeScript(int index) {
    if (index >= 0 && index < m_scripts.size()) {
        m_scripts.removeAt(index);
        updateExecutionOrder();
    }
}

void LispScriptManager::removeScript(const QString& filePath) {
    for (int i = 0; i < m_scripts.size(); ++i) {
        if (m_scripts[i].filePath == filePath) {
            m_scripts.removeAt(i);
            updateExecutionOrder();
            return;
        }
    }
}

void LispScriptManager::clearAllScripts() {
    m_scripts.clear();
}

void LispScriptManager::moveScriptUp(int index) {
    if (index > 0 && index < m_scripts.size()) {
        m_scripts.swapItemsAt(index, index - 1);
        updateExecutionOrder();
    }
}

void LispScriptManager::moveScriptDown(int index) {
    if (index >= 0 && index < m_scripts.size() - 1) {
        m_scripts.swapItemsAt(index, index + 1);
        updateExecutionOrder();
    }
}

void LispScriptManager::setScriptOrder(int fromIndex, int toIndex) {
    if (fromIndex >= 0 && fromIndex < m_scripts.size() &&
        toIndex >= 0 && toIndex < m_scripts.size()) {
        m_scripts.move(fromIndex, toIndex);
        updateExecutionOrder();
    }
}

void LispScriptManager::sortScriptsByName() {
    std::sort(m_scripts.begin(), m_scripts.end(), 
              [](const LispScript& a, const LispScript& b) {
        return a.fileName.toLower() < b.fileName.toLower();
    });
    updateExecutionOrder();
}

void LispScriptManager::sortScriptsByPath() {
    std::sort(m_scripts.begin(), m_scripts.end(),
              [](const LispScript& a, const LispScript& b) {
        return a.filePath.toLower() < b.filePath.toLower();
    });
    updateExecutionOrder();
}

void LispScriptManager::setScriptEnabled(int index, bool enabled) {
    if (index >= 0 && index < m_scripts.size()) {
        m_scripts[index].enabled = enabled;
    }
}

void LispScriptManager::enableAllScripts() {
    for (auto& script : m_scripts) {
        script.enabled = true;
    }
}

void LispScriptManager::disableAllScripts() {
    for (auto& script : m_scripts) {
        script.enabled = false;
    }
}

LispScript* LispScriptManager::findScript(const QString& filePath) {
    for (auto& script : m_scripts) {
        if (script.filePath == filePath) {
            return &script;
        }
    }
    return nullptr;
}

QList<LispScript> LispScriptManager::getEnabledScripts() const {
    QList<LispScript> result;
    for (const auto& script : m_scripts) {
        if (script.enabled) {
            result.append(script);
        }
    }
    return result;
}

QStringList LispScriptManager::getEnabledScriptPaths() const {
    QStringList paths;
    for (const auto& script : m_scripts) {
        if (script.enabled && script.fileExists) {
            paths.append(script.filePath);
        }
    }
    return paths;
}

bool LispScriptManager::validateAllScripts() {
    bool allValid = true;
    for (auto& script : m_scripts) {
        script.updateMetadata();
        if (!script.isValid()) {
            allValid = false;
        }
    }
    return allValid;
}

QStringList LispScriptManager::getValidationErrors() const {
    QStringList errors;
    for (const auto& script : m_scripts) {
        QString error = script.getValidationError();
        if (!error.isEmpty()) {
            errors.append(QString("%1: %2").arg(script.fileName, error));
        }
    }
    return errors;
}

void LispScriptManager::updateAllMetadata() {
    for (auto& script : m_scripts) {
        script.updateMetadata();
    }
}

void LispScriptManager::saveToSettings(const QString& group) const {
    QSettings settings;
    settings.beginGroup(group);
    settings.setValue("count", m_scripts.size());
    
    for (int i = 0; i < m_scripts.size(); ++i) {
        m_scripts[i].saveToSettings(settings, QString("script_%1").arg(i));
    }
    
    settings.endGroup();
}

void LispScriptManager::loadFromSettings(const QString& group) {
    QSettings settings;
    settings.beginGroup(group);
    
    int count = settings.value("count", 0).toInt();
    m_scripts.clear();
    
    for (int i = 0; i < count; ++i) {
        LispScript script;
        script.loadFromSettings(settings, QString("script_%1").arg(i));
        m_scripts.append(script);
    }
    
    settings.endGroup();
}

bool LispScriptManager::exportToFile(const QString& filePath) const {
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return false;
    }
    
    QTextStream stream(&file);
    for (const auto& script : m_scripts) {
        stream << script.filePath << "\n";
    }
    
    file.close();
    return true;
}

bool LispScriptManager::importFromFile(const QString& filePath) {
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return false;
    }
    
    QTextStream stream(&file);
    while (!stream.atEnd()) {
        QString line = stream.readLine().trimmed();
        if (!line.isEmpty()) {
            addScript(line);
        }
    }
    
    file.close();
    return true;
}

QString LispScriptManager::getStatisticsSummary() const {
    int total = m_scripts.size();
    int enabled = getEnabledScriptCount();
    int valid = 0;
    int missing = 0;
    
    for (const auto& script : m_scripts) {
        if (script.fileExists) valid++;
        else missing++;
    }
    
    return QString("Total: %1, Enabled: %2, Valid: %3, Missing: %4")
           .arg(total).arg(enabled).arg(valid).arg(missing);
}

void LispScriptManager::updateExecutionOrder() {
    for (int i = 0; i < m_scripts.size(); ++i) {
        m_scripts[i].executionOrder = i;
    }
}

bool LispScriptManager::isValidLispFile(const QString& filePath) const {
    if (filePath.isEmpty()) return false;
    
    QFileInfo info(filePath);
    if (!info.exists()) return false;
    
    QString suffix = info.suffix().toLower();
    return (suffix == "lsp" || suffix == "lisp");
}

// =============================================================================
// ProcessingOptions Implementation (global scope)
// =============================================================================

void ProcessingOptions::initializeDefaults() {
    // Text Processing
    enableTextReplacement = false;
    searchText.clear();
    replaceText.clear();
    useRegex = false;
    useWildcards = false;
    caseSensitive = false;
    wholeWordsOnly = false;
    replaceInTextEntities = true;
    replaceInMTextEntities = true;
    replaceInDimensions = false;
    replaceInTables = false;
    
    // Attribute Processing
    enableAttributeReplacement = false;
    attributeSearchText.clear();
    attributeReplaceText.clear();
    targetAttributes.clear();
    targetBlocks.clear();
    attributeUseRegex = false;
    attributeCaseSensitive = false;
    replaceAttributeValues = true;
    replaceAttributeTags = false;
    includeInvisibleAttributes = false;
    
    // Layer Management
    enableLayerOperations = false;
    layerOperations.clear();
    createMissingLayers = true;
    preserveLayerProperties = true;
    updateLayerStandards = false;
    
    // LISP (v5.1: nur Hauptschalter + ScriptManager)
    enableLispExecution = false;
    
    // File Processing
    sourceFolder.clear();
    filePatterns = QStringList() << "*.dwg";
    includeSubfolders = false;
    createBackups = true;
    backupSuffix = ".bak";
    processReadOnlyFiles = false;
    skipEmptyFiles = true;
    skipLockedFiles = true;
    createBackupFolder = false;
    backupFolderPath.clear();
    
    // Performance
    maxConcurrentFiles = 4;
    progressUpdateInterval = 100;
    useMultiThreading = false;
    memoryLimitMB = 1024;
    
    // Logging
    enableDetailedLogging = false;
    createProcessingReport = true;
    logFilePath.clear();
    logToConsole = true;
    debugMode = false;
}

bool ProcessingOptions::validatePaths() const {
    if (sourceFolder.isEmpty()) return false;
    return QDir(sourceFolder).exists();
}

bool ProcessingOptions::validateTextOptions() const {
    if (!enableTextReplacement) return true;
    return !searchText.isEmpty();
}

bool ProcessingOptions::validateAttributeOptions() const {
    if (!enableAttributeReplacement) return true;
    return !attributeSearchText.isEmpty();
}

bool ProcessingOptions::validateLayerOptions() const {
    if (!enableLayerOperations) return true;
    return true; // Layer operations are self-contained
}

bool ProcessingOptions::validateLispOptions() const {
    if (!enableLispExecution) return true;
    return lispScriptManager.hasEnabledScripts();
}

bool ProcessingOptions::validateFileOptions() const {
    return !filePatterns.isEmpty();
}

bool ProcessingOptions::isValid() const {
    return validatePaths() && validateFileOptions();
}

QString ProcessingOptions::getValidationError() const {
    if (sourceFolder.isEmpty()) return "Source folder not specified";
    if (!QDir(sourceFolder).exists()) return "Source folder does not exist";
    if (filePatterns.isEmpty()) return "No file patterns specified";
    return QString();
}

void ProcessingOptions::loadDefaultSettings() {
    initializeDefaults();
}

void ProcessingOptions::loadConservativeSettings() {
    initializeDefaults();
    createBackups = true;
    maxConcurrentFiles = 1;
    useMultiThreading = false;
    enableDetailedLogging = true;
}

void ProcessingOptions::loadAggressiveSettings() {
    initializeDefaults();
    createBackups = false;
    maxConcurrentFiles = 8;
    useMultiThreading = true;
    enableDetailedLogging = false;
}

void ProcessingOptions::saveToSettings(const QString& group) const {
    QSettings settings;
    settings.beginGroup(group);
    settings.setValue("sourceFolder", sourceFolder);
    settings.setValue("includeSubfolders", includeSubfolders);
    settings.setValue("createBackups", createBackups);
    settings.setValue("enableTextReplacement", enableTextReplacement);
    settings.setValue("enableAttributeReplacement", enableAttributeReplacement);
    settings.setValue("enableLayerOperations", enableLayerOperations);
    settings.setValue("enableLispExecution", enableLispExecution);
    settings.endGroup();
    
    lispScriptManager.saveToSettings(group + "/LispScripts");
}

void ProcessingOptions::loadFromSettings(const QString& group) {
    QSettings settings;
    settings.beginGroup(group);
    sourceFolder = settings.value("sourceFolder").toString();
    includeSubfolders = settings.value("includeSubfolders", false).toBool();
    createBackups = settings.value("createBackups", true).toBool();
    enableTextReplacement = settings.value("enableTextReplacement", false).toBool();
    enableAttributeReplacement = settings.value("enableAttributeReplacement", false).toBool();
    enableLayerOperations = settings.value("enableLayerOperations", false).toBool();
    enableLispExecution = settings.value("enableLispExecution", false).toBool();
    settings.endGroup();
    
    lispScriptManager.loadFromSettings(group + "/LispScripts");
}

bool ProcessingOptions::exportToFile(const QString& filePath) const {
    // Simple JSON export
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly)) return false;
    
    QJsonObject obj;
    obj["sourceFolder"] = sourceFolder;
    obj["enableTextReplacement"] = enableTextReplacement;
    obj["enableLispExecution"] = enableLispExecution;
    
    QJsonDocument doc(obj);
    file.write(doc.toJson());
    file.close();
    return true;
}

bool ProcessingOptions::importFromFile(const QString& filePath) {
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) return false;
    
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    file.close();
    
    if (doc.isNull()) return false;
    
    QJsonObject obj = doc.object();
    sourceFolder = obj["sourceFolder"].toString();
    enableTextReplacement = obj["enableTextReplacement"].toBool();
    enableLispExecution = obj["enableLispExecution"].toBool();
    
    return true;
}

ProcessingOptions ProcessingOptions::copy() const {
    return *this; // Default copy works due to value semantics
}

void ProcessingOptions::reset() {
    initializeDefaults();
}

QString ProcessingOptions::getConfigurationSummary() const {
    QStringList features;
    if (enableTextReplacement) features << "Text Replacement";
    if (enableAttributeReplacement) features << "Attribute Replacement";
    if (enableLayerOperations) features << "Layer Operations";
    if (enableLispExecution) features << QString("LISP Scripts (%1)").arg(lispScriptManager.getEnabledScriptCount());
    
    if (features.isEmpty()) return "No features enabled";
    return features.join(", ");
}

QStringList ProcessingOptions::getEnabledFeatures() const {
    QStringList features;
    if (enableTextReplacement) features << "Text Replacement";
    if (enableAttributeReplacement) features << "Attribute Replacement";
    if (enableLayerOperations) features << "Layer Operations";
    if (enableLispExecution) features << "LISP Execution";
    return features;
}

int ProcessingOptions::getEstimatedComplexity() const {
    int complexity = 1;
    if (enableTextReplacement) complexity += 2;
    if (enableAttributeReplacement) complexity += 2;
    if (enableLayerOperations) complexity += 2;
    if (enableLispExecution) complexity += 3;
    if (useRegex) complexity += 1;
    return qMin(complexity, 10);
}

QString ProcessingOptions::getRecommendations() const {
    QStringList recs;
    if (!createBackups) recs << "Consider enabling backups for safety";
    if (maxConcurrentFiles > 4) recs << "High concurrency may cause file locking issues";
    if (recs.isEmpty()) return "Configuration looks good";
    return recs.join("; ");
}

// =============================================================================
// ProcessingUtils Namespace Implementation
// =============================================================================

namespace ProcessingUtils {

bool isValidDwgFile(const QString& filePath) {
    QFileInfo info(filePath);
    if (!info.exists()) return false;
    QString suffix = info.suffix().toLower();
    return (suffix == "dwg" || suffix == "dxf");
}

bool isValidLispFile(const QString& filePath) {
    QFileInfo info(filePath);
    if (!info.exists()) return false;
    QString suffix = info.suffix().toLower();
    return (suffix == "lsp" || suffix == "lisp");
}

QString formatFileSize(qint64 bytes) {
    if (bytes >= 1024 * 1024) {
        return QString("%1 MB").arg(bytes / (1024.0 * 1024.0), 0, 'f', 2);
    } else if (bytes >= 1024) {
        return QString("%1 KB").arg(bytes / 1024.0, 0, 'f', 1);
    }
    return QString("%1 bytes").arg(bytes);
}

QString formatElapsedTime(qint64 milliseconds) {
    qint64 seconds = milliseconds / 1000;
    qint64 minutes = seconds / 60;
    if (minutes > 0) {
        return QString("%1m %2s").arg(minutes).arg(seconds % 60);
    }
    return QString("%1.%2s").arg(seconds).arg((milliseconds % 1000) / 100);
}

QString generateConfigBackupName() {
    return QString("config_backup_%1.json")
           .arg(QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss"));
}

bool backupConfiguration(const ProcessingOptions& options) {
    QString backupName = generateConfigBackupName();
    return options.exportToFile(backupName);
}

QStringList findRecentConfigurations() {
    // TODO: scan config directory
    return QStringList();
}

QStringList validateProcessingOptions(const ProcessingOptions& options) {
    QStringList errors;
    if (options.sourceFolder.isEmpty()) errors << "Source folder not set";
    if (options.filePatterns.isEmpty()) errors << "No file patterns";
    return errors;
}

QString getDetailedValidationReport(const ProcessingOptions& options) {
    QStringList errors = validateProcessingOptions(options);
    if (errors.isEmpty()) return "All settings valid";
    return errors.join("\n");
}

} // namespace ProcessingUtils
