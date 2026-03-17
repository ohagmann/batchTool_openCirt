/**
 * @file OpenCirtTab.h
 * @brief OpenCirt Tab - GA-Automation for BricsCAD BatchProcessing Plugin
 * @version 2.0.0
 * 
 * Implements the OpenCirt tab with 5 functions:
 * 1. BMK-Nummerierung generieren
 * 2. BAS-Generierung
 * 3. GA-Funktionslisten generieren
 * 4. Textbreitenanpassung
 * 5. Gesamtprojekt generieren
 * 
 * Architecture: C++ Orchestrator generates SCR files, LISP scripts
 * handle in-drawing operations. Executed via _.SCRIPT in current
 * BricsCAD instance (same as LISP tab).
 * 
 * Reference: OpenCirt_Tab_TechnicalSpec_v1.1
 */

#ifndef OPENCIRTTAB_H
#define OPENCIRTTAB_H

#include <QWidget>
#include <QString>
#include <QStringList>
#include <QMap>
#include <QVector>
#include <QJsonObject>
#include <memory>

QT_BEGIN_NAMESPACE
class QCheckBox;
class QPushButton;
class QTextEdit;
class QLabel;
class QGroupBox;
class QProgressBar;
class QTimer;
QT_END_NAMESPACE

namespace BatchProcessing {

// ============================================================================
// Phase State
// ============================================================================

/**
 * @brief GA-FL generation phase tracking
 * 
 * Since _.SCRIPT runs asynchronously, Phase 2 (creation/fill) can only
 * start after Phase 1 (extraction) has completed. The user must click
 * the GA-FL button twice, or use the Full Project flow.
 */
enum class GaFlPhase {
    Idle,           ///< No GA-FL operation in progress
    Phase1Done      ///< Extraction complete, ready for Phase 2
};

// ============================================================================
// Data Structures
// ============================================================================

/**
 * @brief Extracted datapoint from a source DWG (Phase 1 output)
 */
struct DataPoint {
    QString bmk;                    ///< OC_BMK (e.g. "BSK01")
    QString bezeichnung;            ///< OC_BEZEICHNUNG (e.g. "ZUL-Ventilator")
    QString aks;                    ///< OC_AKS (e.g. "H01.HZG")
    QString basString;              ///< OC_BAS_DP_n (generated BAS)
    QString refDp;                  ///< OC_REF_DP_n (reference name for ODS lookup)
    QString integDp;                ///< OC_INTEG_DP_n (integration type)
    QString fcodeDp;                ///< OC_FCODE_DP_n
    int dpIndex = 0;                ///< Datenpunkt-Index (1..25)
    QMap<QString, QString> funktionsWerte; ///< OC_x_x_x_DP_n values (60 columns)
};

/**
 * @brief Info about a source DWG with active datapoints
 */
struct SourceDrawingInfo {
    QString filePath;               ///< Full path to source DWG
    QString fileName;               ///< Filename without extension
    QString aspName;                ///< ASP name (from folder structure)
    QString aspFolder;              ///< ASP folder path
    QString gewerk;                 ///< Gewerk (from folder, e.g. "RLT")
    QString anlage;                 ///< Anlage (from folder, e.g. "TKA-1030")
    QVector<DataPoint> dataPoints;  ///< Active datapoints
    int gaFlSheetCount = 0;         ///< Number of GA-FL sheets needed
    
    // Plankopf attributes (carried over to GA-FL)
    QMap<QString, QString> plankopfAttributes;
};

/**
 * @brief Project configuration stored in opencirt_config.json
 */
struct OpenCirtConfig {
    bool includeBmk = true;
    bool includeBas = true;
    QString lastProjectRoot;
    
    // Paths (relative to project root)
    static constexpr const char* REFERENZEN_DIR = "01- Referenzen";
    static constexpr const char* SKRIPTE_DIR = "02- Skripte";
    static constexpr const char* VORLAGEN_DIR = "04- Vorlagen";
    static constexpr const char* ZEICHNUNGEN_DIR = "05- Projekt Zeichnungen";
    
