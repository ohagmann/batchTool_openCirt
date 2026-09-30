/**
 * @file OpenCirtTab.h
 * @brief openCirt Tab - GA-Automation for BricsCAD BatchProcessing Plugin
 * @version 3.0.0
 * 
 * Der Tab steuert den Ablauf: Projekt aufbauen, Projekt erstellen
 * (Plankopf, Deckblaetter, BMK, BAS, Extraktion, GA-FL, Summen, Textbreiten),
 * Projekt bereinigen, PDF publizieren, IO-Liste, Sensorliste.
 * 
 * Architektur: Die Zeichnungen werden als Side-Database bearbeitet
 * (core/OpenCirtEngine, core/ProjectBuilder) - ohne Editor, ohne LISP, unter
 * Windows und Linux gleich. Bis Version 1.6 erzeugte der Tab dafuer Skripte
 * mit LISP, die BricsCAD im Editor ausfuehrte. Nur der PDF-Publish braucht
 * weiterhin BricsCAD selbst und laeuft in einer eigenen Batch-Instanz.
 * 
 * Reference: OpenCirt_Tab_TechnicalSpec_v1.1
 */

#ifndef OPENCIRTTAB_H
#define OPENCIRTTAB_H

#include <QWidget>
#include <QString>
#include <QStringList>
#include <QMap>
#include <QDateTime>
#include <QVector>
#include <QJsonObject>
#include <QProcessEnvironment>
#include <functional>
#include <memory>

QT_BEGIN_NAMESPACE
class QCheckBox;
class QPushButton;
class QLabel;
class QGroupBox;
class QProgressBar;
class QTimer;
QT_END_NAMESPACE

namespace BatchProcessing {

class OcDrawing;

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
    QString produkt;                ///< OC_PRODUKT (Block-Ebene, fuer Sensorliste)
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
    
    // Inhaltsverzeichnis: rows the entry block brings along, and the wildcard
    // used to locate that block inside the sheet template (version-agnostic).
    static constexpr int INHALT_ROWS_PER_PAGE = 22;
    static constexpr const char* INHALT_BLOCK_PATTERN = "OC_VORLAGE_EINTRAG_INHALT_DIN_A2*";
    
    // Max datapoints per sheet
    static constexpr int MAX_DP_FIRST_SHEET = 25;  // Zeilen 1-25, kein Uebertrag
    static constexpr int MAX_DP_FOLLOW_SHEET = 24;  // Zeile 1 = Uebertrag, Zeilen 2-25 = DPs
    static constexpr int SUMME_ROW = 26;            // Summenzeile (OC_SUM_1..58)
    
    bool loadFromFile(const QString& projectRoot);
    bool saveToFile(const QString& projectRoot) const;
};

// ============================================================================
// openCirt Tab Widget
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
    
    /// Check if openCirt functions are enabled
    bool isEnabled() const;

signals:
    void logMessage(const QString& message, const QString& type);

    /// Kurzlebiger Fortschritt fuer die Statuszeile. Gehoert bewusst nicht ins
    /// Protokoll, weil dort nur Ergebnisse stehen sollen. Ein leerer Text
    /// bedeutet "fertig, Statuszeile zuruecksetzen".
    void statusMessage(const QString& text);

    void processingStarted();
    void processingFinished(bool success, const QString& summary);
    void progressUpdated(const QString& phase, int current, int total);

private slots:
    // Button handlers
    void onFullProjectGenerate();

    void onEnableToggled(bool enabled);
    void onPublishPdf();
    void onSensorListeGenerate();
    void onDatenpunktExport();

