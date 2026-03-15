/**
 * @file ProcessingOptions.h
 * @brief Datenstrukturen für Batch-Processing Optionen
 * 
 * Definiert alle Einstellungen und Konfigurationen für die DWG-Verarbeitung,
 * einschließlich Text-Ersetzung, Attribut-Bearbeitung, Layer-Management
 * und LISP-Skript-Integration.
 */

#ifndef PROCESSINGOPTIONS_H
#define PROCESSINGOPTIONS_H

#include <QString>
#include <QStringList>
#include <QList>
#include <QDateTime>
#include <QFileInfo>
#include <QSettings>
#include <QDebug>

// Forward declarations
namespace BatchProcessing {
    class ProcessingStatistics;
}

// =============================================================================
// LISP Script Datenstrukturen
// =============================================================================

/**
 * @brief Einzelnes LISP-Skript mit Metadaten
 */
class LispScript {
public:
    LispScript();
    LispScript(const QString& filePath, bool enabled = true);
    
    // Basic Properties
    QString filePath;           // Vollständiger Pfad zur .lisp Datei
    QString fileName;           // Nur der Dateiname (für Anzeige)
    bool enabled = true;        // Checkbox-Status (aktiv/inaktiv)
    int executionOrder = 0;     // Reihenfolge der Ausführung
    
    // Metadata (automatisch ermittelt)
    QString fileSize;           // Dateigröße formatiert (z.B. "2.5 KB")
    QDateTime lastModified;     // Letzte Änderung
    bool fileExists = false;    // Prüfung ob Datei noch existiert
    QString status;             // Status-Text (OK, Missing, Error, etc.)
    
    // Advanced Properties
    QString description;        // Optionale Beschreibung
    QStringList dependencies;   // Abhängigkeiten zu anderen LISP-Dateien
    QString expectedRuntime;    // Geschätzte Laufzeit (z.B. "< 1s", "2-5s")
    
    // Validation und Utility
    bool isValid() const;
    QString getValidationError() const;
    void updateMetadata();      // Dateigröße, Änderungsdatum, etc. aktualisieren
    QString getDisplayText() const;  // Text für Listendarstellung
    
    // Serialization für QSettings
    void saveToSettings(QSettings& settings, const QString& prefix) const;
    void loadFromSettings(const QSettings& settings, const QString& prefix);
    
    // Comparison Operators (für Sorting)
    bool operator<(const LispScript& other) const;
    bool operator==(const LispScript& other) const;
};

/**
 * @brief LISP-Skript-Manager für alle LISP-Operationen
 */
class LispScriptManager {
public:
    LispScriptManager();
    
    // Script Management
    void addScript(const QString& filePath);
    void addScript(const LispScript& script);
    void removeScript(int index);
    void removeScript(const QString& filePath);
    void clearAllScripts();
    
    // Order Management
    void moveScriptUp(int index);
    void moveScriptDown(int index);
    void setScriptOrder(int fromIndex, int toIndex);
    void sortScriptsByName();
    void sortScriptsByPath();
    
    // Enable/Disable Management
    void setScriptEnabled(int index, bool enabled);
    void enableAllScripts();
    void disableAllScripts();
    
    // Access und Queries
    const QList<LispScript>& getScripts() const { return m_scripts; }
    QList<LispScript>& getScripts() { return m_scripts; }
    int getScriptCount() const { return m_scripts.size(); }
    int getEnabledScriptCount() const;
    bool hasScripts() const { return !m_scripts.isEmpty(); }
    bool hasEnabledScripts() const { return getEnabledScriptCount() > 0; }
    
    // Script Access
    LispScript* getScript(int index);
    const LispScript* getScript(int index) const;
    LispScript* findScript(const QString& filePath);
    QList<LispScript> getEnabledScripts() const;
    QStringList getEnabledScriptPaths() const;
    
    // Validation
    bool validateAllScripts();
    QStringList getValidationErrors() const;
    void updateAllMetadata();
    
    // Serialization
    void saveToSettings(const QString& group = "LispScripts") const;
    void loadFromSettings(const QString& group = "LispScripts");
    
    // Import/Export
    bool exportToFile(const QString& filePath) const;
    bool importFromFile(const QString& filePath);
    
