/**
 * @file DwgProcessor.h
 * @brief Core DWG Processing Engine Header
 * 
 * Definiert die Hauptklassen für die DWG-Verarbeitung.
 * Nutzt echte BRX API für BricsCAD Integration.
 */

#ifndef DWGPROCESSOR_H
#define DWGPROCESSOR_H

#include <QString>
#include <QStringList>
#include <QList>
#include <QVector>
#include <QMap>
#include <QProgressDialog>
#include <QObject>
#include <QThread>
#include <QMutex>
#include <QTimer>
#include <memory>

// BRX SDK Includes - echte BRX API
#include "brx_importexport.h"
#include "AcDb/AcDbDatabase.h"
#include "AcDb/AcDbEntity.h"
#include "AcDb/AcDbText.h"
#include "AcDb/AcDbMText.h"
#include "AcDb/AcDbBlockTable.h"
#include "AcDb/AcDbLayerTable.h"
#include "AcDb/AcDbLayerTableRecord.h"
#include "AcDb/AcDbTransactionManager.h"
#include "AcDb/AcDbBlockReference.h"
#include "AcDb/AcDbAttribute.h"
#include "AcDb/AcDbAttributeDefinition.h"
#include "AcDb/AcDbSymbolTable.h"
#include "AcDb/AcDbSymbolTableIterator.h"
#include "AcDb/AcDbBlockTableRecord.h"
#include "AcDb/AcDbBlockTableIterator.h"
#include "AcDb/AcDbLayerTableIterator.h"

namespace BatchProcessing {

// Forward declarations
class ProcessingOptions;
class BatchProcessingEngine;
class DwgFileProcessor;
class ProcessingStatistics;
class LispProcessExecutor;
struct LispExecutionConfig;

/**
 * @brief Layer Information Structure
 * MOVED TO TOP - before first usage to fix compilation error
 */
struct LayerInfo {
    QString name;
    int colorIndex = 7;
    bool isFrozen = false;
    bool isOff = false;
    QString linetypeName = "Continuous";
    int transparency = 0;
    bool isLocked = false;
    int entityCount = 0;  // Anzahl Entities auf diesem Layer
};

/**
 * @brief Optionen für die DWG-Verarbeitung
 */
class ProcessingOptions {
public:
    ProcessingOptions();
    ~ProcessingOptions() = default;

    // Files to process
    QStringList filesToProcess;    

    // Backup Settings
    bool deleteOldBackups = false;
    bool useTimestampInBackup = false;
    bool useCustomBackupLocation = false;
    QString customBackupFolder = "";    

    // Text Processing Options
    bool enableTextReplacement = false;
    QString searchText;
    QString replaceText;
    bool useRegex = false;
    bool useWildcards = false;
    bool caseSensitive = false;
    bool wholeWordsOnly = false;
    bool processSingleLineText = true;
    bool processMultiLineText = true;
    bool processDimensionText = false;
    bool processLeaderText = false;

    // Multiple Replacements
    struct TextReplacement {
        QString searchText;
        QString replaceText;
        bool isEnabled = true;
    };
    QList<TextReplacement> multipleReplacements;
    
    // Attribute Processing Options
    bool enableAttributeReplacement = false;
    QString attributeSearchText;
    QString attributeReplaceText;
    QStringList targetAttributes;  // Filter für spezifische Attribute
    QStringList targetBlocks;      // Filter für spezifische Blöcke
    bool invisibleAttributesCheck = false;
    bool constantAttributesCheck = false;
    
    // Layer Management Options
    bool enableLayerOperations = false;
    struct LayerOperation {
        QString type;        // Delete, Rename, Change Color, etc.
        QString layerName;   // Target layer name or pattern
        QString newValue;    // New name/color/etc.
        bool isEnabled = true;
    };
    QList<LayerOperation> layerOperations;   // Liste der Layer-Operationen
    