    // Fixed filenames
    static constexpr const char* BAS_CSV = "BAS.csv";
    static constexpr const char* GA_FL_VORLAGE_ODS = "GA_FL_VORLAGE.ods";
    static constexpr const char* GA_FL_VORLAGE_DWG = "OC_VORLAGE_GA_FL.dwg";
    static constexpr const char* CONFIG_FILE = "opencirt_config.json";
    static constexpr const char* PLANKOPF_CSV = "plankopfdaten.csv";
    
    // GA-FL Block name in template
    static constexpr const char* GA_FL_BLOCK_NAME = "VDI3814_GA_FL_V_1_0";
    
    // Max datapoints per sheet
    static constexpr int MAX_DP_FIRST_SHEET = 25;  // Zeilen 1-25, kein Uebertrag
    static constexpr int MAX_DP_FOLLOW_SHEET = 24;  // Zeile 1 = Uebertrag, Zeilen 2-25 = DPs
    static constexpr int SUMME_ROW = 26;            // Summenzeile (OC_SUM_1..58)
    
    bool loadFromFile(const QString& projectRoot);
    bool saveToFile(const QString& projectRoot) const;
};

// ============================================================================
// OpenCirt Tab Widget
// ============================================================================

class OpenCirtTab : public QWidget {
    Q_OBJECT

public:
    explicit OpenCirtTab(QWidget* parent = nullptr);
    ~OpenCirtTab() = default;
    
    /// Set the project root directory (from General tab)
    void setProjectRoot(const QString& root);
    QString projectRoot() const { return m_projectRoot; }
    
    /// Load/save configuration
    void loadConfig();
    void saveConfig();
    
    /// Check if OpenCirt functions are enabled
    bool isEnabled() const;

signals:
    void logMessage(const QString& message, const QString& type);
    void processingStarted();
    void processingFinished(bool success, const QString& summary);
    void progressUpdated(const QString& phase, int current, int total);

private slots:
    // Button handlers
    void onPlankopfCsvGenerate();
    void onDeckblattGenerate();
    void onBmkGenerate();
    void onBasGenerate();
    void onGaFlGenerate();
    void onTextwidthAdjust();
    void onFullProjectGenerate();
    
    void onEnableToggled(bool enabled);
    void onPublishPdf();
    
    /// Timer callback: poll for Phase 1 completion marker
    void onPhase1PollTimer();

private:
    void setupUi();
    void updateButtonStates();
    
    // ================================================================
    // Core Orchestration Methods
    // ================================================================
    
    /// Read plankopfdaten.csv from Referenzen folder (dynamic key-value pairs)
    QMap<QString, QString> readPlankopfCsv();
    
    /// Generate SCR to write plankopfdaten.csv attributes into all project DWGs
    QString generatePlankopfCsvScr(const QStringList& dwgFiles);
    
    /// Generate LISP snippet to set arbitrary attributes in all Plankopf blocks
    QString generateSetPlankopfSnippet(const QMap<QString, QString>& attrs);
    
    /// Validate project structure (check required folders/files)
    bool validateProjectStructure(QStringList& errors);
    
    /// Find all DWGs in project drawings folder (recursive)
    QStringList findProjectDwgs();
    
    /// Detect ASP from folder path (looks for "ASP" or "ISP" in folder name)
    QString detectAspFromPath(const QString& dwgPath);
    
    /// Convert ODS to CSV using LibreOffice or Excel
    bool convertOdsToCSV(const QString& odsPath, const QString& csvPath);
    
    /// Find LibreOffice executable
    QString findLibreOffice();
    
    /// Find Microsoft Excel executable
    QString findExcel();
    
    // ================================================================
    // Phase 1: BMK Nummerierung
    // ================================================================
    
    /// Generate SCR for BMK numbering across all project DWGs
    QString generateBmkScr(const QStringList& dwgFiles);
    
    // ================================================================
    // Phase 2: BAS Generation
    // ================================================================
    