    // Statistics
    QString getStatisticsSummary() const;
    
private:
    QList<LispScript> m_scripts;
    
    // Helper methods
    void updateExecutionOrder();
    bool isValidLispFile(const QString& filePath) const;
};

// =============================================================================
// Processing Options - Haupt-Konfigurationsklasse
// =============================================================================

/**
 * @brief Optionen für die DWG-Verarbeitung
 */
class ProcessingOptions {
public:
    ProcessingOptions();
    ~ProcessingOptions() = default;

    // =============================================================================
    // Text Processing Options
    // =============================================================================
    bool enableTextReplacement = false;
    QString searchText;
    QString replaceText;
    bool useRegex = false;
    bool useWildcards = false;
    bool caseSensitive = false;
    bool wholeWordsOnly = false;
    
    // Advanced Text Options
    bool replaceInTextEntities = true;      // DBText Entitäten
    bool replaceInMTextEntities = true;     // MText Entitäten
    bool replaceInDimensions = false;       // Bemaßungen
    bool replaceInTables = false;           // Tabellen
    
    // =============================================================================
    // Attribute Processing Options
    // =============================================================================
    bool enableAttributeReplacement = false;
    QString attributeSearchText;
    QString attributeReplaceText;
    QStringList targetAttributes;          // Filter für spezifische Attribute
    QStringList targetBlocks;              // Filter für spezifische Blöcke
    
    // Advanced Attribute Options
    bool attributeUseRegex = false;
    bool attributeCaseSensitive = false;
    bool replaceAttributeValues = true;     // Attributwerte
    bool replaceAttributeTags = false;      // Attribut-Tags
    bool includeInvisibleAttributes = false; // Unsichtbare Attribute
    
    // =============================================================================
    // Layer Management Options
    // =============================================================================
    bool enableLayerOperations = false;
    QStringList layerOperations;           // Liste der Layer-Operationen
    
    // Advanced Layer Options
    bool createMissingLayers = true;        // Fehlende Layer erstellen
    bool preserveLayerProperties = true;    // Layer-Eigenschaften beibehalten
    bool updateLayerStandards = false;      // Nach Standards aktualisieren
    
    // =============================================================================
    // LISP Script Options (v5.1 in-process via _.SCRIPT)
    // =============================================================================
    bool enableLispExecution = false;          // Hauptschalter für LISP-Ausführung
    LispScriptManager lispScriptManager;       // Verwaltet alle LISP-Skripte
    
    // =============================================================================
    // File Processing Options
    // =============================================================================
    QString sourceFolder;
    QStringList filePatterns;              // *.dwg, *.dxf, etc.
    bool includeSubfolders = false;
    bool createBackups = true;
    QString backupSuffix = ".bak";
    
    // Advanced File Options
    bool processReadOnlyFiles = false;      // Read-only Dateien verarbeiten
    bool skipEmptyFiles = true;             // Leere Dateien überspringen
    bool skipLockedFiles = true;            // Gesperrte Dateien überspringen
    bool createBackupFolder = false;        // Separater Backup-Ordner
    QString backupFolderPath;               // Pfad für Backup-Ordner
    
    // =============================================================================
    // Performance und Threading Options
    // =============================================================================
    int maxConcurrentFiles = 4;            // Parallel processing
    int progressUpdateInterval = 100;       // ms
    bool useMultiThreading = false;         // Experimentell
    int memoryLimitMB = 1024;              // Speicher-Limit pro Thread
    
    // =============================================================================
    // Logging und Debugging Options
    // =============================================================================
    bool enableDetailedLogging = false;    // Ausführliche Logs
    bool createProcessingReport = true;     // Abschlussbericht erstellen
    QString logFilePath;                   // Pfad für Log-Datei
    bool logToConsole = true;              // Auch in Console loggen
    bool debugMode = false;                // Debug-Modus aktivieren
    
    // =============================================================================
    // Validation Methods
    // =============================================================================
    bool isValid() const;
    QString getValidationError() const;
    
    // =============================================================================
    // Configuration Management
    // =============================================================================
    void saveToSettings(const QString& group = "ProcessingOptions") const;
    void loadFromSettings(const QString& group = "ProcessingOptions");
    