    // LISP Processing Options
    bool enableLispExecution = false;
    QStringList lispScripts;
    
    // File Processing Options
    QString sourceFolder;
    QStringList filePatterns;      // *.dwg, *.dxf, etc.
    bool includeSubfolders = false;
    bool createBackups = true;
    QString backupSuffix = ".backup";
    
    // Advanced Options
    bool skipReadOnlyFiles = true;
    bool skipEmptyFiles = true;
    int maxConcurrentFiles = 4;    // Parallel processing
    int progressUpdateInterval = 100; // ms
    
    // Validation
    bool isValid() const;
    QString getValidationError() const;
    
    // Serialization
    void saveToSettings(const QString& group = "ProcessingOptions") const;
    void loadFromSettings(const QString& group = "ProcessingOptions");
};

/**
 * @brief Statistiken der Verarbeitung
 */
class ProcessingStatistics {
public:
    ProcessingStatistics();
    void reset();
    
    // File Statistics
    int totalFiles = 0;
    int processedFiles = 0;
    int skippedFiles = 0;
    int errorFiles = 0;
    
    // Operation Statistics
    int textReplacements = 0;
    int attributeReplacements = 0;
    int layerOperations = 0;
    int lispScriptsExecuted = 0;  // Track LISP script executions
    int backupsCreated = 0;
    
    // Performance Statistics
    qint64 startTime = 0;
    qint64 endTime = 0;
    qint64 totalProcessingTime = 0; // ms
    
    // Error Tracking
    QStringList errorMessages;
    QMap<QString, QString> fileErrors; // filename -> error message
    
    // Helper Methods
    qint64 getElapsedTime() const;
    double getAverageTimePerFile() const;
    QString getFormattedSummary() const;
    QString getFormattedReport() const;
};

/**
 * @brief Einzelne DWG-Datei Processor
 */
class DwgFileProcessor {
public:
    explicit DwgFileProcessor(const QString& filePath);
    ~DwgFileProcessor();
    
    // Main Processing
    bool processFile(const ProcessingOptions& options, ProcessingStatistics& stats);
    
    // Individual Operations
    bool performTextReplacement(const ProcessingOptions& options, ProcessingStatistics& stats);
    bool performAttributeReplacement(const ProcessingOptions& options, ProcessingStatistics& stats);
    bool performLayerOperations(const ProcessingOptions& options, ProcessingStatistics& stats);
    
    // Utility Methods
    bool openDatabase();
    bool saveDatabase();
    void closeDatabase();
    bool createBackup(const QString& backupSuffix);
    
    // File Information
    QString getFilePath() const { return m_filePath; }
    QString getFileName() const;
    qint64 getFileSize() const;
    bool isReadOnly() const;
    bool isValid() const { return m_isValid; }
    QString getLastError() const { return m_lastError; }
    
    // LISP Validation (public for UI access)
    static bool validateLispSyntax(const QString& scriptPath, QString& errorMsg);
    
private:
    ProcessingOptions m_currentOptions;
    QString m_filePath;
    bool m_isValid = false;
    QString m_lastError;
    
    // BRX API Objekte
    AcDbDatabase* m_database = nullptr;
    AcTransaction* m_transaction = nullptr;
    
    // Internal Processing Methods
    bool beginTransaction();
    bool commitTransaction();
    void abortTransaction();
    
    // Text Processing Internals
    int processTextEntities(const QString& searchText, const QString& replaceText, bool useRegex, bool caseSensitive);
    int processMTextEntities(const QString& searchText, const QString& replaceText, bool useRegex, bool caseSensitive);
    
    // Attribute Processing Internals
    int processBlockAttributes(const QString& searchText, const QString& replaceText, 
                              const QStringList& targetAttributes, const QStringList& targetBlocks);
    