    /// Parse BAS.csv definition file
    struct BasSegment {
        bool isStatic;          ///< true = literal string, false = attribute name
        QString value;          ///< The static text or attribute name
        bool isDpSuffix;        ///< Attribute ends with _DP (needs _n appended)
    };
    QVector<BasSegment> parseBasCsv(const QString& csvPath);
    
    /// Generate SCR for BAS generation
    QString generateBasScr(const QStringList& dwgFiles);
    
    // ================================================================
    // Phase 3: GA-FL Generation (Two-Phase)
    // ================================================================
    
    // ================================================================
    // Phase 0: Deckblatt Generation
    // ================================================================
    
    /// Find the DIN A2 template (ignoring version number)
    QString findDeckblattVorlage();
    
    /// Cleanup existing Deckblatt files
    int cleanupDeckblaetter();
    
    /// Cleanup temp/backup files from project (*.bak, *.dwl, etc.)
    void onProjektBereinigen();
    
    /// Generate SCR for Deckblatt creation
    QString generateDeckblattScr();
    
    /// Generate LISP snippet to set ASP attribute in all Plankopf blocks
    QString generateSetAspSnippet(const QString& aspValue);
    
    /// Generate SCR to write ASP from folder hierarchy into Plankopf of each source DWG
    QString generatePlankopfAspScr(const QStringList& dwgFiles);
    
    /// Extract display name from folder name (strip leading digits+space)
    static QString folderDisplayName(const QString& folderName);
    
    /// Phase 0: Cleanup - delete existing GA-FL and summary sheets
    bool cleanupGaFl();
    
    /// Phase 1: Generate extraction SCR (extract DPs from source DWGs)
    QString generateExtractionScr(const QStringList& dwgFiles);
    
    /// Intermediate: Read extracted CSVs and plan GA-FL generation
    QVector<SourceDrawingInfo> readExtractedData(const QStringList& dwgFiles);
    
    /// Read ODS reference data (column mapping)
    /// Returns map: DP-Name -> row data (all columns from CSV)
    QMap<QString, QVector<QString>> readOdsReference();
    
    /// Parse a single extracted CSV file into SourceDrawingInfo
    SourceDrawingInfo parseExtractedCsv(const QString& csvPath, const QString& dwgPath);
    
    /// Calculate number of GA-FL sheets needed for N datapoints
    static int calculateSheetCount(int dpCount);
    
    /// Phase 2: Generate creation/fill SCR for GA-FL sheets
    QString generateGaFlCreationScr(const QVector<SourceDrawingInfo>& drawings);
    
    /// Generate summary sheets SCR
    QString generateSummarySheetScr(const QVector<SourceDrawingInfo>& drawings);
    
    // ================================================================
    // Phase 4: Text Width Adjustment
    // ================================================================
    
    /// Find all GA-FL and summary DWGs, generate SCR for text width adjustment
    QString generateTextwidthScr();
    
    // ================================================================
    // Phase 5: Inhaltsverzeichnis + PDF Publish
    // ================================================================
    
    /// Single TOC entry (one line in Inhaltsverzeichnis)
    struct TocEntry {
        QString los;            ///< Los name (filled only on change)
        QString asp;            ///< ASP name (filled only on change)
        QString gewerk;         ///< Gewerk name (filled only on change)
        QString anlage;         ///< Anlage identifier (e.g. TKA1010)
        QString zeichnungsNr;   ///< ZEICHNUNGSNUMMER or filename fallback
        int seite = 0;          ///< Page number in final PDF
    };
    
    /// Collect all DWGs in correct order for publishing
    QStringList collectOrderedDwgsForPublish();
    
    /// Generate DSD file for PUBLISH command
    QString generateDsdFile(const QStringList& orderedDwgs);
    
    /// Find the Inhalt block template DWG
    QString findInhaltBlockVorlage();
    
    /// Delete existing Inhalt DWG files
    int cleanupInhalt();
    