private:
    void setupUi();
    void updateButtonStates();

    // ================================================================
    // Laeufe ohne Editor
    // ================================================================

    /// Zaehler eines Laufs
    struct RunCounts {
        int errors = 0;         ///< Zeichnungen, die sich nicht lesen oder speichern liessen
        int deckblaetter = 0;
        int bmk = 0;            ///< vergebene Betriebsmittelkennzeichen
        int bas = 0;            ///< geschriebene BAS-Strings
        int datenpunkte = 0;
        int gaFl = 0;           ///< GA-FL-Blaetter
        int summen = 0;         ///< Summenblaetter
        int textbreiten = 0;    ///< angepasste Texte
    };

    /// Summenblatt, das der Gesamtlauf erzeugt
    struct SummaryJob {
        QString target;         ///< Zieldatei
        QString csvPath;        ///< CSV mit den Zeilen der Summenebene
        int sheetNum = 1;
        int startRow = 0;
        int dpCount = 0;
    };

    /// false, wenn Zeichnungen des Projekts im Editor geoeffnet sind; zeigt
    /// dann die Liste
    bool checkNoOpenDrawings(const QString& title);

    /// Lauf beginnen: Schaltflaechen sperren, Fortschritt zeigen
    void beginRun(QPushButton* button, const QString& busyText);
    /// Neuer Abschnitt des Laufs mit eigener Zaehlung
    void beginPhase(const QString& label, int total);
    /// Eine Zeichnung des Abschnitts ist fertig
    void stepDone(const QString& fileName);
    void endRun();

    /// Zeichnung lesen, bearbeiten, speichern. Fehler stehen im Protokoll
    /// und in counts.errors.
    bool processDrawing(const QString& path, RunCounts& counts,
                        const std::function<void(OcDrawing&)>& work,
                        bool backup = true);

    /// Wie processDrawing() fuer ein Blatt, das der Lauf eben aus einer
    /// Vorlage kopiert hat: ohne Sicherungskopie
    bool processNewDrawing(const QString& path, RunCounts& counts,
                           const std::function<void(OcDrawing&)>& work);

    /// Zaehlerdateien der BMK-Nummerierung (bmk_counters.tmp) aus dem
    /// Zeichnungsordner entfernen. Rueckgabe: Anzahl.
    int removeBmkCounters();

    /// Schritte des Gesamtlaufs. false bei Abbruch.
    bool runGesamtlauf(const QStringList& dwgFiles, RunCounts& counts);

    /// REF_DP der Datenpunkte gegen die Referenz pruefen; bei fehlenden
    /// Referenzen fragen, ob der Lauf weitergehen soll
    bool confirmMissingReferences(const QVector<SourceDrawingInfo>& drawings);

    // ================================================================
    // Core Orchestration Methods
    // ================================================================
    
    /// Read plankopfdaten.csv from Referenzen folder (dynamic key-value pairs)
    QMap<QString, QString> readPlankopfCsv();
    
    /// Validate project structure (check required folders/files)
    bool validateProjectStructure(QStringList& errors);
    
    /// Find all source DWGs in project drawings folder (recursive). Skips the
    /// top level (project sheets, see findProjektblaetter) and generated sheets
    QStringList findProjectDwgs();

    /// Projektblaetter: DWGs directly on the top level of '05- Projekt
    /// Zeichnungen' (Deckblatt_A, Revisionshistorie). They receive the
    /// Plankopf master data but are not source drawings
    QStringList findProjektblaetter();
    
    /// Detect ASP from folder path (looks for "ASP" or "ISP" in folder name)
    QString detectAspFromPath(const QString& dwgPath);
    
    /// Convert ODS to CSV using LibreOffice or Excel
    bool convertOdsToCSV(const QString& odsPath, const QString& csvPath);
    
    /// Find LibreOffice executable
    QString findLibreOffice();

    /// Umgebung fuer fremde Programme: ohne das Programmverzeichnis von
    /// BricsCAD im Bibliothekspfad
    static QProcessEnvironment externalToolEnvironment();

    /// Find Microsoft Excel executable
    QString findExcel();
    
    // ================================================================
    // Phase 0: Deckblatt Generation
    // ================================================================
    
    /// Find the DIN A2 template (ignoring version number)
    QString findDeckblattVorlage();
    
    /// Cleanup existing Deckblatt files
    int cleanupDeckblaetter();
    
    /// Cleanup temp/backup files from project (*.bak, *.dwl, etc.)
    void onProjektBereinigen();

    /// Quellzeichnungen aus der Erstellliste (CSV) aufbauen: Vorlagen
    /// kopieren, einsortieren, Attribute setzen. Wahlweise nur als Vorschau.
    /// Ersetzt das LISP-Skript OC_PROJECT_BUILD (siehe core/ProjectBuilder).
    void onProjektAufbauen();
    
    /// Je Ordner unter dem Zeichnungsordner ein Deckblatt erzeugen
    void createDeckblaetter(RunCounts& counts);

    /// Derive ASP/GEWERK/ANLAGE from a folder below the drawings root
    void deriveHierarchieFromFolder(const QString& folderPath,
                                    QString& asp, QString& gewerk, QString& anlage);

    /// plankopfdaten.csv des aktuellen Projekts lesen
    void loadPlankopfCsv();

    /// m_plankopfCsvData auf den Stand der plankopfdaten.csv des aktuellen
    /// Projekts bringen; liest neu, wenn Projekt oder Datei sich geaendert haben
    void ensurePlankopfCsvLoaded();

    /// Extract display name from folder name (strip leading digits+space)
    static QString folderDisplayName(const QString& folderName);
    
    /// Phase 0: Cleanup - delete existing GA-FL and summary sheets
    bool cleanupGaFl();
    
    /// Intermediate: Read extracted CSVs and plan GA-FL generation
    QVector<SourceDrawingInfo> readExtractedData(const QStringList& dwgFiles);

    /// Ordner der Extraktionsdaten des geladenen Projekts:
    /// <Temp>/OpenCirt_extract/<Projektname>_<Kennung aus dem Pfad>[_<Zweck>]
    /// Der Gesamtlauf (Zweck leer) und die Listen haben je ihren eigenen,
    /// damit eine Liste die Protokolle des Gesamtlaufs nicht wegraeumt.
    QString projectExtractDir(const QString& purpose) const;

    /// Ordner der Extraktionsdaten fuer den laufenden Lauf leeren und neu
    /// anlegen. false: alte Daten liessen sich nicht entfernen - dann darf
    /// nichts daraus gelesen werden.
    bool resetExtractDir(const QString& purpose = QString());

    /// Der Ordner wurde vom laufenden Lauf fuer das geladene Projekt gefuellt
    bool extractDirUsable() const;

    /// Datenpunkte aller Quellzeichnungen neu extrahieren, ohne die
    /// Zeichnungen zu aendern. unreadable: Zeichnungen, die sich nicht lesen
    /// liessen. false: der Ordner liess sich nicht vorbereiten.
    bool extractDrawings(const QStringList& dwgFiles, const QString& purpose,
                         QStringList& unreadable);
    
    /// Read ODS reference data (column mapping)
    /// Returns map: DP-Name -> row data (all columns from CSV)
    QMap<QString, QVector<QString>> readOdsReference();
    
    /// Parse a single extracted CSV file into SourceDrawingInfo
    SourceDrawingInfo parseExtractedCsv(const QString& csvPath, const QString& dwgPath);
    
    /// Calculate number of GA-FL sheets needed for N datapoints
    static int calculateSheetCount(int dpCount);
    
    /// Phase 2: GA-FL-Blaetter erzeugen und befuellen. extracted traegt je
    /// Quellzeichnung die Zeilen ihrer extrahierten CSV.
    void createGaFlSheets(const QVector<SourceDrawingInfo>& drawings,
                          const QMap<QString, QStringList>& extracted,
                          const QString& refCsvPath, RunCounts& counts);

    /// Phase 3: Summen-CSVs schreiben und die Summenblaetter planen
    /// (m_summaryJobs, m_plannedSummarySheets)
    void planSummarySheets(const QVector<SourceDrawingInfo>& drawings);

    /// Phase 3: GA-FL-Blaetter lesen, Summenblaetter erzeugen, Textbreiten
    /// aller GA-FL- und Summenblaetter anpassen
    void createSummen(RunCounts& counts);

    /// Phase 3: die fertigen GA-FL-Blaetter als Datenquelle fuer die Summen.
    /// Ein Eintrag je Blatt; Funktionswerte je Datenpunkt aus den Blattzellen
    /// (DataPoint::funktionsWerte, Schluessel = Funktionsbasis). sheetPaths
    /// erhaelt die gelesenen Blaetter fuer die Textbreitenanpassung.
    QVector<SourceDrawingInfo> readGaFlSheetsForSummary(QStringList& sheetPaths);

    /// ASP/Gewerk/Anlage aus dem Ablageort einer Zeichnung ableiten
    void applyFolderHierarchie(SourceDrawingInfo& info, const QString& dwgPath);

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
    
    /// Find the Inhaltsverzeichnis sheet template DWG (22 rows preplaced)
    QString findInhaltVorlage();
    
    /// Delete existing Inhalt DWG files
    int cleanupInhalt();
    
    /// Build TOC entries from ordered DWG list
    QVector<TocEntry> buildTocEntries(const QStringList& orderedDwgs, int tocPageCount);
    
    /// Seiten des Inhaltsverzeichnisses erzeugen. Rueckgabe: Anzahl der
    /// Seiten, -1 wenn die Vorlage fehlt.
    int createInhaltPages(const QVector<TocEntry>& entries, int tocPageCount);
    
    /// Timer callback: poll for PDF publish completion marker
    void onPdfDonePollTimer();
    
    /// Launch BricsCAD PDF publish with DSD file
    void launchPublish(const QStringList& orderedDwgs);
    

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
    
    QPushButton* m_btnProjektAufbau;
    QPushButton* m_btnFullProject;
    QPushButton* m_btnPublish;
    QPushButton* m_btnSensorliste;
    QPushButton* m_btnDpExport;
    QPushButton* m_btnBereinigen;
    
    QCheckBox* m_chkIncludeBmk;
    QCheckBox* m_chkIncludeBas;
    
    QProgressBar* m_progressBar;
    
    // ================================================================
    // State
    // ================================================================
    QString m_projectRoot;
    OpenCirtConfig m_config;
    
    // Plankopf CSV data, samt Datei und Stand, aus denen sie stammen
    QMap<QString, QString> m_plankopfCsvData;
    QString m_plankopfCsvPath;
    QDateTime m_plankopfCsvStamp;
    qint64 m_plankopfCsvSize = -1;
    
    /// Ordner der Extraktionsdaten. Er gehoert zu einem Projekt und einem
    /// Zweck und wird nur in dem Lauf gelesen, der ihn gefuellt hat (siehe
    /// resetExtractDir()).
    QString m_extractTempDir;
    QString m_extractPurpose;      ///< leer = Gesamtlauf, sonst z.B. "Sensorliste"
    bool m_extractFresh = false;   ///< vom laufenden Lauf geleert und gefuellt
    QString m_odsProblem;          ///< warum convertOdsToCSV gescheitert ist (fuer die Meldung)

    /// Summenblaetter, die planSummarySheets() vorgesehen hat
    QVector<SummaryJob> m_summaryJobs;

    /// Pfade dieser Summenblaetter, fuer die Textbreitenanpassung
    QStringList m_plannedSummarySheets;

    // Laufender Lauf: Sperre der Schaltflaechen und Fortschritt
    bool m_running = false;
    QPushButton* m_runButton = nullptr;     ///< Schaltflaeche, die den Lauf gestartet hat
    QString m_runButtonText;                ///< ihr Text ausserhalb des Laufs
    QString m_phaseLabel;
    int m_phaseTotal = 0;
    int m_phaseDone = 0;

    /// Last filter used in the datapoint export dialog (semicolon separated)
    QString m_dpExportFilter = "HW";
    
    // PDF publish completion polling (via /b batch instance + marker file)
    QTimer* m_pdfDoneTimer = nullptr;   ///< Polls for publish-done marker
    QString m_pdfDoneMarkerPath;        ///< Path to completion marker written by SCR
    QString m_publishedPdfPath;         ///< Path to the output PDF (from DSD, for success message)

};

} // namespace BatchProcessing

#endif // OPENCIRTTAB_H