    // Layer Processing Internals  
    int processLayerOperations(const QStringList& operations);
    bool createLayer(const QString& layerName, int colorIndex = 7);
    bool deleteLayer(const QString& layerName);
    bool renameLayer(const QString& oldName, const QString& newName);
    bool setLayerColor(const QString& layerName, int colorIndex);
    bool setLayerFrozen(const QString& layerName, bool freeze);
    bool setLayerVisibility(const QString& layerName, bool visible);
    bool setLayerLinetype(const QString& layerName, const QString& linetype);
    bool setLayerTransparency(const QString& layerName, int transparency);
    
    // Layer Discovery Methods - NOW LayerInfo is defined above
    QStringList scanLayersInDatabase();
    QMap<QString, LayerInfo> getLayerDetails();
    
    // LISP Processing Methods
    bool performLispExecution(const ProcessingOptions& options, ProcessingStatistics& stats);
    bool executeLispScript(const QString& scriptPath, QString& output);
    void logLispDebug(const QString& message, const QString& scriptName = QString());
    
    // Utility Internals
    QString performStringReplacement(const QString& source, const QString& searchText, 
                                   const QString& replaceText, bool useRegex, bool caseSensitive) const;
    bool matchesFilter(const QString& text, const QStringList& filters) const;
    QString performTextReplacement(const QString& text, const ProcessingOptions& options);
    QString performAttributeTextReplacement(const QString& text, const ProcessingOptions& options);
};

/**
 * @brief Haupt-Engine für Batch-Processing
 */
class BatchProcessingEngine : public QObject {
    Q_OBJECT
    
public:
    explicit BatchProcessingEngine(QObject* parent = nullptr);
    ~BatchProcessingEngine();
    
    // Main Interface
    bool startProcessing(const ProcessingOptions& options);
    void stopProcessing();
    bool isProcessing() const { return m_isProcessing; }
    
    // Statistics Access
    const ProcessingStatistics& getStatistics() const { return m_statistics; }
    ProcessingStatistics& getStatistics() { return m_statistics; }
    
    // File Discovery
    QStringList findDwgFiles(const QString& folder, const QStringList& patterns, bool includeSubfolders) const;
    
    // Layer Discovery (NEW - replaces Mock functionality)
    QStringList scanLayersInFiles(const QStringList& filePaths);
    QMap<QString, QStringList> getLayersByFile(const QStringList& filePaths);
    QMap<QString, LayerInfo> getLayerStatistics(const QStringList& filePaths);
    
    // LISP Processing (NEW - script-based)
    // Using void* to avoid include conflicts between local and global ProcessingOptions
    bool startLispProcessing(const void* globalOptions);  // Actually takes ::ProcessingOptions*
    void executeLispBatchAsync(const LispExecutionConfig& config);
    
signals:
    void progressUpdated(int current, int total, const QString& currentFile);
    void fileProcessed(const QString& filePath, bool success, const QString& message);
    void processingFinished(bool success, const QString& summary);
    void processingStarted(int totalFiles);
    void errorOccurred(const QString& error);
    void layersScanned(const QStringList& layers);  // NEW - for layer discovery
    
public slots:
    void pauseProcessing();
    void resumeProcessing();
    void abortProcessing();
    
private slots:
    void processNextFile();
    void onProgressTimer();
    void onLispProcessingFinished(int exitCode);
    
private:
    // State Management
    bool m_isProcessing = false;
    bool m_isPaused = false;
    bool m_shouldAbort = false;
    
    // Processing Data
    ProcessingOptions m_options;
    ProcessingStatistics m_statistics;
    QStringList m_fileQueue;
    int m_currentFileIndex = 0;
    
    // Threading and Progress
    QTimer* m_progressTimer = nullptr;
    QMutex m_mutex;
    std::unique_ptr<DwgFileProcessor> m_currentProcessor;
    std::unique_ptr<LispProcessExecutor> m_lispExecutor;
    