    /// Build TOC entries from ordered DWG list
    QVector<TocEntry> buildTocEntries(const QStringList& orderedDwgs, int tocPageCount);
    
    /// Generate SCR for Inhaltsverzeichnis DWG creation
    QString generateInhaltScr(const QVector<TocEntry>& entries, int tocPageCount);
    
    /// Timer callback: poll for Inhalt SCR completion, then publish
    void onPublishPollTimer();
    
    /// Timer callback: poll for PDF publish completion marker
    void onPdfDonePollTimer();
    
    /// Launch BricsCAD PDF publish with DSD file
    void launchPublish(const QStringList& orderedDwgs);
    

    // ================================================================
    // SCR Execution
    // ================================================================
    
    /// Execute a generated SCR file via _.SCRIPT in current BricsCAD instance
    bool executeScrFile(const QString& scrContent, const QString& description);
    
    /// Write SCR content to temp file
    QString writeScrToTempFile(const QString& content, const QString& name);
    
    /// Platform-specific LISP for save (VLA on Windows, command on Linux)
    QString lispSave();
    /// Platform-specific LISP for close (VLA on Windows, command on Linux)
    QString lispClose();
    
    // ================================================================
    // Helpers
    // ================================================================
    
    /// Get full path to a project subfolder
    QString projectPath(const char* subfolder) const;
    
    /// Get full path to a reference file
    QString referencePath(const char* filename) const;
    
    /// Get full path to a template file
    QString templatePath(const char* filename) const;
    
    /// Get the LISP scripts directory
    QString scriptsPath() const;
    
    /// Log to the embedded log widget
    void log(const QString& message, const QString& type = "INFO");
    void logError(const QString& message);
    void logSuccess(const QString& message);
    
    /// Helper: check if value represents "active" (ja, true, 1, x, high, aktiv, wahr)
    static bool isActiveValue(const QString& value);
    
    // ================================================================
    // UI Members
    // ================================================================
    QCheckBox* m_enableCheck;
    QLabel* m_statusLabel;
    
    QPushButton* m_btnPlankopfCsv;
    QPushButton* m_btnDeckblatt;
    QPushButton* m_btnBmk;
    QPushButton* m_btnBas;
    QPushButton* m_btnGaFl;
    QPushButton* m_btnTextwidth;
    QPushButton* m_btnFullProject;
    QPushButton* m_btnPublish;
    QPushButton* m_btnBereinigen;
    
    QCheckBox* m_chkIncludeBmk;
    QCheckBox* m_chkIncludeBas;
    
    QTextEdit* m_logWidget;
    QProgressBar* m_progressBar;
    
    // ================================================================
    // State
    // ================================================================
    QString m_projectRoot;
    OpenCirtConfig m_config;
    
    // Plankopf CSV data (cached during project generation)
    QMap<QString, QString> m_plankopfCsvData;
    
    // GA-FL phase tracking
    GaFlPhase m_gaFlPhase = GaFlPhase::Idle;
    QString m_extractTempDir;   ///< Temp dir for extracted CSVs
    
    // Phase 1 completion polling
    QTimer* m_phase1Timer = nullptr;    ///< Polls for marker file
    QString m_phase1MarkerPath;         ///< Path to completion marker
    bool m_fullProjectMode = false;     ///< True when running Gesamtprojekt
    
    // Publish: Inhalt generation -> PDF publish polling
    QTimer* m_publishTimer = nullptr;   ///< Polls for Inhalt SCR completion
    QString m_publishMarkerPath;        ///< Path to Inhalt completion marker
    QStringList m_pendingPublishDwgs;   ///< DWGs to publish after Inhalt is ready
    
    // PDF publish completion polling (via /b batch instance + marker file)
    QTimer* m_pdfDoneTimer = nullptr;   ///< Polls for publish-done marker
    QString m_pdfDoneMarkerPath;        ///< Path to completion marker written by SCR
    QString m_publishedPdfPath;         ///< Path to the output PDF (from DSD, for success message)

};

} // namespace BatchProcessing

#endif // OPENCIRTTAB_H