    // Presets
    void loadDefaultSettings();
    void loadConservativeSettings();       // Sichere Einstellungen
    void loadAggressiveSettings();         // Maximale Performance
    
    // Import/Export
    bool exportToFile(const QString& filePath) const;
    bool importFromFile(const QString& filePath);
    
    // =============================================================================
    // Statistics und Information
    // =============================================================================
    QString getConfigurationSummary() const;
    QStringList getEnabledFeatures() const;
    int getEstimatedComplexity() const;    // 1-10 Skala
    QString getRecommendations() const;     // Verbesserungsvorschläge
    
    // =============================================================================
    // Utility Methods
    // =============================================================================
    ProcessingOptions copy() const;         // Deep copy
    void reset();                          // Auf Defaults zurücksetzen
    bool isEmpty() const;                  // Prüft ob irgendwelche Optionen aktiviert sind
    
private:
    // Internal helper methods
    void initializeDefaults();
    bool validatePaths() const;
    bool validateTextOptions() const;
    bool validateAttributeOptions() const;
    bool validateLayerOptions() const;
    bool validateLispOptions() const;
    bool validateFileOptions() const;
};

// =============================================================================
// Inline Implementations für Performance-kritische Methoden
// =============================================================================

inline LispScript::LispScript() : executionOrder(0), enabled(true), fileExists(false) {
    status = "Not Loaded";
}

inline LispScript::LispScript(const QString& filePath, bool enabled) 
    : filePath(filePath), enabled(enabled), executionOrder(0), fileExists(false) {
    fileName = QFileInfo(filePath).fileName();
    updateMetadata();
}

inline bool LispScript::isValid() const {
    return fileExists && !filePath.isEmpty() && getValidationError().isEmpty();
}

inline QString LispScript::getValidationError() const {
    if (filePath.isEmpty()) {
        return "File path is empty";
    }
    if (!fileExists) {
        return "File does not exist";
    }
    if (!filePath.toLower().endsWith(".lsp") && !filePath.toLower().endsWith(".lisp")) {
        return "File is not a LISP script (.lsp or .lisp)";
    }
    return QString(); // No error
}

inline QString LispScript::getDisplayText() const {
    QString text = fileName;
    if (!enabled) {
        text = QString("[Disabled] %1").arg(text);
    }
    if (!fileExists) {
        text = QString("[Missing] %1").arg(text);
    }
    return text;
}

inline bool LispScript::operator<(const LispScript& other) const {
    return executionOrder < other.executionOrder;
}

inline bool LispScript::operator==(const LispScript& other) const {
    return filePath == other.filePath;
}

inline LispScriptManager::LispScriptManager() {
    // Constructor body (implementation in .cpp file)
}

inline int LispScriptManager::getEnabledScriptCount() const {
    int count = 0;
    for (const auto& script : m_scripts) {
        if (script.enabled) {
            count++;
        }
    }
    return count;
}

inline LispScript* LispScriptManager::getScript(int index) {
    if (index >= 0 && index < m_scripts.size()) {
        return &m_scripts[index];
    }
    return nullptr;
}

inline const LispScript* LispScriptManager::getScript(int index) const {
    if (index >= 0 && index < m_scripts.size()) {
        return &m_scripts[index];
    }
    return nullptr;
}

inline ProcessingOptions::ProcessingOptions() {
    initializeDefaults();
}

inline bool ProcessingOptions::isEmpty() const {
    return !enableTextReplacement && 
           !enableAttributeReplacement && 
           !enableLayerOperations && 
           !enableLispExecution;
}

// =============================================================================
// Global Utility Functions
// =============================================================================

namespace ProcessingUtils {
    // File validation utilities
    bool isValidDwgFile(const QString& filePath);
    bool isValidLispFile(const QString& filePath);
    QString formatFileSize(qint64 bytes);
    QString formatElapsedTime(qint64 milliseconds);
    
    // Configuration utilities
    QString generateConfigBackupName();
    bool backupConfiguration(const ProcessingOptions& options);
    QStringList findRecentConfigurations();
    
    // Validation utilities
    QStringList validateProcessingOptions(const ProcessingOptions& options);
    QString getDetailedValidationReport(const ProcessingOptions& options);
}

#endif // PROCESSINGOPTIONS_H