    // Layer Cache (NEW)
    QMap<QString, QStringList> m_layerCache;  // filepath -> layer names
    QMap<QString, LayerInfo> m_layerInfoCache;  // layername -> info
    
    // Internal Methods
    void initializeProcessing();
    void finalizeProcessing(bool success);
    void processFileInternal(const QString& filePath);
    bool validateOptions() const;
    void updateProgress();
    void emitProgress();
    
    // Layer Processing Helpers (NEW)
    void scanFileForLayers(const QString& filePath);
    void updateLayerCache(const QString& filePath, const QStringList& layers);
};

/**
 * @brief Factory-Klasse für DWG-Processing
 */
class DwgProcessorFactory {
public:
    static std::unique_ptr<BatchProcessingEngine> createBatchEngine(QObject* parent = nullptr);
    static std::unique_ptr<DwgFileProcessor> createFileProcessor(const QString& filePath);
    
    // Configuration
    static void setDefaultOptions(const ProcessingOptions& options);
    static ProcessingOptions getDefaultOptions();
    
    // Utility Functions
    static bool isValidDwgFile(const QString& filePath);
    static QStringList getSupportedFileExtensions();
    static QString getVersionString();
    
    // Layer Utilities (NEW - replaces Mock functionality)
    static QStringList scanLayersInFile(const QString& filePath);
    static LayerInfo getLayerInfo(const QString& filePath, const QString& layerName);
    static bool testBrxConnection();
    
private:
    static ProcessingOptions s_defaultOptions;
};

/**
 * @brief Utility-Klassen für DWG-Processing
 */
namespace DwgUtils {
    
    // File Utilities
    bool isValidDwgFile(const QString& filePath);
    bool isFileReadOnly(const QString& filePath);
    qint64 getFileSize(const QString& filePath);
    QString getFileChecksum(const QString& filePath);
    
    // Backup Utilities
    QString generateBackupFileName(const QString& originalFile, const QString& suffix);
    bool createFileBackup(const QString& sourceFile, const QString& backupFile);
    bool restoreFromBackup(const QString& backupFile, const QString& targetFile);
    
    // String Processing Utilities
    QString performRegexReplacement(const QString& source, const QString& pattern, const QString& replacement);
    QString performWildcardReplacement(const QString& source, const QString& pattern, const QString& replacement);
    bool matchesPattern(const QString& text, const QString& pattern, bool useRegex, bool caseSensitive);
    
    // Progress Utilities
    QString formatElapsedTime(qint64 milliseconds);
    QString formatFileSize(qint64 bytes);
    QString formatProcessingRate(int filesProcessed, qint64 elapsedTime);
    
    // BRX Integration Utilities (NEW)
    bool initializeBrxEnvironment();
    void cleanupBrxEnvironment();
    QString getBrxVersion();
    bool isBrxAvailable();
    
    // Layer Processing Utilities (NEW - replaces Mock)
    QStringList extractLayersFromDatabase(AcDbDatabase* database);
    LayerInfo getLayerInfoFromRecord(AcDbLayerTableRecord* layerRecord);
    bool createLayerInDatabase(AcDbDatabase* database, const QString& layerName, int colorIndex = 7);
    bool deleteLayerFromDatabase(AcDbDatabase* database, const QString& layerName);
    bool renameLayerInDatabase(AcDbDatabase* database, const QString& oldName, const QString& newName);
    bool setLayerPropertiesInDatabase(AcDbDatabase* database, const QString& layerName, const LayerInfo& info);
    
    // LISP Processing Utilities
    bool executeLispInDatabase(AcDbDatabase* database, const QString& lispCode, QString& result);
    bool loadLispFile(const QString& filePath, QString& lispCode);
    bool parseLispSyntax(const QString& lispCode, QStringList& errors);
    QString formatLispError(const QString& error, int lineNumber = -1);
    
} // namespace DwgUtils

} // namespace BatchProcessing

#endif // DWGPROCESSOR_H
