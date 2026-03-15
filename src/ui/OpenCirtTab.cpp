#include "windows_fix.h"  // CRITICAL: Qt 6.8+ fix - MUST be FIRST
/**
 * @file OpenCirtTab.cpp
 * @brief OpenCirt Tab Implementation - GA-Automation Orchestrator
 * @version 1.0.0
 * 
 * Reference: OpenCirt_Tab_TechnicalSpec_v1.1
 */

// BRX Platform headers
#ifdef __linux__
#include "brx_platform_linux.h"
#else
#include "brx_platform_windows.h"
#endif

// BRX API headers
#include "aced.h"
#include "AcApDMgr.h"
#include "dblayout.h"   // AcDbLayout, AcDbDatabase, AcDbDictionary for side-DB layout reading
#include "dbsymtb.h"   // AcDbBlockTable, AcDbBlockTableRecord for ModelSpace iteration
#include "dbents.h"    // AcDbBlockReference, AcDbAttribute for reading block attributes

#include "OpenCirtTab.h"

// Qt Headers
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QCheckBox>
#include <QPushButton>
#include <QTextEdit>
#include <QLabel>
#include <QProgressBar>
#include <QMessageBox>
#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QFile>
#include <QTextStream>
#include <QProcess>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDateTime>
#include <QStandardPaths>
#include <QDebug>
#include <QApplication>
#include <QRegularExpression>
#include <QTimer>
#include <QSet>

// BRX result codes
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
// OpenCirtConfig - Project Configuration
// ============================================================================

bool OpenCirtConfig::loadFromFile(const QString& projectRoot) {
    QString path = projectRoot + "/" + REFERENZEN_DIR + "/" + CONFIG_FILE;
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return false;
    }
    
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    file.close();
    
    if (doc.isNull() || !doc.isObject()) {
        return false;
    }
    
    QJsonObject obj = doc.object();
    includeBmk = obj.value("includeBmk").toBool(true);
    includeBas = obj.value("includeBas").toBool(true);
    lastProjectRoot = obj.value("lastProjectRoot").toString();
    
    return true;
}

bool OpenCirtConfig::saveToFile(const QString& projectRoot) const {
    QString dirPath = projectRoot + "/" + REFERENZEN_DIR;
    QDir dir;
    if (!dir.mkpath(dirPath)) {
        return false;
    }
    
    QString path = dirPath + "/" + CONFIG_FILE;
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        return false;
    }
    
    QJsonObject obj;
    obj["includeBmk"] = includeBmk;
    obj["includeBas"] = includeBas;
    obj["lastProjectRoot"] = lastProjectRoot;
    obj["version"] = "1.1";
    obj["lastModified"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    
    QJsonDocument doc(obj);
    file.write(doc.toJson(QJsonDocument::Indented));
    file.close();
    
    return true;
}

// ============================================================================
// Constructor & UI Setup
// ============================================================================

OpenCirtTab::OpenCirtTab(QWidget* parent)
    : QWidget(parent)
{
    setupUi();
    updateButtonStates();
    
    // Phase 1 completion polling timer
    m_phase1Timer = new QTimer(this);
    m_phase1Timer->setInterval(2000);  // Poll every 2 seconds
    connect(m_phase1Timer, &QTimer::timeout, this, &OpenCirtTab::onPhase1PollTimer);
    
    // Publish: Inhalt completion polling timer
    m_publishTimer = new QTimer(this);
    m_publishTimer->setInterval(2000);
    connect(m_publishTimer, &QTimer::timeout, this, &OpenCirtTab::onPublishPollTimer);

}

void OpenCirtTab::setupUi() {
    auto* mainLayout = new QVBoxLayout(this);
    
    // --- Enable Checkbox + Status ---
    auto* topLayout = new QHBoxLayout();
    m_enableCheck = new QCheckBox("OpenCirt-Funktionen aktivieren");
    m_enableCheck->setToolTip("Aktiviert die OpenCirt GA-Automatisierungsfunktionen");
    topLayout->addWidget(m_enableCheck);
    
    m_statusLabel = new QLabel("Kein Projekt geladen");
    m_statusLabel->setStyleSheet("QLabel { color: #888; font-style: italic; }");
    topLayout->addStretch();
    topLayout->addWidget(m_statusLabel);
    mainLayout->addLayout(topLayout);
    
    // --- Function Buttons ---
    auto* buttonGroup = new QGroupBox("OpenCirt Funktionen");
    auto* buttonLayout = new QVBoxLayout(buttonGroup);
    
    // Helper lambda for button creation with description
    auto createButtonRow = [&](const QString& buttonText, const QString& description) -> QPushButton* {
        auto* rowLayout = new QHBoxLayout();
        auto* btn = new QPushButton(buttonText);
        btn->setMinimumWidth(220);
        btn->setMinimumHeight(32);
        auto* descLabel = new QLabel(description);
        descLabel->setStyleSheet("QLabel { color: #666; }");
        rowLayout->addWidget(btn);
        rowLayout->addWidget(descLabel);
        rowLayout->addStretch();
        buttonLayout->addLayout(rowLayout);
        return btn;
    };
    
    m_btnPlankopfCsv = createButtonRow(
        "Plankopf-Daten setzen",
        "Auftraggeber, Auftragnehmer, Projekt aus plankopfdaten.csv");
    
    m_btnDeckblatt = createButtonRow(
        "Deckblaetter erstellen",
        "Deckblaetter fuer Lose, ASPs, Gewerke und Anlagen");
    
    m_btnBmk = createButtonRow(
        "BMK erstellen",
        "Betriebsmittelkennzeichnungen nummerieren");
    
    m_btnBas = createButtonRow(
        "BAS erstellen",
        "Benutzeradressierungssystem (BAS) aus BAS.csv zusammenbauen");
    
    m_btnGaFl = createButtonRow(
        "GA-FL erstellen",
        "Funktionslisten erzeugen und befuellen");
    
    m_btnTextwidth = createButtonRow(
        "Textbreiten anpassen",
        "Textbreiten in GA-FL-Dateien korrigieren");
    
    m_btnPublish = createButtonRow(
        "PDF publizieren",
        "Alle Zeichnungen als Multi-Sheet PDF mit Inhaltsverzeichnis");
    
    // Separator before full project
    buttonLayout->addSpacing(8);
    auto* separator = new QLabel("");
    separator->setFrameStyle(QFrame::HLine | QFrame::Sunken);
    buttonLayout->addWidget(separator);
    buttonLayout->addSpacing(4);
    
    m_btnFullProject = createButtonRow(
        "Projekt erstellen",
        "Alle Schritte in korrekter Reihenfolge");
    m_btnFullProject->setStyleSheet(
        "QPushButton { font-weight: bold; }");
    
    buttonLayout->addSpacing(4);
    
    m_btnBereinigen = createButtonRow(
        "Projekt bereinigen",
        "Temporaere Dateien und Backups loeschen");
    
    mainLayout->addWidget(buttonGroup);
    
    // --- Options for Full Project ---
    auto* optionsGroup = new QGroupBox("Optionen (Gesamtlauf)");
    auto* optionsLayout = new QVBoxLayout(optionsGroup);
    
    m_chkIncludeBmk = new QCheckBox("BMK-Nummerierung einschliessen");
    m_chkIncludeBmk->setChecked(true);
    m_chkIncludeBmk->setToolTip(
        "BMK-Nummerierung vor GA-FL-Generierung ausfuehren.\n"
        "Steuerung pro DWG ueber Plankopf-Attribut FREITEXT_05.");
    optionsLayout->addWidget(m_chkIncludeBmk);
    
    m_chkIncludeBas = new QCheckBox("BAS-Generierung einschliessen");
    m_chkIncludeBas->setChecked(true);
    m_chkIncludeBas->setToolTip(
        "BAS-Generierung vor GA-FL-Generierung ausfuehren.\n"
        "Setzt korrekte BMK-Nummerierung voraus.");
    optionsLayout->addWidget(m_chkIncludeBas);
    
    mainLayout->addWidget(optionsGroup);
    
    // --- Progress Bar ---
    m_progressBar = new QProgressBar();
    m_progressBar->setVisible(false);
    m_progressBar->setTextVisible(true);
    mainLayout->addWidget(m_progressBar);
    
    // --- Log Output ---
    auto* logGroup = new QGroupBox("Protokoll");
    auto* logLayout = new QVBoxLayout(logGroup);
    
    m_logWidget = new QTextEdit();
    m_logWidget->setReadOnly(true);
    m_logWidget->setMaximumHeight(180);
    m_logWidget->setFont(QFont("Consolas", 8));
    m_logWidget->setStyleSheet(
        "QTextEdit { background-color: #1e1e1e; color: #d4d4d4; }");
    logLayout->addWidget(m_logWidget);
    
    mainLayout->addWidget(logGroup);
    
    // --- Connections ---
    connect(m_enableCheck, &QCheckBox::toggled, this, &OpenCirtTab::onEnableToggled);
    connect(m_btnPlankopfCsv, &QPushButton::clicked, this, &OpenCirtTab::onPlankopfCsvGenerate);
    connect(m_btnDeckblatt, &QPushButton::clicked, this, &OpenCirtTab::onDeckblattGenerate);
    connect(m_btnBmk, &QPushButton::clicked, this, &OpenCirtTab::onBmkGenerate);
    connect(m_btnBas, &QPushButton::clicked, this, &OpenCirtTab::onBasGenerate);
    connect(m_btnGaFl, &QPushButton::clicked, this, &OpenCirtTab::onGaFlGenerate);
    connect(m_btnTextwidth, &QPushButton::clicked, this, &OpenCirtTab::onTextwidthAdjust);
    connect(m_btnFullProject, &QPushButton::clicked, this, &OpenCirtTab::onFullProjectGenerate);
    connect(m_btnPublish, &QPushButton::clicked, this, &OpenCirtTab::onPublishPdf);
    connect(m_btnBereinigen, &QPushButton::clicked, this, &OpenCirtTab::onProjektBereinigen);
}

void OpenCirtTab::onEnableToggled(bool enabled) {
    m_btnPlankopfCsv->setEnabled(enabled);
    m_btnDeckblatt->setEnabled(enabled);
    m_btnBmk->setEnabled(enabled);
    m_btnBas->setEnabled(enabled);
    m_btnGaFl->setEnabled(enabled);
    m_btnTextwidth->setEnabled(enabled);
    m_btnPublish->setEnabled(enabled);
    m_btnFullProject->setEnabled(enabled);
    m_btnBereinigen->setEnabled(enabled);
    m_chkIncludeBmk->setEnabled(enabled);
    m_chkIncludeBas->setEnabled(enabled);
    
    if (enabled && !m_projectRoot.isEmpty()) {
        m_statusLabel->setText("Bereit");
        m_statusLabel->setStyleSheet("QLabel { color: #2e7d32; font-weight: bold; }");
    } else if (enabled) {
        m_statusLabel->setText("Bitte Projektordner im General-Tab angeben");
        m_statusLabel->setStyleSheet("QLabel { color: #e65100; }");
    } else {
        m_statusLabel->setText("OpenCirt deaktiviert");
        m_statusLabel->setStyleSheet("QLabel { color: #888; font-style: italic; }");
    }
}

// ============================================================================
// Phase 1 Completion Polling
// ============================================================================

void OpenCirtTab::onPhase1PollTimer() {
    if (m_phase1MarkerPath.isEmpty()) {
        m_phase1Timer->stop();
        return;
    }
    
    // Check if marker file exists (written by LISP at end of Phase 1 SCR)
    if (!QFile::exists(m_phase1MarkerPath)) {
        return;  // Still running, keep polling
    }
    
    // Phase 1 complete!
    m_phase1Timer->stop();
    QFile::remove(m_phase1MarkerPath);
    
    logSuccess("Phase 1 abgeschlossen - starte Phase 2 automatisch...");
    QApplication::processEvents();
    
    // Read extracted data
    QStringList dwgFiles = findProjectDwgs();
    QVector<SourceDrawingInfo> drawings = readExtractedData(dwgFiles);
    
    if (drawings.isEmpty()) {
        logError("Keine extrahierten Datenpunkte gefunden.");
        m_gaFlPhase = GaFlPhase::Idle;
        m_fullProjectMode = false;
        m_btnFullProject->setText("Projekt erstellen");
        m_btnGaFl->setText("GA-FL erstellen");
        m_btnFullProject->setEnabled(true);
        m_btnGaFl->setEnabled(true);
        return;
    }
    
    // === Vorab-Validierung: REF_DP gegen Referenz-CSV pruefen ===
    {
        QMap<QString, QVector<QString>> refData = readOdsReference();
        if (!refData.isEmpty()) {
            QSet<QString> missingRefs;
            QMap<QString, QStringList> missingPerDrawing;  // ref -> list of drawings
            QDir drawingsRoot(projectPath(OpenCirtConfig::ZEICHNUNGEN_DIR));
            
            for (const SourceDrawingInfo& d : drawings) {
                for (const DataPoint& dp : d.dataPoints) {
                    QString ref = dp.refDp.trimmed();
                    if (!ref.isEmpty() && !refData.contains(ref)) {
                        missingRefs.insert(ref);
                        missingPerDrawing[ref].append(drawingsRoot.relativeFilePath(d.filePath));
                    }
                }
            }
            
            if (!missingRefs.isEmpty()) {
                QString msg = QString("ACHTUNG: %1 Datenpunkt-Referenz(en) fehlen in GA_FL_VORLAGE.ods:\n\n")
                              .arg(missingRefs.size());
                for (const QString& ref : missingRefs) {
                    msg += QString("  %1  (in: %2)\n")
                           .arg(ref, missingPerDrawing[ref].join(", "));
                }
                msg += "\nBetroffene Datenpunkte werden mit 0 belegt und mit [!REF] markiert.\n"
                       "Fortfahren?";
                
                logError(QString("%1 fehlende DP-Referenz(en) in GA_FL_VORLAGE.ods").arg(missingRefs.size()));
                for (const QString& ref : missingRefs) {
                    logError(QString("  Fehlend: %1 (in: %2)")
                             .arg(ref, missingPerDrawing[ref].join(", ")));
                }
                
                QMessageBox::StandardButton reply = QMessageBox::warning(
                    this, "Fehlende DP-Referenzen", msg,
                    QMessageBox::Yes | QMessageBox::Cancel);
                
                if (reply != QMessageBox::Yes) {
                    logError("Abbruch durch Benutzer (fehlende DP-Referenzen)");
                    m_gaFlPhase = GaFlPhase::Idle;
                    m_fullProjectMode = false;
                    m_btnFullProject->setText("Projekt erstellen");
                    m_btnGaFl->setText("GA-FL erstellen");
                    m_btnFullProject->setEnabled(true);
                    m_btnGaFl->setEnabled(true);
                    return;
                }
            }
        }
    }
    
    QString combinedScr;
    
    // GA-FL sheets per source drawing
    log("GA-FL Erzeugung und Befuellung...");
    combinedScr += "; === GA-FL Phase 2: Erzeugung ===\n";
    // Reset fehlende-Referenzen-Liste fuer diesen Lauf
    combinedScr += "(progn (setq *oc-missing-refs* nil)(princ))\n";
    
    // Alte Logdatei loeschen (wird pro Dokument neu via *oc-log-path* gesetzt)
    {
        QString logFilePath = m_extractTempDir + "/fillgafl_log.txt";
        logFilePath.replace("\\", "/");
        combinedScr += QString("; === Logdatei: %1 ===\n").arg(logFilePath);
        combinedScr += QString("(progn (if (findfile \"%1\")(vl-file-delete \"%1\"))(princ))\n").arg(logFilePath);
    }
    
    combinedScr += generateGaFlCreationScr(drawings);
    
    // Summary sheets (ASP + Los + Projekt)
    log("Summenblatter erzeugen...");
    combinedScr += "; === Summenblatter ===\n";
    combinedScr += generateSummarySheetScr(drawings);
    
    // Text width adjustment (if in full project mode)
    if (m_fullProjectMode) {
        QString textLspPath = scriptsPath() + "/TextBreitenAnpassenBloecke.lsp";
        if (QFileInfo::exists(textLspPath)) {
            textLspPath.replace("\\", "/");
            combinedScr += "; === Textbreitenanpassung ===\n";
            
            for (const SourceDrawingInfo& drawing : drawings) {
                QString targetFolder = QFileInfo(drawing.filePath).absolutePath();
                targetFolder.replace("\\", "/");
                
                for (int sheet = 1; sheet <= drawing.gaFlSheetCount; ++sheet) {
                    QString gaFlName = QString("%1_GA_FL_%2.dwg")
                                       .arg(drawing.fileName)
                                       .arg(sheet, 2, 10, QChar('0'));
                    QString gaFlPath = targetFolder + "/" + gaFlName;
                    
                    combinedScr += QString("_.OPEN \"%1\"\n").arg(gaFlPath);
                    combinedScr += QString("(progn (load \"%1\")(princ))\n").arg(textLspPath);
                    combinedScr += "(progn (TextBreitenAnpassenBloecke)(princ))\n";
                    combinedScr += lispSave();
                    combinedScr += lispClose();
                }
            }
        }
    }
    
    // Fehlende DP-Referenzen per Alert melden (nach allen GA-FL-Dateien)
    combinedScr += "; === Fehlende Referenzen pruefen ===\n";
    combinedScr += "(progn (vl-catch-all-apply 'oc-fl-show-missing-refs nil)(princ))\n";
    
    // Restore system variables
    combinedScr += "; === Cleanup ===\n";
    combinedScr += "(progn (setvar \"FILEDIA\" 1)(princ))\n";
    combinedScr += "(progn (setvar \"CMDECHO\" 1)(princ))\n";
    combinedScr += "(progn (setvar \"EXPERT\" 0)(princ))\n";
    
    if (!combinedScr.trimmed().isEmpty()) {
        QString desc = m_fullProjectMode ? "Projekt Phase 2" : "GA-FL Phase 2";
        executeScrFile(combinedScr, desc);
        logSuccess("Phase 2 gestartet - BricsCAD verarbeitet alle Schritte.");
    }
    
    // Reset state
    m_gaFlPhase = GaFlPhase::Idle;
    m_fullProjectMode = false;
    m_btnFullProject->setText("Projekt erstellen");
    m_btnGaFl->setText("GA-FL erstellen");
    m_btnFullProject->setEnabled(true);
    m_btnGaFl->setEnabled(true);
}

void OpenCirtTab::updateButtonStates() {
    bool enabled = m_enableCheck->isChecked() && !m_projectRoot.isEmpty();
    m_btnPlankopfCsv->setEnabled(enabled);
    m_btnDeckblatt->setEnabled(enabled);
    m_btnBmk->setEnabled(enabled);
    m_btnBas->setEnabled(enabled);
    m_btnGaFl->setEnabled(enabled);
    m_btnTextwidth->setEnabled(enabled);
    m_btnPublish->setEnabled(enabled);
    m_btnFullProject->setEnabled(enabled);
    m_btnBereinigen->setEnabled(enabled);
}

bool OpenCirtTab::isEnabled() const {
    return m_enableCheck->isChecked();
}

// ============================================================================
// Project Root & Configuration
// ============================================================================

void OpenCirtTab::setProjectRoot(const QString& root) {
    m_projectRoot = root;
    updateButtonStates();
    
    if (!root.isEmpty()) {
        loadConfig();
        m_statusLabel->setText(QString("Projekt: %1").arg(QDir(root).dirName()));
        m_statusLabel->setStyleSheet("QLabel { color: #2e7d32; }");
    } else {
        m_statusLabel->setText("Kein Projekt geladen");
        m_statusLabel->setStyleSheet("QLabel { color: #888; font-style: italic; }");
    }
    
    onEnableToggled(m_enableCheck->isChecked());
}

void OpenCirtTab::loadConfig() {
    if (m_projectRoot.isEmpty()) return;
    
    if (m_config.loadFromFile(m_projectRoot)) {
        m_chkIncludeBmk->setChecked(m_config.includeBmk);
        m_chkIncludeBas->setChecked(m_config.includeBas);
        log("Projektkonfiguration geladen");
    }
}

void OpenCirtTab::saveConfig() {
    if (m_projectRoot.isEmpty()) return;
    
    m_config.includeBmk = m_chkIncludeBmk->isChecked();
    m_config.includeBas = m_chkIncludeBas->isChecked();
    m_config.lastProjectRoot = m_projectRoot;
    
    m_config.saveToFile(m_projectRoot);
}

// ============================================================================
// Path Helpers
// ============================================================================

QString OpenCirtTab::projectPath(const char* subfolder) const {
    return m_projectRoot + "/" + subfolder;
}

QString OpenCirtTab::referencePath(const char* filename) const {
    return projectPath(OpenCirtConfig::REFERENZEN_DIR) + "/" + filename;
}

QString OpenCirtTab::templatePath(const char* filename) const {
    return projectPath(OpenCirtConfig::VORLAGEN_DIR) + "/" + filename;
}

QString OpenCirtTab::scriptsPath() const {
    return projectPath(OpenCirtConfig::SKRIPTE_DIR);
}

// ============================================================================
// Validation
// ============================================================================

bool OpenCirtTab::validateProjectStructure(QStringList& errors) {
    errors.clear();
    
    if (m_projectRoot.isEmpty()) {
        errors << "Kein Projektverzeichnis angegeben.";
        return false;
    }
    
    // Check required directories
    QStringList requiredDirs = {
        OpenCirtConfig::REFERENZEN_DIR,
        OpenCirtConfig::SKRIPTE_DIR,
        OpenCirtConfig::VORLAGEN_DIR,
        OpenCirtConfig::ZEICHNUNGEN_DIR
    };
    
    for (const QString& dir : requiredDirs) {
        QString fullPath = projectPath(dir.toUtf8().constData());
        if (!QDir(fullPath).exists()) {
            errors << QString("Ordner fehlt: %1").arg(dir);
        }
    }
    
    return errors.isEmpty();
}

QStringList OpenCirtTab::findProjectDwgs() {
    QStringList dwgFiles;
    QString drawingsDir = projectPath(OpenCirtConfig::ZEICHNUNGEN_DIR);
    
    QDirIterator it(drawingsDir, QStringList() << "*.dwg", QDir::Files,
                    QDirIterator::Subdirectories);
    while (it.hasNext()) {
        QString path = it.next();
        // Skip GA-FL files and summary sheets (they are generated)
        QString fileName = QFileInfo(path).fileName();
        if (fileName.contains("_GA_FL_") || fileName.contains("_Summe_") ||
            fileName.contains("_Deckblatt") || fileName.contains("_Inhalt_") ||
            fileName.startsWith("0001 Projekt_Summe", Qt::CaseInsensitive) ||
            fileName.startsWith("0000 Projekt_Deckblatt", Qt::CaseInsensitive) ||
            fileName.startsWith("0000 Projekt_Inhalt", Qt::CaseInsensitive)) {
            continue;
        }
        dwgFiles << path;
    }
    
    dwgFiles.sort(Qt::CaseInsensitive);
    return dwgFiles;
}

QString OpenCirtTab::detectAspFromPath(const QString& dwgPath) {
    // Walk up the folder hierarchy to find a folder containing "ASP" or "ISP"
    QDir dir(QFileInfo(dwgPath).absolutePath());
    QString drawingsRoot = projectPath(OpenCirtConfig::ZEICHNUNGEN_DIR);
    
    while (dir.absolutePath().length() > drawingsRoot.length()) {
        QString dirName = dir.dirName();
        if (dirName.contains("ASP", Qt::CaseInsensitive) ||
            dirName.contains("ISP", Qt::CaseInsensitive)) {
            return dirName;
        }
        dir.cdUp();
    }
    
    return QString(); // No ASP found
}

bool OpenCirtTab::isActiveValue(const QString& value) {
    QString v = value.trimmed().toLower();
    return (v == "ja" || v == "true" || v == "1" || v == "x" ||
            v == "high" || v == "aktiv" || v == "wahr");
}

// ============================================================================
// ODS to CSV Conversion
// ============================================================================

QString OpenCirtTab::findLibreOffice() {
#ifdef _WIN32
    // Common Windows paths
    QStringList paths = {
        "C:/Program Files/LibreOffice/program/soffice.exe",
        "C:/Program Files (x86)/LibreOffice/program/soffice.exe"
    };
    for (const QString& p : paths) {
        if (QFileInfo::exists(p)) return p;
    }
    
    // Try PATH
    QProcess proc;
    proc.start("where", QStringList() << "soffice.exe");
    proc.waitForFinished(5000);
    QString output = proc.readAllStandardOutput().trimmed();
    if (!output.isEmpty() && QFileInfo::exists(output.split("\n").first())) {
        return output.split("\n").first().trimmed();
    }
#else
    // Linux
    QStringList paths = {
        "/usr/bin/libreoffice",
        "/usr/bin/soffice",
        "/snap/bin/libreoffice"
    };
    for (const QString& p : paths) {
        if (QFileInfo::exists(p)) return p;
    }
#endif
    return QString();
}

QString OpenCirtTab::findExcel() {
#ifdef _WIN32
    // Common Windows paths for Excel
    QStringList paths = {
        "C:/Program Files/Microsoft Office/root/Office16/EXCEL.EXE",
        "C:/Program Files (x86)/Microsoft Office/root/Office16/EXCEL.EXE",
        "C:/Program Files/Microsoft Office/root/Office15/EXCEL.EXE"
    };
    for (const QString& p : paths) {
        if (QFileInfo::exists(p)) return p;
    }
#endif
    return QString();
}

bool OpenCirtTab::convertOdsToCSV(const QString& odsPath, const QString& csvPath) {
    // IMPORTANT: Always delete existing CSV first (no caching!)
    if (QFile::exists(csvPath)) {
        QFile::remove(csvPath);
        log(QString("Alte CSV geloescht: %1").arg(QFileInfo(csvPath).fileName()));
    }
    
    QString outDir = QFileInfo(csvPath).absolutePath();
    
    // Try LibreOffice first
    QString libreOffice = findLibreOffice();
    if (!libreOffice.isEmpty()) {
        log(QString("Konvertiere ODS mit LibreOffice: %1").arg(QFileInfo(odsPath).fileName()));
        
        QProcess proc;
        proc.setWorkingDirectory(outDir);
        QStringList args = {
            "--headless",
            "--convert-to", "csv",
            "--outdir", outDir,
            odsPath
        };
        proc.start(libreOffice, args);
        
        if (proc.waitForFinished(30000) && proc.exitCode() == 0) {
            if (QFile::exists(csvPath)) {
                logSuccess("ODS erfolgreich zu CSV konvertiert (LibreOffice)");
                return true;
            }
            // LibreOffice might name the CSV differently
            QString baseName = QFileInfo(odsPath).completeBaseName();
            QString altCsv = outDir + "/" + baseName + ".csv";
            if (QFile::exists(altCsv) && altCsv != csvPath) {
                QFile::rename(altCsv, csvPath);
                logSuccess("ODS erfolgreich zu CSV konvertiert (LibreOffice)");
                return true;
            }
        }
        logError(QString("LibreOffice-Konvertierung fehlgeschlagen: %1")
                 .arg(proc.readAllStandardError().trimmed()));
    }
    
    // Fallback: Excel via PowerShell (Windows only)
#ifdef _WIN32
    QString excel = findExcel();
    if (!excel.isEmpty()) {
        log("Fallback: Konvertiere ODS mit Excel via PowerShell...");
        
        QProcess proc;
        QString odsWin = odsPath;
        odsWin.replace("/", "\\");
        QString csvWin = csvPath;
        csvWin.replace("/", "\\");
        QString psScript = QString(
            "$excel = New-Object -ComObject Excel.Application; "
            "$excel.Visible = $false; "
            "$excel.DisplayAlerts = $false; "
            "$wb = $excel.Workbooks.Open('%1'); "
            "$wb.SaveAs('%2', 6); "  // 6 = CSV
            "$wb.Close($false); "
            "$excel.Quit()"
        ).arg(odsWin)
         .arg(csvWin);
        
        proc.start("powershell", QStringList() << "-Command" << psScript);
        if (proc.waitForFinished(60000) && QFile::exists(csvPath)) {
            logSuccess("ODS erfolgreich zu CSV konvertiert (Excel)");
            return true;
        }
        logError("Excel-Konvertierung fehlgeschlagen");
    }
#endif
    
    logError("ODS-Konvertierung fehlgeschlagen: Weder LibreOffice noch Excel gefunden");
    return false;
}

// ============================================================================
// SCR Execution
// ============================================================================

QString OpenCirtTab::writeScrToTempFile(const QString& content, const QString& name) {
    QString tempPath = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    QString dirName = QString("OpenCirt_%1").arg(QDateTime::currentMSecsSinceEpoch());
    QString fullDir = tempPath + "/" + dirName;
    
    QDir().mkpath(fullDir);
    
    QString scrPath = fullDir + "/" + name + ".scr";
    QFile file(scrPath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        logError(QString("SCR-Datei konnte nicht geschrieben werden: %1").arg(scrPath));
        return QString();
    }
    
    QTextStream stream(&file);
    stream << content;
    file.close();
    
    return scrPath;
}

bool OpenCirtTab::executeScrFile(const QString& scrContent, const QString& description) {
    QString scrPath = writeScrToTempFile(scrContent, description);
    if (scrPath.isEmpty()) return false;
    
    log(QString("Starte %1...").arg(description));
    
    // Set system variables for batch mode
    struct resbuf rb;
    rb.restype = RTSHORT;
    
    rb.resval.rint = 0;
    acedSetVar(_T("FILEDIA"), &rb);
    rb.resval.rint = 0;
    acedSetVar(_T("CMDECHO"), &rb);
    rb.resval.rint = 5;
    acedSetVar(_T("EXPERT"), &rb);
    
    // Execute _.SCRIPT
    QString path = scrPath;
    path.replace("\\", "/");
    
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
        logError(QString("_.SCRIPT fehlgeschlagen (Code: %1)").arg(rc));
        // Restore system variables
        rb.resval.rint = 1;
        acedSetVar(_T("FILEDIA"), &rb);
        rb.resval.rint = 1;
        acedSetVar(_T("CMDECHO"), &rb);
        rb.resval.rint = 0;
        acedSetVar(_T("EXPERT"), &rb);
        return false;
    }
    
    logSuccess(QString("%1 gestartet - BricsCAD verarbeitet...").arg(description));
    return true;
}

// ============================================================================
// Logging
// ============================================================================

void OpenCirtTab::log(const QString& message, const QString& type) {
    QString timestamp = QDateTime::currentDateTime().toString("hh:mm:ss");
    QString color = "#d4d4d4";
    
    if (type == "ERROR") color = "#f44747";
    else if (type == "SUCCESS") color = "#6a9955";
    else if (type == "WARN") color = "#ce9178";
    
    m_logWidget->append(
        QString("<span style='color:#858585'>[%1]</span> "
                "<span style='color:%2'>%3</span>")
        .arg(timestamp, color, message.toHtmlEscaped()));
    
    // Scroll to bottom
    QTextCursor cursor = m_logWidget->textCursor();
    cursor.movePosition(QTextCursor::End);
    m_logWidget->setTextCursor(cursor);
    
    emit logMessage(message, type);
}

// Platform-specific LISP for save+close (VLA on Windows, command on Linux)
QString OpenCirtTab::lispSave() {
#ifdef _WIN32
    return "(progn (vla-save (vla-get-ActiveDocument (vlax-get-acad-object)))(princ))\n";
#else
    return "(progn (command \"_.QSAVE\")(princ))\n";
#endif
}

QString OpenCirtTab::lispClose() {
#ifdef _WIN32
    return "(progn (vla-close (vla-get-ActiveDocument (vlax-get-acad-object)) :vlax-false)(princ))\n";
#else
    return "(progn (command \"_.CLOSE\")(princ))\n";
#endif
}

void OpenCirtTab::logError(const QString& message) {
    log(message, "ERROR");
}

void OpenCirtTab::logSuccess(const QString& message) {
    log(message, "SUCCESS");
}

// ============================================================================
// BAS.csv Parser
// ============================================================================

QVector<OpenCirtTab::BasSegment> OpenCirtTab::parseBasCsv(const QString& csvPath) {
    QVector<BasSegment> segments;
    
    QFile file(csvPath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        logError(QString("BAS.csv konnte nicht geoeffnet werden: %1").arg(csvPath));
        return segments;
    }
    
    QTextStream stream(&file);
    stream.setEncoding(QStringConverter::Utf8);
    
    while (!stream.atEnd()) {
        QString line = stream.readLine().trimmed();
        if (line.isEmpty()) continue;
        
        BasSegment seg;
        
        // Check if line is a static string (in quotes)
        if ((line.startsWith("\"") && line.endsWith("\"")) ||
            (line.startsWith("'") && line.endsWith("'"))) {
            seg.isStatic = true;
            seg.value = line.mid(1, line.length() - 2); // Remove quotes
            seg.isDpSuffix = false;
        } else {
            seg.isStatic = false;
            seg.value = line;
            // Check if attribute name ends with _DP (needs _n appended per datapoint)
            seg.isDpSuffix = line.endsWith("_DP", Qt::CaseInsensitive);
        }
        
        segments << seg;
    }
    
    file.close();
    log(QString("BAS.csv geladen: %1 Segmente").arg(segments.size()));
    return segments;
}

// ============================================================================
// Button Handlers
// ============================================================================

void OpenCirtTab::onBmkGenerate() {
    QStringList errors;
    if (!validateProjectStructure(errors)) {
        QMessageBox::warning(this, "Projektstruktur",
            "Projektstruktur unvollstaendig:\n\n" + errors.join("\n"));
        return;
    }
    
    log("=== BMK-Nummerierung starten ===");
    
    // Check for LISP script
    QString lspPath = scriptsPath() + "/BmkNummerierung.lsp";
    if (!QFileInfo::exists(lspPath)) {
        logError(QString("LISP-Skript nicht gefunden: %1").arg(lspPath));
        QMessageBox::critical(this, "Fehler",
            QString("LISP-Skript nicht gefunden:\n%1").arg(lspPath));
        return;
    }
    
    QStringList dwgFiles = findProjectDwgs();
    if (dwgFiles.isEmpty()) {
        logError("Keine Projektzeichnungen gefunden");
        return;
    }
    
    log(QString("%1 DWG-Dateien gefunden").arg(dwgFiles.size()));
    
    QString scr = generateBmkScr(dwgFiles);
    executeScrFile(scr, "BMK-Nummerierung");
}

void OpenCirtTab::onBasGenerate() {
    QStringList errors;
    if (!validateProjectStructure(errors)) {
        QMessageBox::warning(this, "Projektstruktur",
            "Projektstruktur unvollstaendig:\n\n" + errors.join("\n"));
        return;
    }
    
    // Warning about BMK dependency
    QMessageBox::StandardButton reply = QMessageBox::information(this,
        "BAS-Generierung",
        "Bitte stellen Sie sicher, dass die BMK-Nummerierung bereits "
        "ausgefuehrt wurde. Die BAS-Generierung benoetigt korrekte BMKs "
        "als Grundlage.\n\nFortfahren?",
        QMessageBox::Yes | QMessageBox::Cancel);
    
    if (reply != QMessageBox::Yes) return;
    
    log("=== BAS-Generierung starten ===");
    
    // Check for BAS.csv
    QString basCsvPath = referencePath(OpenCirtConfig::BAS_CSV);
    if (!QFileInfo::exists(basCsvPath)) {
        logError(QString("BAS.csv nicht gefunden: %1").arg(basCsvPath));
        QMessageBox::critical(this, "Fehler",
            QString("Referenzdatei fehlt:\n%1").arg(basCsvPath));
        return;
    }
    
    // Check for LISP script
    QString lspPath = scriptsPath() + "/GenBas.lsp";
    if (!QFileInfo::exists(lspPath)) {
        logError(QString("LISP-Skript nicht gefunden: %1").arg(lspPath));
        QMessageBox::critical(this, "Fehler",
            QString("LISP-Skript nicht gefunden:\n%1").arg(lspPath));
        return;
    }
    
    QStringList dwgFiles = findProjectDwgs();
    if (dwgFiles.isEmpty()) {
        logError("Keine Projektzeichnungen gefunden");
        return;
    }
    
    log(QString("%1 DWG-Dateien gefunden").arg(dwgFiles.size()));
    
    QString scr = generateBasScr(dwgFiles);
    executeScrFile(scr, "BAS-Generierung");
}

void OpenCirtTab::onGaFlGenerate() {
    QStringList errors;
    if (!validateProjectStructure(errors)) {
        QMessageBox::warning(this, "Projektstruktur",
            "Projektstruktur unvollstaendig:\n\n" + errors.join("\n"));
        return;
    }
    
    QMessageBox::StandardButton reply = QMessageBox::warning(this,
        "GA-FL erstellen",
        "Extraktion und Erzeugung der Funktionslisten.\n\n"
        "Alle bestehenden *_GA_FL_*.dwg und *_Summe*.dwg werden geloescht.\n\n"
        "Phase 2 (Erzeugung) startet automatisch nach Abschluss der Extraktion.\n\nFortfahren?",
        QMessageBox::Yes | QMessageBox::Cancel);
    
    if (reply != QMessageBox::Yes) return;
    
    log("=== GA-FL Phase 1: Extraktion ===");
    
    // Check prerequisites
    QString vorlage = templatePath(OpenCirtConfig::GA_FL_VORLAGE_DWG);
    if (!QFileInfo::exists(vorlage)) {
        logError(QString("GA-FL-Vorlage nicht gefunden: %1").arg(vorlage));
        QMessageBox::critical(this, "Fehler",
            QString("GA-FL-Vorlage nicht gefunden:\n%1").arg(vorlage));
        return;
    }
    
    QString odsPath = referencePath(OpenCirtConfig::GA_FL_VORLAGE_ODS);
    if (!QFileInfo::exists(odsPath)) {
        logError(QString("Referenzdatei fehlt: %1").arg(odsPath));
        QMessageBox::critical(this, "Fehler",
            QString("Referenzvorlage fehlt:\n%1").arg(odsPath));
        return;
    }
    
    QString extractLsp = scriptsPath() + "/ExtractDP.lsp";
    QString fillLsp = scriptsPath() + "/FillGaFl.lsp";
    
    QStringList missingScripts;
    if (!QFileInfo::exists(extractLsp)) missingScripts << "ExtractDP.lsp";
    if (!QFileInfo::exists(fillLsp)) missingScripts << "FillGaFl.lsp";
    
    if (!missingScripts.isEmpty()) {
        logError(QString("LISP-Skripte fehlen: %1").arg(missingScripts.join(", ")));
        QMessageBox::critical(this, "Fehler",
            QString("Folgende LISP-Skripte fehlen im Ordner %1:\n\n%2")
            .arg(scriptsPath(), missingScripts.join("\n")));
        return;
    }
    
    // Phase 0: Cleanup
    log("Phase 0: Cleanup...");
    if (!cleanupGaFl()) {
        logError("Cleanup fehlgeschlagen");
        return;
    }
    
    // Convert ODS to CSV (always fresh)
    QString csvPath = referencePath("GA_FL_VORLAGE.csv");
    if (!convertOdsToCSV(odsPath, csvPath)) {
        return;
    }
    
    // Phase 1: Extract datapoints from source DWGs
    QStringList dwgFiles = findProjectDwgs();
    if (dwgFiles.isEmpty()) {
        logError("Keine Projektzeichnungen gefunden");
        return;
    }
    
    // Set temp dir for extraction
    m_extractTempDir = QStandardPaths::writableLocation(QStandardPaths::TempLocation)
                       + "/OpenCirt_extract";
    // Clean old extraction data
    QDir extractDir(m_extractTempDir);
    if (extractDir.exists()) {
        extractDir.removeRecursively();
    }
    QDir().mkpath(m_extractTempDir);
    
    log(QString("Phase 1: Extraktion aus %1 DWG-Dateien...").arg(dwgFiles.size()));
    QString extractScr = generateExtractionScr(dwgFiles);
    
    // Add marker file write at end of Phase 1 SCR
    m_phase1MarkerPath = m_extractTempDir + "/phase1_complete.marker";
    QString markerPath = m_phase1MarkerPath;
    markerPath.replace("\\", "/");
    extractScr += "; --- Phase 1 completion marker ---\n";
    extractScr += QString("(progn (setq f (open \"%1\" \"w\"))(write-line \"PHASE1_COMPLETE\" f)(close f)(princ))\n")
                  .arg(markerPath);
    
    m_fullProjectMode = false;
    if (executeScrFile(extractScr, "GA-FL Phase 1 - Extraktion")) {
        m_gaFlPhase = GaFlPhase::Phase1Done;
        m_btnGaFl->setEnabled(false);
        m_btnFullProject->setEnabled(false);
        m_btnGaFl->setText("GA-FL wird erstellt...");
        m_phase1Timer->start();
        logSuccess("Phase 1 gestartet. Phase 2 startet automatisch nach Abschluss.");
    }
}

void OpenCirtTab::onTextwidthAdjust() {
    QStringList errors;
    if (!validateProjectStructure(errors)) {
        QMessageBox::warning(this, "Projektstruktur",
            "Projektstruktur unvollstaendig:\n\n" + errors.join("\n"));
        return;
    }
    
    log("=== Textbreitenanpassung starten ===");
    
    QString lspPath = scriptsPath() + "/TextBreitenAnpassenBloecke.lsp";
    if (!QFileInfo::exists(lspPath)) {
        logError(QString("LISP-Skript nicht gefunden: %1").arg(lspPath));
        return;
    }
    
    QString scr = generateTextwidthScr();
    if (scr.isEmpty()) {
        logError("Keine GA-FL-Dateien zum Anpassen gefunden");
        return;
    }
    
    executeScrFile(scr, "Textbreitenanpassung");
}

void OpenCirtTab::onFullProjectGenerate() {
    QStringList errors;
    if (!validateProjectStructure(errors)) {
        QMessageBox::warning(this, "Projektstruktur",
            "Projektstruktur unvollstaendig:\n\n" + errors.join("\n"));
        return;
    }
    
    QMessageBox::StandardButton reply = QMessageBox::warning(this,
        "Projekt erstellen",
        "Empfehlung: Speichern Sie den gesamten Zeichnungsordner vorher als "
        "ZIP-Archiv als Sicherungskopie.\n\n"
        "Schritte: Deckblaetter + BMK + BAS + Extraktion + GA-FL + Summen + Textbreiten\n"
        "Alle bestehenden *_Deckblatt.dwg, *_GA_FL_*.dwg und *_Summe*.dwg werden geloescht.\n\n"
        "Phase 2 (Erzeugung) startet automatisch nach Abschluss der Extraktion.\n\nFortfahren?",
        QMessageBox::Yes | QMessageBox::Cancel);
    
    if (reply != QMessageBox::Yes) return;
    
    log("=== PROJEKT ERSTELLEN ===");
    saveConfig();
    
    QStringList dwgFiles = findProjectDwgs();
    if (dwgFiles.isEmpty()) {
        logError("Keine Projektzeichnungen gefunden");
        return;
    }
    
    log(QString("%1 DWG-Dateien gefunden").arg(dwgFiles.size()));
    
    // Step 0: Cleanup
    log("Cleanup...");
    cleanupDeckblaetter();
    cleanupInhalt();
    cleanupGaFl();
    
    // Step 1: ODS conversion
    log("ODS-Konvertierung...");
    QString odsPath = referencePath(OpenCirtConfig::GA_FL_VORLAGE_ODS);
    QString csvPath = referencePath("GA_FL_VORLAGE.csv");
    if (QFileInfo::exists(odsPath)) {
        if (!convertOdsToCSV(odsPath, csvPath)) {
            logError("ODS-Konvertierung fehlgeschlagen - Abbruch");
            return;
        }
    }
    
    // Set temp dir for extraction
    m_extractTempDir = QStandardPaths::writableLocation(QStandardPaths::TempLocation)
                       + "/OpenCirt_extract";
    QDir extractDir(m_extractTempDir);
    if (extractDir.exists()) {
        extractDir.removeRecursively();
    }
    QDir().mkpath(m_extractTempDir);
    
    // Build combined Phase 1 SCR
    QString combinedScr;
    
    // Step 1.5: Plankopf-Stammdaten aus CSV (AG, AN, PR etc.)
    log("Plankopf-Daten aus CSV setzen...");
    QString plankopfCsvScr = generatePlankopfCsvScr(dwgFiles);
    if (!plankopfCsvScr.trimmed().isEmpty()) {
        combinedScr += "; === Plankopf-Stammdaten aus CSV ===\n";
        combinedScr += plankopfCsvScr;
    }
    
    // Step 2: Deckblaetter
    log("Deckblaetter erzeugen...");
    QString deckblattScr = generateDeckblattScr();
    if (!deckblattScr.trimmed().isEmpty()) {
        combinedScr += "; === Deckblaetter ===\n";
        combinedScr += deckblattScr;
    }
    
    // Step 2.5: Plankopf ASP/GEWERK/ANLAGE aus Ordnerhierarchie setzen
    log("Plankopf-ASP aus Ordnerhierarchie setzen...");
    QString aspScr = generatePlankopfAspScr(dwgFiles);
    if (!aspScr.trimmed().isEmpty()) {
        combinedScr += "; === Plankopf ASP aus Ordnerhierarchie ===\n";
        combinedScr += aspScr;
    }
    
    // Step 3: BMK (optional)
    if (m_chkIncludeBmk->isChecked()) {
        log("BMK-Nummerierung...");
        QString lspPath = scriptsPath() + "/BmkNummerierung.lsp";
        if (QFileInfo::exists(lspPath)) {
            combinedScr += "; === BMK-Nummerierung ===\n";
            combinedScr += generateBmkScr(dwgFiles);
        } else {
            logError("BmkNummerierung.lsp nicht gefunden - uebersprungen");
        }
    } else {
        log("BMK-Nummerierung uebersprungen (deaktiviert)");
    }
    
    // Step 4: BAS (optional)
    if (m_chkIncludeBas->isChecked()) {
        log("BAS-Generierung...");
        QString lspPath = scriptsPath() + "/GenBas.lsp";
        QString basCsv = referencePath(OpenCirtConfig::BAS_CSV);
        if (QFileInfo::exists(lspPath) && QFileInfo::exists(basCsv)) {
            combinedScr += "; === BAS-Generierung ===\n";
            combinedScr += generateBasScr(dwgFiles);
        } else {
            logError("GenBas.lsp oder BAS.csv nicht gefunden - uebersprungen");
        }
    } else {
        log("BAS-Generierung uebersprungen (deaktiviert)");
    }
    
    // Step 5: GA-FL Extraction
    log("GA-FL Extraktion...");
    combinedScr += "; === GA-FL Phase 1: Extraktion ===\n";
    combinedScr += generateExtractionScr(dwgFiles);
    
    // Add marker file write at end of Phase 1 SCR
    m_phase1MarkerPath = m_extractTempDir + "/phase1_complete.marker";
    QString markerPath = m_phase1MarkerPath;
    markerPath.replace("\\", "/");
    combinedScr += "; --- Phase 1 completion marker ---\n";
    combinedScr += QString("(progn (setq f (open \"%1\" \"w\"))(write-line \"PHASE1_COMPLETE\" f)(close f)(princ))\n")
                  .arg(markerPath);
    
    // Execute Phase 1
    m_fullProjectMode = true;
    if (!combinedScr.trimmed().isEmpty()) {
        if (executeScrFile(combinedScr, "Projekt erstellen - Phase 1")) {
            m_gaFlPhase = GaFlPhase::Phase1Done;
            m_btnFullProject->setEnabled(false);
            m_btnGaFl->setEnabled(false);
            m_btnFullProject->setText("Projekt wird erstellt...");
            m_phase1Timer->start();
            logSuccess("Phase 1 gestartet. Phase 2 startet automatisch nach Abschluss.");
        }
    } else {
        logError("Keine Operationen zum Ausfuehren");
        m_fullProjectMode = false;
    }
}

// ============================================================================
// Projekt bereinigen
// ============================================================================

void OpenCirtTab::onProjektBereinigen() {
    QStringList errors;
    if (!validateProjectStructure(errors)) {
        QMessageBox::warning(this, "Projektstruktur",
            "Projektstruktur unvollstaendig:\n\n" + errors.join("\n"));
        return;
    }
    
    QString drawingsDir = projectPath(OpenCirtConfig::ZEICHNUNGEN_DIR);
    
    // File patterns to clean up
    QStringList patterns = {
        "*.bak", "*.dwl", "*.dwl2", "*.sv$", "*.ac$",
        "*.tmp", "*.log",
        "crash_report.txt",
        "Thumbs.db", "desktop.ini"
    };
    
    // Scan for matching files
    QStringList filesToDelete;
    qint64 totalSize = 0;
    
    QDirIterator it(drawingsDir, patterns, QDir::Files | QDir::Hidden,
                    QDirIterator::Subdirectories);
    while (it.hasNext()) {
        QString path = it.next();
        QFileInfo fi(path);
        totalSize += fi.size();
        filesToDelete << path;
    }
    
    if (filesToDelete.isEmpty()) {
        QMessageBox::information(this, "Projekt bereinigen",
            "Keine temporaeren Dateien gefunden.\n\n"
            "Das Projekt ist bereits sauber.");
        log("Projekt bereinigen: Keine temporaeren Dateien gefunden");
        return;
    }
    
    // Build summary by extension
    QMap<QString, int> extCount;
    QMap<QString, qint64> extSize;
    for (const QString& path : filesToDelete) {
        QString name = QFileInfo(path).fileName();
        QString ext;
        if (name == "crash_report.txt" || name == "Thumbs.db" || name == "desktop.ini") {
            ext = name;
        } else {
            ext = "*." + QFileInfo(path).suffix();
        }
        extCount[ext]++;
        extSize[ext] += QFileInfo(path).size();
    }
    
    QString summary;
    for (auto it = extCount.constBegin(); it != extCount.constEnd(); ++it) {
        double sizeMB = extSize[it.key()] / (1024.0 * 1024.0);
        summary += QString("  %1: %2 Dateien (%3 MB)\n")
                   .arg(it.key())
                   .arg(it.value())
                   .arg(sizeMB, 0, 'f', 1);
    }
    
    double totalMB = totalSize / (1024.0 * 1024.0);
    
    QMessageBox::StandardButton reply = QMessageBox::warning(this,
        "Projekt bereinigen",
        QString("Folgende temporaere Dateien wurden in\n"
                "'%1' gefunden:\n\n"
                "%2\n"
                "Gesamt: %3 Dateien (%4 MB)\n\n"
                "Endgueltig loeschen?")
        .arg(OpenCirtConfig::ZEICHNUNGEN_DIR)
        .arg(summary)
        .arg(filesToDelete.size())
        .arg(totalMB, 0, 'f', 1),
        QMessageBox::Yes | QMessageBox::Cancel);
    
    if (reply != QMessageBox::Yes) return;
    
    log("=== PROJEKT BEREINIGEN ===");
    
    int deleted = 0;
    int failed = 0;
    for (const QString& path : filesToDelete) {
        if (QFile::remove(path)) {
            deleted++;
        } else {
            failed++;
            logError(QString("Konnte nicht loeschen: %1").arg(QFileInfo(path).fileName()));
        }
    }
    
    if (failed == 0) {
        logSuccess(QString("Bereinigung abgeschlossen: %1 Dateien geloescht (%2 MB freigegeben)")
                   .arg(deleted).arg(totalMB, 0, 'f', 1));
    } else {
        log(QString("Bereinigung: %1 geloescht, %2 fehlgeschlagen").arg(deleted).arg(failed), "WARN");
    }
}

// ============================================================================
// Deckblatt Generation
// ============================================================================

QString OpenCirtTab::folderDisplayName(const QString& folderName) {
    // "01 ASP01" -> "ASP01", "01 Los 1 Anlagenautomation" -> "Los 1 Anlagenautomation"
    // "00 GA" -> "GA", "03 KAE" -> "KAE"
    QRegularExpression re("^\\d+[-\\s]+(.+)$");
    QRegularExpressionMatch match = re.match(folderName);
    if (match.hasMatch()) {
        return match.captured(1).trimmed();
    }
    return folderName;
}

QString OpenCirtTab::findDeckblattVorlage() {
    // Find OC_VORLAGE_DIN_A2_*.dwg ignoring version number
    QString vorlagenDir = projectPath(OpenCirtConfig::VORLAGEN_DIR);
    QDir dir(vorlagenDir);
    QStringList filters;
    filters << "OC_VORLAGE_DIN_A2*.dwg";
    QStringList matches = dir.entryList(filters, QDir::Files);
    if (!matches.isEmpty()) {
        QString path = vorlagenDir + "/" + matches.first();
        path.replace("\\", "/");
        return path;
    }
    return QString();
}

int OpenCirtTab::cleanupDeckblaetter() {
    QString drawingsDir = projectPath(OpenCirtConfig::ZEICHNUNGEN_DIR);
    int deletedCount = 0;
    
    QDirIterator it(drawingsDir, QStringList() << "*_Deckblatt.dwg" << "*_Deckblatt.bak"
                    << "*_Deckblatt_B.dwg" << "*_Deckblatt_B.bak",
                    QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        QString path = it.next();
        // Never delete manually created _A (Uebersichtsseite)
        if (QFileInfo(path).fileName().contains("_Deckblatt_A")) continue;
        if (QFile::remove(path)) {
            deletedCount++;
        }
    }
    
    log(QString("Deckblatt-Cleanup: %1 Dateien geloescht").arg(deletedCount));
    return deletedCount;
}

QString OpenCirtTab::generateDeckblattScr() {
    QString scr;
    
    QString vorlage = findDeckblattVorlage();
    if (vorlage.isEmpty()) {
        logError("Deckblatt-Vorlage OC_VORLAGE_DIN_A2_*.dwg nicht gefunden");
        return scr;
    }
    
    QString drawingsDir = projectPath(OpenCirtConfig::ZEICHNUNGEN_DIR);
    drawingsDir.replace("\\", "/");
    
    // Recursively find all subdirectories under Zeichnungen
    QDirIterator dirIt(drawingsDir, QDir::Dirs | QDir::NoDotAndDotDot,
                       QDirIterator::Subdirectories);
    
    QStringList folders;
    while (dirIt.hasNext()) {
        folders << dirIt.next();
    }
    folders.sort();  // Ensure alphabetical order
    
    int deckblattCount = 0;
    
    // === Projekt-Deckblatt (root level, reads PR1 for project name) ===
    {
        QString targetName = "0000 Projekt_Deckblatt_B.dwg";
        QString targetPath = drawingsDir + "/" + targetName;
        targetPath.replace("\\", "/");
        
        scr += "; --- Projekt-Deckblatt (PR1 aus Vorlage) ---\n";
        
        // 1. Copy template
        scr += QString("(progn (vl-file-copy \"%1\" \"%2\" T)(princ))\n")
               .arg(vorlage, targetPath);
        
        // 2. Open
        scr += QString("_.OPEN \"%1\"\n").arg(targetPath);
        
        // 3. Thaw layer GA-Deckblatt, freeze Trennlinien layers
        // Direct VLA property manipulation - more reliable than command in SCR context
        scr += "(progn\n"
               "  (setq layers (vla-get-Layers (vla-get-ActiveDocument (vlax-get-acad-object))))\n"
               "  (if (tblsearch \"LAYER\" \"GA-Deckblatt\")\n"
               "    (progn\n"
               "      (setq lo (vla-item layers \"GA-Deckblatt\"))\n"
               "      (vla-put-Freeze lo :vlax-false)\n"
               "      (vla-put-LayerOn lo :vlax-true)\n"
               "    )\n"
               "  )\n"
               "  (foreach ln '(\"GA-Trennlinie-BAS\" \"GA-Trennlinie-LVB\"\n"
               "                \"GA-Trennlinie-Regeldiagramme\" \"GA-Trennlinie-Regelstruktur\"\n"
               "                \"GA-Konstruktionslinie\" \"GA-MBE-Grafik\")\n"
               "    (if (tblsearch \"LAYER\" ln)\n"
               "      (progn\n"
               "        (setq lo (vla-item layers ln))\n"
               "        (vla-put-Freeze lo :vlax-true)\n"
               "      )\n"
               "    )\n"
               "  )\n"
               "  (princ)\n"
               ")\n";
        
        // 4. Read PR1 attribute from Plankopf, replace DECKBLATT text
        scr += "(progn\n"
               "  (setq projekt-name \"Projekt\")\n"
               "  (setq ss-ins (ssget \"X\" '((0 . \"INSERT\"))))\n"
               "  (if ss-ins\n"
               "    (progn\n"
               "      (setq i 0)\n"
               "      (while (< i (sslength ss-ins))\n"
               "        (setq ent (ssname ss-ins i))\n"
               "        (setq obj (vlax-ename->vla-object ent))\n"
               "        (if (vlax-property-available-p obj 'HasAttributes)\n"
               "          (if (= (vla-get-HasAttributes obj) :vlax-true)\n"
               "            (foreach att (vlax-invoke obj 'GetAttributes)\n"
               "              (if (= (strcase (vla-get-TagString att)) \"PR1\")\n"
               "                (if (/= (vla-get-TextString att) \"\")\n"
               "                  (setq projekt-name (vla-get-TextString att))\n"
               "                )\n"
               "              )\n"
               "            )\n"
               "          )\n"
               "        )\n"
               "        (setq i (1+ i))\n"
               "      )\n"
               "    )\n"
               "  )\n"
               "  (setq ss-txt (ssget \"X\" '((0 . \"TEXT,MTEXT\"))))\n"
               "  (if ss-txt\n"
               "    (progn\n"
               "      (setq j 0)\n"
               "      (while (< j (sslength ss-txt))\n"
               "        (setq ent2 (ssname ss-txt j))\n"
               "        (setq ed (entget ent2))\n"
               "        (setq txt (cdr (assoc 1 ed)))\n"
               "        (if (= (strcase txt) \"DECKBLATT\")\n"
               "          (progn\n"
               "            (setq ed (subst (cons 1 projekt-name) (assoc 1 ed) ed))\n"
               "            (entmod ed)\n"
               "            (entupd ent2)\n"
               "          )\n"
               "        )\n"
               "        (setq j (1+ j))\n"
               "      )\n"
               "    )\n"
               "  )\n"
               "  (princ)\n"
               ")\n";
        
        // 4b. ASP-Attribut leeren (Projekt-Ebene)
        scr += generateSetAspSnippet("");
        
        // 4c. Plankopf-Stammdaten aus CSV (falls vorhanden)
        if (!m_plankopfCsvData.isEmpty()) {
            scr += generateSetPlankopfSnippet(m_plankopfCsvData);
        }
        
        // 5. Save and close
        scr += lispSave();
        scr += lispClose();
        
        deckblattCount++;
    }
    
    // === Folder-level Deckblaetter ===
    for (const QString& folderPath : folders) {
        QDir folder(folderPath);
        QString folderName = folder.dirName();
        QString displayName = folderDisplayName(folderName);
        
        // Target: {folderPath}/0000 {displayName}_Deckblatt.dwg
        // "0000 " sorts before "00 " and "01 " -> always first in folder
        QString targetName = QString("0000 %1_Deckblatt.dwg").arg(displayName);
        QString targetPath = folderPath + "/" + targetName;
        targetPath.replace("\\", "/");
        
        scr += QString("; --- Deckblatt: %1 ---\n").arg(displayName);
        
        // 1. Copy template
        scr += QString("(progn (vl-file-copy \"%1\" \"%2\" T)(princ))\n")
               .arg(vorlage, targetPath);
        
        // 2. Open
        scr += QString("_.OPEN \"%1\"\n").arg(targetPath);
        
        // 3. Thaw layer GA-Deckblatt, freeze Trennlinien layers
        // Direct VLA property manipulation - more reliable than command in SCR context
        scr += "(progn\n"
               "  (setq layers (vla-get-Layers (vla-get-ActiveDocument (vlax-get-acad-object))))\n"
               "  (if (tblsearch \"LAYER\" \"GA-Deckblatt\")\n"
               "    (progn\n"
               "      (setq lo (vla-item layers \"GA-Deckblatt\"))\n"
               "      (vla-put-Freeze lo :vlax-false)\n"
               "      (vla-put-LayerOn lo :vlax-true)\n"
               "    )\n"
               "  )\n"
               "  (foreach ln '(\"GA-Trennlinie-BAS\" \"GA-Trennlinie-LVB\"\n"
               "                \"GA-Trennlinie-Regeldiagramme\" \"GA-Trennlinie-Regelstruktur\"\n"
               "                \"GA-Konstruktionslinie\" \"GA-MBE-Grafik\")\n"
               "    (if (tblsearch \"LAYER\" ln)\n"
               "      (progn\n"
               "        (setq lo (vla-item layers ln))\n"
               "        (vla-put-Freeze lo :vlax-true)\n"
               "      )\n"
               "    )\n"
               "  )\n"
               "  (princ)\n"
               ")\n";
        
        // 4. Replace text "DECKBLATT" with display name (TEXT + MTEXT entities)
        scr += QString(
            "(progn\n"
            "  (setq ss (ssget \"X\" '((0 . \"TEXT,MTEXT\"))))\n"
            "  (if ss\n"
            "    (progn\n"
            "      (setq i 0)\n"
            "      (while (< i (sslength ss))\n"
            "        (setq ent (ssname ss i))\n"
            "        (setq ed (entget ent))\n"
            "        (setq txt (cdr (assoc 1 ed)))\n"
            "        (if (= (strcase txt) \"DECKBLATT\")\n"
            "          (progn\n"
            "            (setq ed (subst (cons 1 \"%1\") (assoc 1 ed) ed))\n"
            "            (entmod ed)\n"
            "            (entupd ent)\n"
            "          )\n"
            "        )\n"
            "        (setq i (1+ i))\n"
            "      )\n"
            "    )\n"
            "  )\n"
            "  (princ)\n"
            ")\n"
        ).arg(displayName);
        
        // 4b. ASP-Attribut setzen: Hierarchie nach oben durchlaufen um ASP-Ordner zu finden
        {
            QString aspValue;
            QDir aspSearch(folderPath);
            while (aspSearch.absolutePath().length() > drawingsDir.length()) {
                QString dn = aspSearch.dirName();
                if (dn.contains("ASP", Qt::CaseInsensitive) ||
                    dn.contains("ISP", Qt::CaseInsensitive)) {
                    aspValue = folderDisplayName(dn);
                    break;
                }
                aspSearch.cdUp();
            }
            scr += generateSetAspSnippet(aspValue);  // leer wenn kein ASP gefunden
        }
        
        // 4c. Plankopf-Stammdaten aus CSV (falls vorhanden)
        if (!m_plankopfCsvData.isEmpty()) {
            scr += generateSetPlankopfSnippet(m_plankopfCsvData);
        }
        
        // 5. Save and close
        scr += lispSave();
        scr += lispClose();
        
        deckblattCount++;
    }
    
    log(QString("Deckblatt-SCR: %1 Deckblaetter geplant").arg(deckblattCount));
    return scr;
}

QString OpenCirtTab::generateSetAspSnippet(const QString& aspValue) {
    return QString(
        "(progn\n"
        "  (setq ss-ins (ssget \"X\" '((0 . \"INSERT\"))))\n"
        "  (if ss-ins\n"
        "    (progn\n"
        "      (setq i 0)\n"
        "      (while (< i (sslength ss-ins))\n"
        "        (setq ent (ssname ss-ins i))\n"
        "        (setq obj (vlax-ename->vla-object ent))\n"
        "        (if (and (vlax-property-available-p obj 'HasAttributes)\n"
        "                 (= (vla-get-HasAttributes obj) :vlax-true))\n"
        "          (foreach att (vlax-invoke obj 'GetAttributes)\n"
        "            (if (= (strcase (vla-get-TagString att)) \"ASP\")\n"
        "              (vla-put-TextString att \"%1\")\n"
        "            )\n"
        "          )\n"
        "        )\n"
        "        (setq i (1+ i))\n"
        "      )\n"
        "    )\n"
        "  )\n"
        "  (princ)\n"
        ")\n"
    ).arg(aspValue);
}

// ============================================================================
// Plankopf CSV: Read & Apply
// ============================================================================

QMap<QString, QString> OpenCirtTab::readPlankopfCsv() {
    QMap<QString, QString> data;
    QString csvPath = referencePath(OpenCirtConfig::PLANKOPF_CSV);
    
    QFile file(csvPath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        log(QString("plankopfdaten.csv nicht gefunden: %1").arg(csvPath), "WARN");
        return data;
    }
    
    QTextStream stream(&file);
    stream.setEncoding(QStringConverter::Utf8);
    
    bool headerSkipped = false;
    int lineNum = 0;
    
    while (!stream.atEnd()) {
        QString line = stream.readLine().trimmed();
        lineNum++;
        if (line.isEmpty()) continue;
        
        // Zeile 1 = Header, ueberspringen
        if (!headerSkipped) {
            headerSkipped = true;
            continue;
        }
        
        // Semikolon-getrennt: Spalte1=Attributname, Spalte2=Wert, Spalte3=Kommentar(ignoriert)
        QStringList fields = line.split(";");
        if (fields.size() < 2) continue;
        
        QString attrName = fields[0].trimmed();
        QString attrValue = fields[1].trimmed();
        
        if (attrName.isEmpty()) continue;
        
        data[attrName] = attrValue;
    }
    
    file.close();
    log(QString("plankopfdaten.csv: %1 Attribute gelesen").arg(data.size()));
    return data;
}

QString OpenCirtTab::generateSetPlankopfSnippet(const QMap<QString, QString>& attrs) {
    if (attrs.isEmpty()) return QString();
    
    // Generate LISP that sets all given attributes in all INSERT blocks
    // (not just Plankopf blocks - catches all blocks that have matching attributes)
    QString lisp;
    lisp += "(progn\n";
    lisp += "  (setq ss-all (ssget \"X\" '((0 . \"INSERT\"))))\n";
    lisp += "  (if ss-all\n";
    lisp += "    (progn\n";
    lisp += "      (setq idx 0)\n";
    lisp += "      (while (< idx (sslength ss-all))\n";
    lisp += "        (setq ent (ssname ss-all idx))\n";
    lisp += "        (setq obj (vlax-ename->vla-object ent))\n";
    lisp += "        (if (and (vlax-property-available-p obj 'HasAttributes)\n";
    lisp += "                 (= (vla-get-HasAttributes obj) :vlax-true))\n";
    lisp += "          (foreach att (vlax-invoke obj 'GetAttributes)\n";
    lisp += "            (cond\n";
    
    for (auto it = attrs.constBegin(); it != attrs.constEnd(); ++it) {
        // Escape backslashes and quotes for LISP string embedding
        QString escapedVal = it.value();
        escapedVal.replace("\\", "\\\\");
        escapedVal.replace("\"", "\\\"");
        
        lisp += QString(
            "              ((= (strcase (vla-get-TagString att)) \"%1\")\n"
            "               (vla-put-TextString att \"%2\"))\n"
        ).arg(it.key().toUpper(), escapedVal);
    }
    
    lisp += "            )\n";
    lisp += "          )\n";
    lisp += "        )\n";
    lisp += "        (setq idx (1+ idx))\n";
    lisp += "      )\n";
    lisp += "    )\n";
    lisp += "  )\n";
    lisp += "  (princ)\n";
    lisp += ")\n";
    
    return lisp;
}

QString OpenCirtTab::generatePlankopfCsvScr(const QStringList& dwgFiles) {
    QMap<QString, QString> csvData = readPlankopfCsv();
    if (csvData.isEmpty()) return QString();
    
    // Cache for later use (Deckblatt, parseExtractedCsv)
    m_plankopfCsvData = csvData;
    
    QString lispSnippet = generateSetPlankopfSnippet(csvData);
    if (lispSnippet.isEmpty()) return QString();
    
    QString scr;
    int count = 0;
    
    for (const QString& dwg : dwgFiles) {
        QString dwgPath = dwg;
        dwgPath.replace("\\", "/");
        
        scr += QString("; --- Plankopf-CSV: %1 ---\n").arg(QFileInfo(dwg).fileName());
        scr += QString("_.OPEN \"%1\"\n").arg(dwgPath);
        scr += lispSnippet;
        scr += lispSave();
        scr += lispClose();
        count++;
    }
    
    log(QString("Plankopf-CSV-SCR: %1 Attribute in %2 Dateien").arg(csvData.size()).arg(count));
    return scr;
}

void OpenCirtTab::onPlankopfCsvGenerate() {
    QStringList errors;
    if (!validateProjectStructure(errors)) {
        QMessageBox::warning(this, "Projektstruktur",
            "Projektstruktur unvollstaendig:\n\n" + errors.join("\n"));
        return;
    }
    
    // Check if CSV exists
    QString csvPath = referencePath(OpenCirtConfig::PLANKOPF_CSV);
    if (!QFileInfo::exists(csvPath)) {
        QMessageBox::warning(this, "Plankopf-Daten",
            QString("Datei nicht gefunden:\n%1\n\n"
                    "Bitte erstellen Sie die Datei im Referenzen-Ordner.\n"
                    "Format: Semikolon-getrennt, Spalte 1 = Attributname, "
                    "Spalte 2 = Wert, Spalte 3 = Kommentar (optional)")
            .arg(csvPath));
        return;
    }
    
    QStringList dwgFiles = findProjectDwgs();
    if (dwgFiles.isEmpty()) {
        logError("Keine Projektzeichnungen gefunden");
        return;
    }
    
    log("=== PLANKOPF-DATEN AUS CSV ===");
    
    // Also find existing Deckblatt DWGs to include them
    QString drawingsDir = projectPath(OpenCirtConfig::ZEICHNUNGEN_DIR);
    QDirIterator deckIt(drawingsDir, QStringList() << "*_Deckblatt*.dwg",
                        QDir::Files, QDirIterator::Subdirectories);
    QStringList allDwgs = dwgFiles;
    while (deckIt.hasNext()) {
        QString deckPath = deckIt.next();
        if (!allDwgs.contains(deckPath)) {
            allDwgs.append(deckPath);
        }
    }
    
    // Also find existing GA-FL and Summe DWGs
    QDirIterator gaflIt(drawingsDir, QStringList() << "*_GA_FL_*.dwg" << "*_Summe_*.dwg",
                        QDir::Files, QDirIterator::Subdirectories);
    while (gaflIt.hasNext()) {
        QString gaflPath = gaflIt.next();
        if (!allDwgs.contains(gaflPath)) {
            allDwgs.append(gaflPath);
        }
    }
    
    log(QString("%1 Dateien werden aktualisiert (inkl. Deckblaetter, GA-FL, Summen)").arg(allDwgs.size()));
    
    QString scr = generatePlankopfCsvScr(allDwgs);
    if (scr.isEmpty()) {
        logError("Keine Plankopf-Daten zum Schreiben");
        return;
    }
    
    executeScrFile(scr, "Plankopf-Daten setzen");
}

QString OpenCirtTab::generatePlankopfAspScr(const QStringList& dwgFiles) {
    QString scr;
    QString drawingsDir = projectPath(OpenCirtConfig::ZEICHNUNGEN_DIR);
    int setCount = 0;
    
    for (const QString& dwg : dwgFiles) {
        // Find ASP folder path by walking up
        QDir searchDir(QFileInfo(dwg).absolutePath());
        QString aspFolderPath;
        QString aspRaw;
        
        while (searchDir.absolutePath().length() > drawingsDir.length()) {
            QString dn = searchDir.dirName();
            if (dn.contains("ASP", Qt::CaseInsensitive) ||
                dn.contains("ISP", Qt::CaseInsensitive)) {
                aspRaw = dn;
                aspFolderPath = searchDir.absolutePath();
                break;
            }
            searchDir.cdUp();
        }
        
        if (aspRaw.isEmpty()) continue;
        
        QString aspDisplay = folderDisplayName(aspRaw);
        
        // Derive Gewerk and Anlage from path below ASP folder
        // Structure: ASP / Gewerk / Anlage / *.dwg
        QString dwgParent = QFileInfo(dwg).absolutePath();
        QString relPath = QDir(aspFolderPath).relativeFilePath(dwgParent);
        QStringList pathParts = relPath.split("/", Qt::SkipEmptyParts);
        // Filter out "." (DWG directly in ASP folder)
        pathParts.removeAll(".");
        
        QString gewerkDisplay;
        QString anlageDisplay;
        
        if (pathParts.size() >= 1) {
            gewerkDisplay = folderDisplayName(pathParts[0]);
        }
        if (pathParts.size() >= 2) {
            anlageDisplay = folderDisplayName(pathParts[1]);
        }
        
        QString dwgPath = dwg;
        dwgPath.replace("\\", "/");
        
        scr += QString("; --- Plankopf: %1 -> ASP=%2 GEWERK=%3 ANLAGE=%4 ---\n")
               .arg(QFileInfo(dwg).fileName(), aspDisplay, gewerkDisplay, anlageDisplay);
        scr += QString("_.OPEN \"%1\"\n").arg(dwgPath);
        
        // Targeted LISP: set ASP, GEWERK, ANLAGE in Plankopf block (OC_RSH_Plankopf_quer*)
        // Checks both tag variants (GEWERK/OC_GEWERK, ANLAGE/OC_ANLAGE) for compatibility
        scr += QString(
            "(progn\n"
            "  (setq ss-pk (ssget \"X\" '((0 . \"INSERT\")(2 . \"OC_RSH_Plankopf_quer*\"))))\n"
            "  (if ss-pk\n"
            "    (progn\n"
            "      (setq i 0)\n"
            "      (while (< i (sslength ss-pk))\n"
            "        (setq ent (ssname ss-pk i))\n"
            "        (setq obj (vlax-ename->vla-object ent))\n"
            "        (if (and (vlax-property-available-p obj 'HasAttributes)\n"
            "                 (= (vla-get-HasAttributes obj) :vlax-true))\n"
            "          (foreach att (vlax-invoke obj 'GetAttributes)\n"
            "            (cond\n"
            "              ((= (strcase (vla-get-TagString att)) \"ASP\")\n"
            "               (vla-put-TextString att \"%1\"))\n"
            "              ((member (strcase (vla-get-TagString att)) '(\"GEWERK\" \"OC_GEWERK\"))\n"
            "               (vla-put-TextString att \"%2\"))\n"
            "              ((member (strcase (vla-get-TagString att)) '(\"ANLAGE\" \"OC_ANLAGE\"))\n"
            "               (vla-put-TextString att \"%3\"))\n"
            "            )\n"
            "          )\n"
            "        )\n"
            "        (setq i (1+ i))\n"
            "      )\n"
            "    )\n"
            "  )\n"
            "  (princ)\n"
            ")\n"
        ).arg(aspDisplay, gewerkDisplay, anlageDisplay);
        
        scr += lispSave();
        scr += lispClose();
        setCount++;
    }
    
    log(QString("Plankopf-Attribute-SCR: %1 von %2 Dateien erhalten ASP/GEWERK/ANLAGE aus Ordnerhierarchie")
        .arg(setCount).arg(dwgFiles.size()));
    return scr;
}

void OpenCirtTab::onDeckblattGenerate() {
    QStringList errors;
    if (!validateProjectStructure(errors)) {
        QMessageBox::warning(this, "Projektstruktur",
            "Projektstruktur unvollstaendig:\n\n" + errors.join("\n"));
        return;
    }
    
    QString vorlage = findDeckblattVorlage();
    if (vorlage.isEmpty()) {
        QMessageBox::warning(this, "Deckblaetter",
            "Deckblatt-Vorlage OC_VORLAGE_DIN_A2_*.dwg nicht gefunden \n"
            "im Vorlagen-Ordner.");
        return;
    }
    
    QMessageBox::StandardButton reply = QMessageBox::question(this,
        "Deckblaetter erstellen",
        "Alle bestehenden *_Deckblatt.dwg werden geloescht \n"
        "und fuer jeden Ordner (Los, ASP, Gewerk, Anlage) \n"
        "neu erzeugt.\n\nFortfahren?",
        QMessageBox::Yes | QMessageBox::Cancel);
    
    if (reply != QMessageBox::Yes) return;
    
    log("=== DECKBLAETTER ERSTELLEN ===");
    
    // Cleanup
    cleanupDeckblaetter();
    
    // Generate SCR
    QString scr = generateDeckblattScr();
    if (scr.trimmed().isEmpty()) {
        logError("Keine Deckblaetter zu erzeugen");
        return;
    }
    
    // Wrap with system variable setup
    QString fullScr;
    fullScr += "(progn (setvar \"FILEDIA\" 0)(princ))\n";
    fullScr += "(progn (setvar \"CMDECHO\" 0)(princ))\n";
    fullScr += "(progn (setvar \"EXPERT\" 5)(princ))\n";
    fullScr += scr;
    fullScr += "(progn (setvar \"FILEDIA\" 1)(princ))\n";
    fullScr += "(progn (setvar \"CMDECHO\" 1)(princ))\n";
    fullScr += "(progn (setvar \"EXPERT\" 0)(princ))\n";
    
    if (executeScrFile(fullScr, "Deckblaetter")) {
        logSuccess("Deckblatt-Generierung gestartet.");
    }
}

// ============================================================================
// SCR Generation: BMK
// ============================================================================

QString OpenCirtTab::generateBmkScr(const QStringList& dwgFiles) {
    QString scr;
    QString lspPath = scriptsPath() + "/BmkNummerierung.lsp";
    lspPath.replace("\\", "/");
    
    for (const QString& dwg : dwgFiles) {
        QString dwgPath = dwg;
        dwgPath.replace("\\", "/");
        
        scr += QString("_.OPEN \"%1\"\n").arg(dwgPath);
        scr += QString("(progn (load \"%1\")(princ))\n").arg(lspPath);
        scr += "(progn (BmkNummerierung)(princ))\n";
        scr += lispSave();
        scr += lispClose();
    }
    
    log(QString("BMK-SCR generiert: %1 Dateien").arg(dwgFiles.size()));
    return scr;
}

// ============================================================================
// SCR Generation: BAS
// ============================================================================

QString OpenCirtTab::generateBasScr(const QStringList& dwgFiles) {
    QString scr;
    QString lspPath = scriptsPath() + "/GenBas.lsp";
    lspPath.replace("\\", "/");
    
    // The BAS.csv path is passed to the LISP as a parameter
    QString basCsvPath = referencePath(OpenCirtConfig::BAS_CSV);
    basCsvPath.replace("\\", "/");
    
    for (const QString& dwg : dwgFiles) {
        QString dwgPath = dwg;
        dwgPath.replace("\\", "/");
        
        scr += QString("_.OPEN \"%1\"\n").arg(dwgPath);
        scr += QString("(progn (load \"%1\")(princ))\n").arg(lspPath);
        // Pass BAS.csv path as global variable before calling
        scr += QString("(progn (setq *oc-bas-csv-path* \"%1\")(princ))\n").arg(basCsvPath);
        scr += "(progn (GenBas)(princ))\n";
        scr += lispSave();
        scr += lispClose();
    }
    
    log(QString("BAS-SCR generiert: %1 Dateien").arg(dwgFiles.size()));
    return scr;
}

// ============================================================================
// SCR Generation: GA-FL Phase 1 (Extraction)
// ============================================================================

QString OpenCirtTab::generateExtractionScr(const QStringList& dwgFiles) {
    QString scr;
    QString lspPath = scriptsPath() + "/ExtractDP.lsp";
    lspPath.replace("\\", "/");
    
    // Temp directory for extracted CSVs
    QString tempDir = QStandardPaths::writableLocation(QStandardPaths::TempLocation)
                      + "/OpenCirt_extract";
    QDir().mkpath(tempDir);
    tempDir.replace("\\", "/");
    
    // Mirror Zeichnungen folder structure in temp dir to avoid CSV name collisions
    QString drawingsDir = projectPath(OpenCirtConfig::ZEICHNUNGEN_DIR);
    QDir drawingsRoot(drawingsDir);
    
    for (const QString& dwg : dwgFiles) {
        QString dwgPath = dwg;
        dwgPath.replace("\\", "/");
        
        // Compute relative path from Zeichnungen root -> subdirectory in temp
        QString relPath = drawingsRoot.relativeFilePath(QFileInfo(dwg).absolutePath());
        QString subDir = tempDir + "/" + relPath;
        subDir.replace("\\", "/");
        QDir().mkpath(subDir);
        
        scr += QString("_.OPEN \"%1\"\n").arg(dwgPath);
        scr += QString("(progn (load \"%1\")(princ))\n").arg(lspPath);
        // Pass mirrored subdirectory as output directory
        scr += QString("(progn (setq *oc-extract-dir* \"%1\")(princ))\n").arg(subDir);
        scr += "(progn (ExtractDP)(princ))\n";
        scr += lispSave();
        scr += lispClose();
    }
    
    log(QString("Extraktions-SCR generiert: %1 Dateien, Temp: %2").arg(dwgFiles.size()).arg(tempDir));
    return scr;
}

// ============================================================================
// GA-FL Cleanup
// ============================================================================

bool OpenCirtTab::cleanupGaFl() {
    QString drawingsDir = projectPath(OpenCirtConfig::ZEICHNUNGEN_DIR);
    int deletedCount = 0;
    
    QDirIterator it(drawingsDir, QStringList() << "*.dwg", QDir::Files,
                    QDirIterator::Subdirectories);
    while (it.hasNext()) {
        QString path = it.next();
        QString fileName = QFileInfo(path).fileName();
        
        if (fileName.contains("_GA_FL_") || 
            fileName.contains("_Summe_") ||
            fileName.startsWith("0001 Projekt_Summe", Qt::CaseInsensitive) ||
            fileName.startsWith("00 Projekt_Summe", Qt::CaseInsensitive) ||  // Legacy
            fileName == "Projekt_Summe.dwg") {  // Legacy
            if (QFile::remove(path)) {
                deletedCount++;
            } else {
                logError(QString("Konnte nicht loeschen: %1").arg(fileName));
            }
        }
    }
    
    // Also delete CSV cache
    QString csvCache = referencePath("GA_FL_VORLAGE.csv");
    if (QFile::exists(csvCache)) {
        QFile::remove(csvCache);
        log("CSV-Cache geloescht");
    }
    
    log(QString("Cleanup: %1 Dateien geloescht").arg(deletedCount));
    return true;
}

// ============================================================================
// SCR Generation: Text Width Adjustment
// ============================================================================

QString OpenCirtTab::generateTextwidthScr() {
    QString drawingsDir = projectPath(OpenCirtConfig::ZEICHNUNGEN_DIR);
    QString lspPath = scriptsPath() + "/TextBreitenAnpassenBloecke.lsp";
    lspPath.replace("\\", "/");
    
    // Find all GA-FL and summary files
    QStringList gaFlFiles;
    QDirIterator it(drawingsDir, QStringList() << "*.dwg", QDir::Files,
                    QDirIterator::Subdirectories);
    while (it.hasNext()) {
        QString path = it.next();
        QString fileName = QFileInfo(path).fileName();
        if (fileName.contains("_GA_FL_") || 
            fileName.contains("_Summe_") ||
            fileName.startsWith("0001 Projekt_Summe", Qt::CaseInsensitive) ||
            fileName.startsWith("00 Projekt_Summe", Qt::CaseInsensitive) ||
            fileName == "Projekt_Summe.dwg") {
            gaFlFiles << path;
        }
    }
    
    if (gaFlFiles.isEmpty()) {
        return QString();
    }
    
    QString scr;
    for (const QString& dwg : gaFlFiles) {
        QString dwgPath = dwg;
        dwgPath.replace("\\", "/");
        
        scr += QString("_.OPEN \"%1\"\n").arg(dwgPath);
        scr += QString("(progn (load \"%1\")(princ))\n").arg(lspPath);
        scr += "(progn (TextBreitenAnpassenBloecke)(princ))\n";
        scr += lispSave();
        scr += lispClose();
    }
    
    log(QString("Textbreiten-SCR generiert: %1 GA-FL-Dateien").arg(gaFlFiles.size()));
    return scr;
}

// ============================================================================
// Phase 2: Calculate Sheet Count
// ============================================================================

int OpenCirtTab::calculateSheetCount(int dpCount) {
    if (dpCount <= 0) return 0;
    if (dpCount <= OpenCirtConfig::MAX_DP_FIRST_SHEET) return 1;
    
    // First sheet: 25 DPs, follow-up sheets: 24 DPs each (row 1 = carryover)
    int remaining = dpCount - OpenCirtConfig::MAX_DP_FIRST_SHEET;
    int followSheets = (remaining + OpenCirtConfig::MAX_DP_FOLLOW_SHEET - 1) 
                       / OpenCirtConfig::MAX_DP_FOLLOW_SHEET;
    return 1 + followSheets;
}

// ============================================================================
// Phase 2: Parse Single Extracted CSV
// ============================================================================

SourceDrawingInfo OpenCirtTab::parseExtractedCsv(const QString& csvPath, const QString& dwgPath) {
    SourceDrawingInfo info;
    info.filePath = dwgPath;
    info.fileName = QFileInfo(dwgPath).completeBaseName();
    info.aspName = detectAspFromPath(dwgPath);
    info.aspFolder = QFileInfo(dwgPath).absolutePath();
    
    // Detect Gewerk and Anlage from folder structure relative to ASP
    // Structure: ASP / Gewerk / Anlage / *.dwg
    {
        QString dwgParent = QFileInfo(dwgPath).absolutePath();
        QString drawingsRoot = projectPath(OpenCirtConfig::ZEICHNUNGEN_DIR);
        
        // Find ASP folder by walking up
        QDir aspSearchDir(dwgParent);
        QString aspFolderPath;
        while (aspSearchDir.absolutePath().length() > drawingsRoot.length()) {
            if (aspSearchDir.dirName().contains("ASP", Qt::CaseInsensitive) ||
                aspSearchDir.dirName().contains("ISP", Qt::CaseInsensitive)) {
                aspFolderPath = aspSearchDir.absolutePath();
                break;
            }
            aspSearchDir.cdUp();
        }
        
        if (!aspFolderPath.isEmpty()) {
            QString relPath = QDir(aspFolderPath).relativeFilePath(dwgParent);
            QStringList pathParts = relPath.split("/", Qt::SkipEmptyParts);
            pathParts.removeAll(".");
            
            if (pathParts.size() >= 1) {
                info.gewerk = folderDisplayName(pathParts[0]);
            }
            if (pathParts.size() >= 2) {
                info.anlage = folderDisplayName(pathParts[1]);
            }
        } else {
            // Fallback: parent folder as gewerk
            info.gewerk = folderDisplayName(QDir(dwgParent).dirName());
        }
    }
    
    QFile file(csvPath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        logError(QString("CSV nicht lesbar: %1").arg(csvPath));
        return info;
    }
    
    QTextStream stream(&file);
    // ExtractDP.lsp schreibt ueber BricsCAD LISP (open) in Windows-Systemcodepage (CP1252)
    stream.setEncoding(QStringConverter::Latin1);
    
    QStringList header;
    bool headerRead = false;
    
    while (!stream.atEnd()) {
        QString line = stream.readLine().trimmed();
        if (line.isEmpty()) continue;
        
        // Parse Plankopf comment line
        if (line.startsWith("#PLANKOPF;")) {
            QStringList parts = line.mid(10).split(";");
            for (const QString& part : parts) {
                int eqPos = part.indexOf('=');
                if (eqPos > 0) {
                    QString key = part.left(eqPos);
                    QString val = part.mid(eqPos + 1);
                    info.plankopfAttributes[key] = val;
                }
            }
            // Plankopf-Stammdaten aus CSV drueberlagern (z.B. AG, AN, PR)
            if (!m_plankopfCsvData.isEmpty()) {
                for (auto it = m_plankopfCsvData.constBegin(); it != m_plankopfCsvData.constEnd(); ++it) {
                    info.plankopfAttributes[it.key()] = it.value();
                }
            }
            
            // ASP aus Ordnerhierarchie setzen (ueberschreibt evtl. vorhandenen Wert)
            if (!info.aspName.isEmpty()) {
                info.plankopfAttributes["ASP"] = folderDisplayName(info.aspName);
            } else {
                info.plankopfAttributes["ASP"] = QString();
            }
            continue;
        }
        
        // Skip other comment lines
        if (line.startsWith("#")) continue;
        
        QStringList fields = line.split(";");
        
        // First non-comment line is header
        if (!headerRead) {
            header = fields;
            headerRead = true;
            continue;
        }
        
        // Data line -> DataPoint
        DataPoint dp;
        if (fields.size() > 0) dp.bmk = fields[0];
        if (fields.size() > 1) dp.bezeichnung = fields[1];
        if (fields.size() > 2) dp.aks = fields[2];
        if (fields.size() > 3) dp.refDp = fields[3];
        if (fields.size() > 4) dp.fcodeDp = fields[4];
        if (fields.size() > 5) dp.basString = fields[5];
        if (fields.size() > 6) dp.integDp = fields[6];
        
        // Columns 7+ are the 60 function values (OC_x_x_x)
        for (int col = 7; col < fields.size() && col < header.size(); ++col) {
            QString colName = header[col];
            QString val = fields[col];
            if (!val.trimmed().isEmpty()) {
                dp.funktionsWerte[colName] = val;
            }
        }
        
        // Assign DP index based on order
        dp.dpIndex = info.dataPoints.size() + 1;
        info.dataPoints.append(dp);
    }
    
    file.close();
    
    // Calculate sheet count
    info.gaFlSheetCount = calculateSheetCount(info.dataPoints.size());
    
    log(QString("  %1: %2 DPs -> %3 Blaetter")
        .arg(info.fileName)
        .arg(info.dataPoints.size())
        .arg(info.gaFlSheetCount));
    
    return info;
}

// ============================================================================
// Phase 2: Read All Extracted Data
// ============================================================================

QVector<SourceDrawingInfo> OpenCirtTab::readExtractedData(const QStringList& dwgFiles) {
    QVector<SourceDrawingInfo> drawings;
    
    if (m_extractTempDir.isEmpty()) {
        m_extractTempDir = QStandardPaths::writableLocation(QStandardPaths::TempLocation)
                           + "/OpenCirt_extract";
    }
    
    log(QString("Lese extrahierte Daten aus: %1").arg(m_extractTempDir));
    
    int totalDp = 0;
    int totalSheets = 0;
    
    // Mirror Zeichnungen folder structure to find CSVs (matches generateExtractionScr)
    QString drawingsDir = projectPath(OpenCirtConfig::ZEICHNUNGEN_DIR);
    QDir drawingsRoot(drawingsDir);
    
    for (const QString& dwg : dwgFiles) {
        QString baseName = QFileInfo(dwg).completeBaseName();
        QString relPath = drawingsRoot.relativeFilePath(QFileInfo(dwg).absolutePath());
        QString csvPath = m_extractTempDir + "/" + relPath + "/" + baseName + ".csv";
        
        if (!QFileInfo::exists(csvPath)) {
            // No CSV means no active DPs in this DWG - skip silently
            continue;
        }
        
        SourceDrawingInfo info = parseExtractedCsv(csvPath, dwg);
        
        if (!info.dataPoints.isEmpty()) {
            totalDp += info.dataPoints.size();
            totalSheets += info.gaFlSheetCount;
            drawings.append(info);
        }
    }
    
    log(QString("Extraktion gelesen: %1 Zeichnungen, %2 DPs, %3 GA-FL-Blaetter")
        .arg(drawings.size()).arg(totalDp).arg(totalSheets));
    
    return drawings;
}

// ============================================================================
// Phase 2: Read ODS Reference Data
// ============================================================================

QMap<QString, QVector<QString>> OpenCirtTab::readOdsReference() {
    QMap<QString, QVector<QString>> referenceData;
    
    QString csvPath = referencePath("GA_FL_VORLAGE.csv");
    if (!QFileInfo::exists(csvPath)) {
        logError(QString("Referenz-CSV nicht gefunden: %1").arg(csvPath));
        return referenceData;
    }
    
    QFile file(csvPath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        logError("Referenz-CSV nicht lesbar");
        return referenceData;
    }
    
    QTextStream stream(&file);
    stream.setEncoding(QStringConverter::Utf8);
    
    // Skip header line
    if (!stream.atEnd()) stream.readLine();
    
    while (!stream.atEnd()) {
        QString line = stream.readLine().trimmed();
        if (line.isEmpty()) continue;
        
        // LibreOffice CSV is comma-separated with optional quoting
        QVector<QString> fields;
        bool inQuotes = false;
        QString current;
        
        for (int i = 0; i < line.length(); ++i) {
            QChar ch = line[i];
            if (ch == '"') {
                inQuotes = !inQuotes;
            } else if (ch == ',' && !inQuotes) {
                fields.append(current.trimmed());
                current.clear();
            } else {
                current += ch;
            }
        }
        fields.append(current.trimmed());
        
        // Column B (index 1) = DP name (key for lookup)
        if (fields.size() > 1 && !fields[1].isEmpty()) {
            referenceData[fields[1]] = fields;
        }
    }
    
    file.close();
    log(QString("Referenz-CSV geladen: %1 Eintraege").arg(referenceData.size()));
    
    return referenceData;
}

// ============================================================================
// Phase 2: Generate GA-FL Creation/Fill SCR
// ============================================================================

QString OpenCirtTab::generateGaFlCreationScr(const QVector<SourceDrawingInfo>& drawings) {
    QString scr;
    
    QString vorlage = templatePath(OpenCirtConfig::GA_FL_VORLAGE_DWG);
    vorlage.replace("\\", "/");
    
    QString fillLspPath = scriptsPath() + "/FillGaFl.lsp";
    fillLspPath.replace("\\", "/");
    
    QString refCsvPath = referencePath("GA_FL_VORLAGE.csv");
    refCsvPath.replace("\\", "/");
    
    if (m_extractTempDir.isEmpty()) {
        m_extractTempDir = QStandardPaths::writableLocation(QStandardPaths::TempLocation)
                           + "/OpenCirt_extract";
    }
    QString tempDir = m_extractTempDir;
    tempDir.replace("\\", "/");
    
    int totalSheets = 0;
    
    // Ensure batch mode system variables at SCR start
    scr += "; --- Batch-Modus Systemvariablen ---\n";
    scr += "(progn (setvar \"FILEDIA\" 0)(princ))\n";
    scr += "(progn (setvar \"CMDECHO\" 0)(princ))\n";
    scr += "(progn (setvar \"EXPERT\" 5)(princ))\n";
    
    // Mirror Zeichnungen folder structure to find CSVs (matches generateExtractionScr)
    QString drawingsDir = projectPath(OpenCirtConfig::ZEICHNUNGEN_DIR);
    QDir drawingsRoot(drawingsDir);
    
    for (const SourceDrawingInfo& drawing : drawings) {
        if (drawing.dataPoints.isEmpty()) continue;
        
        int dpCount = drawing.dataPoints.size();
        int sheetCount = drawing.gaFlSheetCount;
        
        // Source CSV path: mirrored subdirectory structure
        QString relPath = drawingsRoot.relativeFilePath(QFileInfo(drawing.filePath).absolutePath());
        QString srcCsvPath = tempDir + "/" + relPath + "/" + drawing.fileName + ".csv";
        srcCsvPath.replace("\\", "/");
        
        // Target folder for GA-FL sheets = same as source DWG folder
        QString targetFolder = QFileInfo(drawing.filePath).absolutePath();
        targetFolder.replace("\\", "/");
        
        int dpOffset = 0;  // Running offset into the datapoint list
        
        for (int sheet = 1; sheet <= sheetCount; ++sheet) {
            bool isFirstSheet = (sheet == 1);
            int maxDpThisSheet = isFirstSheet 
                ? OpenCirtConfig::MAX_DP_FIRST_SHEET 
                : OpenCirtConfig::MAX_DP_FOLLOW_SHEET;
            int dpThisSheet = qMin(maxDpThisSheet, dpCount - dpOffset);
            
            // GA-FL filename: <SourceName>_GA_FL_<SheetNum>.dwg
            QString gaFlName = QString("%1_GA_FL_%2.dwg")
                               .arg(drawing.fileName)
                               .arg(sheet, 2, 10, QChar('0'));
            QString gaFlPath = targetFolder + "/" + gaFlName;
            
            scr += QString("; --- %1 Blatt %2/%3 (%4 DPs) ---\n")
                   .arg(drawing.fileName).arg(sheet).arg(sheetCount).arg(dpThisSheet);
            
            // Copy template to target, then open the copy
            scr += QString("(progn (vl-file-copy \"%1\" \"%2\" T)(princ))\n")
                   .arg(vorlage, gaFlPath);
            scr += QString("_.OPEN \"%1\"\n").arg(gaFlPath);
            
            // Log-Pfad setzen (muss nach jedem _.OPEN neu, da LISP-Namespace pro Dokument)
            {
                QString logPath = tempDir + "/fillgafl_log.txt";
                scr += QString("(progn (setq *oc-log-path* \"%1\")(princ))\n").arg(logPath);
            }
            
            // Freeze non-content layers
            scr += "(progn\n"
                   "  (setq layers (vla-get-Layers (vla-get-ActiveDocument (vlax-get-acad-object))))\n"
                   "  (foreach ln '(\"GA-Konstruktionslinie\" \"GA-MBE-Grafik\")\n"
                   "    (if (tblsearch \"LAYER\" ln)\n"
                   "      (progn\n"
                   "        (setq lo (vla-item layers ln))\n"
                   "        (vla-put-Freeze lo :vlax-true)\n"
                   "      )\n"
                   "    )\n"
                   "  )\n"
                   "  (princ)\n"
                   ")\n";
            
            // Load FillGaFl.lsp
            scr += QString("(progn (load \"%1\")(princ))\n").arg(fillLspPath);
            
            // Set global variables for FillGaFl
            scr += QString("(progn (setq *oc-fill-csv-path* \"%1\")(princ))\n")
                   .arg(srcCsvPath);
            scr += QString("(progn (setq *oc-fill-ref-csv-path* \"%1\")(princ))\n")
                   .arg(refCsvPath);
            scr += QString("(progn (setq *oc-fill-sheet-num* %1)(princ))\n")
                   .arg(sheet);
            scr += QString("(progn (setq *oc-fill-start-row* %1)(princ))\n")
                   .arg(dpOffset);
            scr += QString("(progn (setq *oc-fill-dp-count* %1)(princ))\n")
                   .arg(dpThisSheet);
            
            // Execute fill
            scr += "(progn (FillGaFl)(princ))\n";
            
            // Save and close
            scr += lispSave();
            scr += lispClose();
            
            dpOffset += dpThisSheet;
            totalSheets++;
        }
    }
    
    log(QString("GA-FL-Erzeugungs-SCR generiert: %1 Blaetter fuer %2 Zeichnungen")
        .arg(totalSheets).arg(drawings.size()));
    
    return scr;
}

// ============================================================================
// Phase 2: Generate Summary Sheet SCR
// ============================================================================

QString OpenCirtTab::generateSummarySheetScr(const QVector<SourceDrawingInfo>& drawings) {
    QString scr;
    
    QString vorlage = templatePath(OpenCirtConfig::GA_FL_VORLAGE_DWG);
    vorlage.replace("\\", "/");
    
    QString fillLspPath = scriptsPath() + "/FillGaFl.lsp";
    fillLspPath.replace("\\", "/");
    
    QString refCsvPath = referencePath("GA_FL_VORLAGE.csv");
    refCsvPath.replace("\\", "/");
    
    if (m_extractTempDir.isEmpty()) {
        m_extractTempDir = QStandardPaths::writableLocation(QStandardPaths::TempLocation)
                           + "/OpenCirt_extract";
    }
    QString tempDir = m_extractTempDir;
    tempDir.replace("\\", "/");
    
    // ================================================================
    // Step 1: Build per-ASP aggregation
    // ================================================================
    // Map: ASP-Name -> list of drawings in that ASP
    QMap<QString, QVector<const SourceDrawingInfo*>> aspMap;
    for (const SourceDrawingInfo& d : drawings) {
        QString aspKey = d.aspName.isEmpty() ? "_default" : d.aspName;
        aspMap[aspKey].append(&d);
    }
    
    // ================================================================
    // Step 2: Create merged CSV per ASP (combining all DPs)
    // ================================================================
    QStringList aspSumCsvPaths;
    int aspSumSheetTotal = 0;
    
    // Function bases (shared by ASP-Summe and Projekt-Summe)
    QStringList funcBases = {
        "OC_INTEG",
        "OC_1_1_1", "OC_1_1_2", "OC_1_1_3", "OC_1_1_4",
        "OC_1_2_1", "OC_1_2_2",
        "OC_1_3_1", "OC_1_3_2", "OC_1_3_3", "OC_1_3_4", "OC_1_3_5",
        "OC_2_1_1", "OC_2_1_2",
        "OC_2_2_1", "OC_2_2_2", "OC_2_2_3", "OC_2_2_4", "OC_2_2_5",
        "OC_2_2_6", "OC_2_2_7", "OC_2_2_8", "OC_2_2_9", "OC_2_2_10",
        "OC_2_2_11", "OC_2_2_12",
        "OC_2_3_1", "OC_2_3_2", "OC_2_3_3", "OC_2_3_4", "OC_2_3_5",
        "OC_2_3_6", "OC_2_3_7", "OC_2_3_8", "OC_2_3_9",
        "OC_2_4_1", "OC_2_4_2", "OC_2_4_3", "OC_2_4_4", "OC_2_4_5",
        "OC_2_5_1", "OC_2_5_2", "OC_2_5_3", "OC_2_5_4",
        "OC_2_6_1", "OC_2_6_2", "OC_2_6_3", "OC_2_6_4", "OC_2_6_5",
        "OC_2_7_1", "OC_2_7_2", "OC_2_7_3", "OC_2_7_4",
        "OC_3_1_1", "OC_3_1_2", "OC_3_1_3", "OC_3_1_4", "OC_3_1_5", "OC_3_1_6"
        // OC_4_1_1 entfernt: ist Kommentarspalte in ODS, keine GA-Funktion
    };
    int numFuncs = funcBases.size(); // 57
    
    // Read reference CSV for function value aggregation
    QMap<QString, QVector<QString>> refData = readOdsReference();
    
    for (auto it = aspMap.constBegin(); it != aspMap.constEnd(); ++it) {
        QString aspName = it.key();
        const QVector<const SourceDrawingInfo*>& aspDrawings = it.value();
        
        // Group drawings by Gewerk within this ASP
        QMap<QString, QVector<const SourceDrawingInfo*>> gewerkMap;
        for (const SourceDrawingInfo* d : aspDrawings) {
            QString gw = d->gewerk;
            if (gw.isEmpty()) gw = "Unbekannt";
            gewerkMap[gw].append(d);
        }
        
        int gewerkCount = gewerkMap.size();
        if (gewerkCount == 0) continue;
        
        // ================================================================
        // Step 1.5: Gewerk-Summe (one row per Anlage within each Gewerk)
        // ================================================================
        {
            // Find ASP folder path for Gewerk folder resolution
            QString aspFolderPath;
            if (!aspDrawings.isEmpty()) {
                QDir aspSearch(aspDrawings.first()->aspFolder);
                QString drRoot = projectPath(OpenCirtConfig::ZEICHNUNGEN_DIR);
                while (aspSearch.absolutePath().length() > drRoot.length()) {
                    if (aspSearch.dirName().contains("ASP", Qt::CaseInsensitive) ||
                        aspSearch.dirName().contains("ISP", Qt::CaseInsensitive)) {
                        aspFolderPath = aspSearch.absolutePath();
                        break;
                    }
                    aspSearch.cdUp();
                }
            }
            
            for (auto gwIt = gewerkMap.constBegin(); gwIt != gewerkMap.constEnd(); ++gwIt) {
                QString gewerkName = gwIt.key();
                const QVector<const SourceDrawingInfo*>& gwDrawings = gwIt.value();
                
                // Group drawings by Anlage within this Gewerk
                QMap<QString, QVector<const SourceDrawingInfo*>> anlageMap;
                for (const SourceDrawingInfo* d : gwDrawings) {
                    QString anl = d->anlage;
                    if (anl.isEmpty()) anl = gewerkName;  // DWG directly in Gewerk folder
                    anlageMap[anl].append(d);
                }
                
                int anlageCount = anlageMap.size();
                if (anlageCount < 2) continue;  // Skip Gewerk-Summe for single-Anlage Gewerke
                
                // Find Gewerk folder path from ASP folder
                QString gewerkFolderPath;
                if (!aspFolderPath.isEmpty()) {
                    QDir aspDir(aspFolderPath);
                    QStringList subDirs = aspDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
                    for (const QString& sub : subDirs) {
                        if (folderDisplayName(sub) == gewerkName) {
                            gewerkFolderPath = aspFolderPath + "/" + sub;
                            break;
                        }
                    }
                }
                if (gewerkFolderPath.isEmpty()) continue;
                gewerkFolderPath.replace("\\", "/");
                
                // Create Gewerk-Summe CSV: one row per Anlage
                QString gwCsvPath = tempDir + "/" + aspName + "_" + gewerkName + "_Summe.csv";
                QFile gwCsvFile(gwCsvPath);
                if (!gwCsvFile.open(QIODevice::WriteOnly | QIODevice::Text)) continue;
                
                QTextStream gwStream(&gwCsvFile);
                gwStream.setEncoding(QStringConverter::Utf8);
                
                // Header
                gwStream << "BMK;BEZEICHNUNG;AKS;REF_DP;FCODE_DP;BAS_DP;INTEG_DP";
                for (const QString& fb : funcBases) {
                    gwStream << ";" << fb;
                }
                gwStream << "\n";
                
                // Plankopf
                if (!gwDrawings.isEmpty()) {
                    QString aspDisplayForPk = folderDisplayName(aspName);
                    gwStream << "#PLANKOPF";
                    const QMap<QString, QString>& pk = gwDrawings.first()->plankopfAttributes;
                    for (auto pIt = pk.constBegin(); pIt != pk.constEnd(); ++pIt) {
                        if (pIt.key() == "ANLAGE") {
                            gwStream << ";" << pIt.key() << "=";  // empty
                        } else if (pIt.key() == "ASP") {
                            gwStream << ";ASP=" << aspDisplayForPk;
                        } else if (pIt.key() == "GEWERK") {
                            gwStream << ";GEWERK=" << gewerkName;
                        } else if (pIt.key() == "ZEICHNUNGSNUMMER") {
                            gwStream << ";ZEICHNUNGSNUMMER=" << gewerkName << " Summe";
                        } else {
                            gwStream << ";" << pIt.key() << "=" << pIt.value();
                        }
                    }
                    gwStream << "\n";
                }
                
                // One row per Anlage
                for (auto anIt = anlageMap.constBegin(); anIt != anlageMap.constEnd(); ++anIt) {
                    QString anlageName = anIt.key();
                    const QVector<const SourceDrawingInfo*>& anDrawings = anIt.value();
                    
                    QVector<int> funcCounts(numFuncs, 0);
                    for (const SourceDrawingInfo* d : anDrawings) {
                        for (const DataPoint& dp : d->dataPoints) {
                            if (!dp.refDp.isEmpty() && refData.contains(dp.refDp)) {
                                const QVector<QString>& refRow = refData[dp.refDp];
                                for (int fc = 0; fc < numFuncs && (fc + 2) < refRow.size(); ++fc) {
                                    QString cellVal = refRow[fc + 2].trimmed();
                                    if (!cellVal.isEmpty()) {
                                        bool ok;
                                        int numVal = cellVal.toInt(&ok);
                                        funcCounts[fc] += (ok && numVal > 0) ? numVal : 1;
                                    }
                                }
                            }
                        }
                    }
                    
                    gwStream << anlageName << ";"
                             << ";"  // BEZEICHNUNG leer (vermeidet Dopplung)
                             << anlageName << ";"
                             << ";" << ";" << ";";
                    for (int fc = 0; fc < numFuncs; ++fc) {
                        if (fc == 0) {
                            gwStream << ";";  // OC_INTEG: empty
                        } else {
                            gwStream << ";" << (funcCounts[fc] > 0 ? QString::number(funcCounts[fc]) : "");
                        }
                    }
                    gwStream << "\n";
                }
                gwCsvFile.close();
                log(QString("Gewerk-Summe CSV %1/%2: %3 Anlagen").arg(aspName, gewerkName).arg(anlageCount));
                
                // Generate Gewerk-Summe DWG sheets
                QString gwCsvSlash = gwCsvPath;
                gwCsvSlash.replace("\\", "/");
                int gwSheetCount = calculateSheetCount(anlageCount);
                int gwDpOffset = 0;
                
                for (int sheet = 1; sheet <= gwSheetCount; ++sheet) {
                    bool isFirst = (sheet == 1);
                    int maxDp = isFirst ? OpenCirtConfig::MAX_DP_FIRST_SHEET : OpenCirtConfig::MAX_DP_FOLLOW_SHEET;
                    int dpThis = qMin(maxDp, anlageCount - gwDpOffset);
                    
                    QString sumName = QString("0001 %1_Summe_%2.dwg")
                                      .arg(gewerkName)
                                      .arg(sheet, 2, 10, QChar('0'));
                    QString sumPath = gewerkFolderPath + "/" + sumName;
                    
                    scr += QString("; --- Gewerk-Summe: %1/%2 Blatt %3 (%4 Anlagen) ---\n")
                           .arg(aspName, gewerkName).arg(sheet).arg(dpThis);
                    scr += QString("(progn (vl-file-copy \"%1\" \"%2\" T)(princ))\n")
                           .arg(vorlage, sumPath);
                    scr += QString("_.OPEN \"%1\"\n").arg(sumPath);
                    scr += QString("(progn (setq *oc-log-path* \"%1/fillgafl_log.txt\")(princ))\n").arg(tempDir);
                    scr += QString("(progn (load \"%1\")(princ))\n").arg(fillLspPath);
                    scr += QString("(progn (setq *oc-fill-csv-path* \"%1\")(princ))\n").arg(gwCsvSlash);
                    scr += QString("(progn (setq *oc-fill-ref-csv-path* \"%1\")(princ))\n").arg(refCsvPath);
                    scr += QString("(progn (setq *oc-fill-sheet-num* %1)(princ))\n").arg(sheet);
                    scr += QString("(progn (setq *oc-fill-start-row* %1)(princ))\n").arg(gwDpOffset);
                    scr += QString("(progn (setq *oc-fill-dp-count* %1)(princ))\n").arg(dpThis);
                    scr += "(progn (FillGaFl)(princ))\n";
                    scr += lispSave();
                    scr += lispClose();
                    
                    gwDpOffset += dpThis;
                }
            }
        }
        
        // Create ASP summary CSV: one row per Gewerk with aggregated counts
        QString aspCsvPath = tempDir + "/" + aspName + "_Summe.csv";
        QFile aspCsvFile(aspCsvPath);
        if (!aspCsvFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
            logError(QString("ASP-Summen-CSV nicht schreibbar: %1").arg(aspCsvPath));
            continue;
        }
        
        QTextStream aspStream(&aspCsvFile);
        aspStream.setEncoding(QStringConverter::Utf8);
        
        // Header
        aspStream << "BMK;BEZEICHNUNG;AKS;REF_DP;FCODE_DP;BAS_DP;INTEG_DP";
        for (const QString& fb : funcBases) {
            aspStream << ";" << fb;
        }
        aspStream << "\n";
        
        // Plankopf: GEWERK and ANLAGE empty, ASP = display name, ZEICHNUNGSNUMMER = "ASPxx Summe"
        if (!aspDrawings.isEmpty()) {
            QString aspDisplayForPk = folderDisplayName(aspName);
            aspStream << "#PLANKOPF";
            const QMap<QString, QString>& pk = aspDrawings.first()->plankopfAttributes;
            for (auto pIt = pk.constBegin(); pIt != pk.constEnd(); ++pIt) {
                if (pIt.key() == "GEWERK" || pIt.key() == "ANLAGE") {
                    aspStream << ";" << pIt.key() << "=";  // empty
                } else if (pIt.key() == "ASP") {
                    aspStream << ";ASP=" << aspDisplayForPk;
                } else if (pIt.key() == "ZEICHNUNGSNUMMER") {
                    aspStream << ";ZEICHNUNGSNUMMER=" << aspDisplayForPk << " Summe";
                } else {
                    aspStream << ";" << pIt.key() << "=" << pIt.value();
                }
            }
            aspStream << "\n";
        }
        
        // One row per Gewerk with aggregated function counts
        for (auto gwIt = gewerkMap.constBegin(); gwIt != gewerkMap.constEnd(); ++gwIt) {
            QString gewerkName = gwIt.key();
            const QVector<const SourceDrawingInfo*>& gwDrawings = gwIt.value();
            
            QVector<int> funcCounts(numFuncs, 0);
            
            for (const SourceDrawingInfo* d : gwDrawings) {
                for (const DataPoint& dp : d->dataPoints) {
                    if (!dp.refDp.isEmpty() && refData.contains(dp.refDp)) {
                        const QVector<QString>& refRow = refData[dp.refDp];
                        for (int fc = 0; fc < numFuncs && (fc + 2) < refRow.size(); ++fc) {
                            QString cellVal = refRow[fc + 2].trimmed();
                            if (!cellVal.isEmpty()) {
                                bool ok;
                                int numVal = cellVal.toInt(&ok);
                                funcCounts[fc] += (ok && numVal > 0) ? numVal : 1;
                            }
                        }
                    }
                }
            }
            
            // BMK=Gewerk, BEZ=Gewerk, AKS=Gewerk
            // REF_DP/FCODE/BAS/INTEG empty
            aspStream << gewerkName << ";"
                      << ";"  // BEZEICHNUNG leer (vermeidet Dopplung)
                      << gewerkName << ";"
                      << ";" << ";" << ";";
            
            // OC_INTEG (fc==0) empty, rest = counts
            for (int fc = 0; fc < numFuncs; ++fc) {
                if (fc == 0) {
                    aspStream << ";";  // OC_INTEG: leer
                } else {
                    aspStream << ";" << (funcCounts[fc] > 0 ? QString::number(funcCounts[fc]) : "");
                }
            }
            aspStream << "\n";
        }
        
        aspCsvFile.close();
        log(QString("ASP-Summe CSV %1: %2 Gewerke").arg(aspName).arg(gewerkCount));
        
        // Determine ASP summary folder
        QString aspFolder;
        if (!aspDrawings.isEmpty()) {
            QDir dir(aspDrawings.first()->aspFolder);
            QString drawingsRoot = projectPath(OpenCirtConfig::ZEICHNUNGEN_DIR);
            while (dir.absolutePath().length() > drawingsRoot.length()) {
                if (dir.dirName().contains("ASP", Qt::CaseInsensitive) ||
                    dir.dirName().contains("ISP", Qt::CaseInsensitive)) {
                    aspFolder = dir.absolutePath();
                    break;
                }
                dir.cdUp();
            }
        }
        if (aspFolder.isEmpty()) {
            aspFolder = projectPath(OpenCirtConfig::ZEICHNUNGEN_DIR);
        }
        aspFolder.replace("\\", "/");
        
        // Generate SCR for ASP summary sheets (based on Gewerk count)
        int aspSheetCount = calculateSheetCount(gewerkCount);
        int dpOffset = 0;
        QString aspCsvPathSlash = aspCsvPath;
        aspCsvPathSlash.replace("\\", "/");
        
        for (int sheet = 1; sheet <= aspSheetCount; ++sheet) {
            bool isFirstSheet = (sheet == 1);
            int maxDp = isFirstSheet
                ? OpenCirtConfig::MAX_DP_FIRST_SHEET
                : OpenCirtConfig::MAX_DP_FOLLOW_SHEET;
            int dpThisSheet = qMin(maxDp, gewerkCount - dpOffset);
            
            QString aspDisplayName = folderDisplayName(aspName);
            QString sumName = QString("0001 %1_Summe_%2.dwg")
                              .arg(aspDisplayName)
                              .arg(sheet, 2, 10, QChar('0'));
            QString sumPath = aspFolder + "/" + sumName;
            
            scr += QString("; --- ASP-Summe: %1 Blatt %2/%3 (%4 Gewerke) ---\n")
                   .arg(aspName).arg(sheet).arg(aspSheetCount).arg(dpThisSheet);
            
            scr += QString("(progn (vl-file-copy \"%1\" \"%2\" T)(princ))\n")
                   .arg(vorlage, sumPath);
            scr += QString("_.OPEN \"%1\"\n").arg(sumPath);
            scr += QString("(progn (setq *oc-log-path* \"%1/fillgafl_log.txt\")(princ))\n").arg(tempDir);
            scr += QString("(progn (load \"%1\")(princ))\n").arg(fillLspPath);
            scr += QString("(progn (setq *oc-fill-csv-path* \"%1\")(princ))\n")
                   .arg(aspCsvPathSlash);
            scr += QString("(progn (setq *oc-fill-ref-csv-path* \"%1\")(princ))\n")
                   .arg(refCsvPath);
            scr += QString("(progn (setq *oc-fill-sheet-num* %1)(princ))\n").arg(sheet);
            scr += QString("(progn (setq *oc-fill-start-row* %1)(princ))\n").arg(dpOffset);
            scr += QString("(progn (setq *oc-fill-dp-count* %1)(princ))\n").arg(dpThisSheet);
            scr += "(progn (FillGaFl)(princ))\n";
            scr += lispSave();
            scr += lispClose();
            
            dpOffset += dpThisSheet;
            aspSumSheetTotal++;
        }
        
        aspSumCsvPaths << aspCsvPath;
    }
    
    // ================================================================
    // Step 2.5: Los-Summe (one row per ASP within each Los)
    // ================================================================
    // Build Los map: Los folder path -> list of ASP names in that Los
    QMap<QString, QStringList> losAspMap;
    QMap<QString, QString> losFolderNames;  // Los folder path -> display name
    
    QString drawingsRootLos = projectPath(OpenCirtConfig::ZEICHNUNGEN_DIR);
    
    for (auto it = aspMap.constBegin(); it != aspMap.constEnd(); ++it) {
        QString aspName = it.key();
        const QVector<const SourceDrawingInfo*>& aspDrawings = it.value();
        if (aspDrawings.isEmpty()) continue;
        
        // Find ASP folder, then go one level up for Los
        QDir dir(aspDrawings.first()->aspFolder);
        while (dir.absolutePath().length() > drawingsRootLos.length()) {
            if (dir.dirName().contains("ASP", Qt::CaseInsensitive) ||
                dir.dirName().contains("ISP", Qt::CaseInsensitive)) {
                break;
            }
            dir.cdUp();
        }
        
        // dir is now at ASP level, go up one more for Los
        QDir losDir(dir);
        losDir.cdUp();
        QString losFolder = losDir.absolutePath();
        
        // Only if Los folder is between drawings root and ASP
        if (losFolder.length() > drawingsRootLos.length()) {
            losAspMap[losFolder].append(aspName);
            if (!losFolderNames.contains(losFolder)) {
                losFolderNames[losFolder] = folderDisplayName(losDir.dirName());
            }
        }
    }
    
    int losSumSheetTotal = 0;
    
    for (auto losIt = losAspMap.constBegin(); losIt != losAspMap.constEnd(); ++losIt) {
        QString losFolder = losIt.key();
        const QStringList& losAspNames = losIt.value();
        QString losDisplayName = losFolderNames[losFolder];
        
        // Count total ASPs in this Los (one row per ASP, like Projekt-Summe)
        int losAspCount = losAspNames.size();
        if (losAspCount == 0) continue;
        
        // Create Los summary CSV: one row per ASP with aggregated counts
        QString losCsvPath = tempDir + "/" + losDisplayName + "_Summe.csv";
        QFile losCsvFile(losCsvPath);
        if (!losCsvFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
            logError(QString("Los-Summen-CSV nicht schreibbar: %1").arg(losCsvPath));
            continue;
        }
        
        QTextStream losStream(&losCsvFile);
        losStream.setEncoding(QStringConverter::Utf8);
        
        // Header
        losStream << "BMK;BEZEICHNUNG;AKS;REF_DP;FCODE_DP;BAS_DP;INTEG_DP";
        for (const QString& fb : funcBases) {
            losStream << ";" << fb;
        }
        losStream << "\n";
        
        // Plankopf from first ASP's first drawing, GEWERK/ANLAGE empty, ZEICHNUNGSNUMMER override
        bool plankopfDone = false;
        for (const QString& aspName : losAspNames) {
            if (plankopfDone) break;
            const QVector<const SourceDrawingInfo*>& aspDrawings = aspMap[aspName];
            if (!aspDrawings.isEmpty()) {
                losStream << "#PLANKOPF";
                const auto& pk = aspDrawings.first()->plankopfAttributes;
                for (auto pIt = pk.constBegin(); pIt != pk.constEnd(); ++pIt) {
                    if (pIt.key() == "GEWERK" || pIt.key() == "ANLAGE" || pIt.key() == "ASP") {
                        losStream << ";" << pIt.key() << "=";  // empty
                    } else if (pIt.key() == "ZEICHNUNGSNUMMER") {
                        losStream << ";ZEICHNUNGSNUMMER=" << losDisplayName << " Summe";
                    } else {
                        losStream << ";" << pIt.key() << "=" << pIt.value();
                    }
                }
                losStream << "\n";
                plankopfDone = true;
            }
        }
        
        // One row per ASP with aggregated function counts
        for (const QString& aspName : losAspNames) {
            const QVector<const SourceDrawingInfo*>& aspDrawings = aspMap[aspName];
            
            QVector<int> funcCounts(numFuncs, 0);
            for (const SourceDrawingInfo* d : aspDrawings) {
                for (const DataPoint& dp : d->dataPoints) {
                    if (!dp.refDp.isEmpty() && refData.contains(dp.refDp)) {
                        const QVector<QString>& refRow = refData[dp.refDp];
                        for (int fc = 0; fc < numFuncs && (fc + 2) < refRow.size(); ++fc) {
                            QString cellVal = refRow[fc + 2].trimmed();
                            if (!cellVal.isEmpty()) {
                                bool ok;
                                int numVal = cellVal.toInt(&ok);
                                funcCounts[fc] += (ok && numVal > 0) ? numVal : 1;
                            }
                        }
                    }
                }
            }
            
            QString aspDisplayName = folderDisplayName(aspName);
            losStream << aspDisplayName << ";"
                      << ";"  // BEZEICHNUNG leer (vermeidet Dopplung)
                      << aspDisplayName << ";"
                      << ";" << ";" << ";";
            
            for (int fc = 0; fc < numFuncs; ++fc) {
                if (fc == 0) {
                    losStream << ";";  // OC_INTEG: empty
                } else {
                    losStream << ";" << (funcCounts[fc] > 0 ? QString::number(funcCounts[fc]) : "");
                }
            }
            losStream << "\n";
        }
        losCsvFile.close();
        log(QString("Los-Summe CSV %1: %2 ASPs").arg(losDisplayName).arg(losAspCount));
        
        // Generate Los-Summe sheets
        QString losFolderSlash = losFolder;
        losFolderSlash.replace("\\", "/");
        QString losCsvSlash = losCsvPath;
        losCsvSlash.replace("\\", "/");
        
        int losSheetCount = calculateSheetCount(losAspCount);
        int dpOffset = 0;
        
        for (int sheet = 1; sheet <= losSheetCount; ++sheet) {
            bool isFirstSheet = (sheet == 1);
            int maxDp = isFirstSheet
                ? OpenCirtConfig::MAX_DP_FIRST_SHEET
                : OpenCirtConfig::MAX_DP_FOLLOW_SHEET;
            int dpThisSheet = qMin(maxDp, losAspCount - dpOffset);
            
            QString sumName = QString("0001 %1_Summe_%2.dwg")
                              .arg(losDisplayName)
                              .arg(sheet, 2, 10, QChar('0'));
            QString sumPath = losFolderSlash + "/" + sumName;
            
            scr += QString("; --- Los-Summe: %1 Blatt %2/%3 (%4 ASPs) ---\n")
                   .arg(losDisplayName).arg(sheet).arg(losSheetCount).arg(dpThisSheet);
            
            scr += QString("(progn (vl-file-copy \"%1\" \"%2\" T)(princ))\n")
                   .arg(vorlage, sumPath);
            scr += QString("_.OPEN \"%1\"\n").arg(sumPath);
            scr += QString("(progn (setq *oc-log-path* \"%1/fillgafl_log.txt\")(princ))\n").arg(tempDir);
            scr += QString("(progn (load \"%1\")(princ))\n").arg(fillLspPath);
            scr += QString("(progn (setq *oc-fill-csv-path* \"%1\")(princ))\n")
                   .arg(losCsvSlash);
            scr += QString("(progn (setq *oc-fill-ref-csv-path* \"%1\")(princ))\n")
                   .arg(refCsvPath);
            scr += QString("(progn (setq *oc-fill-sheet-num* %1)(princ))\n").arg(sheet);
            scr += QString("(progn (setq *oc-fill-start-row* %1)(princ))\n").arg(dpOffset);
            scr += QString("(progn (setq *oc-fill-dp-count* %1)(princ))\n").arg(dpThisSheet);
            scr += "(progn (FillGaFl)(princ))\n";
            scr += lispSave();
            scr += lispClose();
            
            dpOffset += dpThisSheet;
            losSumSheetTotal++;
        }
    }
    
    // ================================================================
    // Step 3: Projekt_Summe (one row per Los with aggregated counts)
    // ================================================================
    // Hierarchy: ASP-Summe=per Gewerk, Los-Summe=per ASP, Projekt-Summe=per Los
    // If no Los structure exists, fall back to one row per ASP.
    
    int losCount = losAspMap.size();
    bool hasLosStructure = (losCount > 0);
    
    // Determine row count for Projekt-Summe
    int projektRowCount = 0;
    
    if (hasLosStructure) {
        projektRowCount = losCount;
    } else {
        // No Los folders found -> fall back to one row per ASP
        projektRowCount = aspMap.size();
    }
    
    if (projektRowCount > 0) {
        QString projektCsvPath = tempDir + "/Projekt_Summe.csv";
        QFile projektFile(projektCsvPath);
        if (projektFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream pStream(&projektFile);
            pStream.setEncoding(QStringConverter::Utf8);
            
            // Header
            pStream << "BMK;BEZEICHNUNG;AKS;REF_DP;FCODE_DP;BAS_DP;INTEG_DP";
            for (const QString& fb : funcBases) {
                pStream << ";" << fb;
            }
            pStream << "\n";
            
            // Plankopf from first drawing, GEWERK/ANLAGE empty, ZEICHNUNGSNUMMER = "Projekt Summe"
            if (!drawings.isEmpty()) {
                pStream << "#PLANKOPF";
                const auto& pk = drawings.first().plankopfAttributes;
                for (auto pkIt = pk.constBegin(); pkIt != pk.constEnd(); ++pkIt) {
                    if (pkIt.key() == "GEWERK" || pkIt.key() == "ANLAGE" || pkIt.key() == "ASP") {
                        pStream << ";" << pkIt.key() << "=";  // empty
                    } else if (pkIt.key() == "ZEICHNUNGSNUMMER") {
                        pStream << ";ZEICHNUNGSNUMMER=Projekt Summe";
                    } else {
                        pStream << ";" << pkIt.key() << "=" << pkIt.value();
                    }
                }
                pStream << "\n";
            }
            
            if (hasLosStructure) {
                // One row per Los with aggregated function counts
                for (auto losIt = losAspMap.constBegin(); losIt != losAspMap.constEnd(); ++losIt) {
                    QString losFolder = losIt.key();
                    const QStringList& losAspNames = losIt.value();
                    QString losDisplayName = losFolderNames[losFolder];
                    
                    QVector<int> funcCounts(numFuncs, 0);
                    
                    // Aggregate all DPs from all ASPs in this Los
                    for (const QString& aspName : losAspNames) {
                        const QVector<const SourceDrawingInfo*>& aspDrawings = aspMap[aspName];
                        for (const SourceDrawingInfo* d : aspDrawings) {
                            for (const DataPoint& dp : d->dataPoints) {
                                if (!dp.refDp.isEmpty() && refData.contains(dp.refDp)) {
                                    const QVector<QString>& refRow = refData[dp.refDp];
                                    for (int fc = 0; fc < numFuncs && (fc + 2) < refRow.size(); ++fc) {
                                        QString cellVal = refRow[fc + 2].trimmed();
                                        if (!cellVal.isEmpty()) {
                                            bool ok;
                                            int numVal = cellVal.toInt(&ok);
                                            funcCounts[fc] += (ok && numVal > 0) ? numVal : 1;
                                        }
                                    }
                                }
                            }
                        }
                    }
                    
                    pStream << losDisplayName << ";"
                            << ";"  // BEZEICHNUNG leer (vermeidet Dopplung)
                            << losDisplayName << ";"
                            << ";" << ";" << ";";
                    
                    for (int fc = 0; fc < numFuncs; ++fc) {
                        if (fc == 0) {
                            pStream << ";";  // OC_INTEG: empty
                        } else {
                            pStream << ";" << (funcCounts[fc] > 0 ? QString::number(funcCounts[fc]) : "");
                        }
                    }
                    pStream << "\n";
                }
                log(QString("Projekt-Summe CSV: %1 Lose").arg(losCount));
            } else {
                // Fallback: No Los structure -> one row per ASP
                for (auto it = aspMap.constBegin(); it != aspMap.constEnd(); ++it) {
                    QString aspName = it.key();
                    const QVector<const SourceDrawingInfo*>& aspDrawings = it.value();
                    
                    QVector<int> funcCounts(numFuncs, 0);
                    for (const SourceDrawingInfo* d : aspDrawings) {
                        for (const DataPoint& dp : d->dataPoints) {
                            if (!dp.refDp.isEmpty() && refData.contains(dp.refDp)) {
                                const QVector<QString>& refRow = refData[dp.refDp];
                                for (int fc = 0; fc < numFuncs && (fc + 2) < refRow.size(); ++fc) {
                                    QString cellVal = refRow[fc + 2].trimmed();
                                    if (!cellVal.isEmpty()) {
                                        bool ok;
                                        int numVal = cellVal.toInt(&ok);
                                        funcCounts[fc] += (ok && numVal > 0) ? numVal : 1;
                                    }
                                }
                            }
                        }
                    }
                    
                    QString aspDisplayName = folderDisplayName(aspName);
                    pStream << aspDisplayName << ";"
                            << ";"  // BEZEICHNUNG leer (vermeidet Dopplung)
                            << aspDisplayName << ";"
                            << ";" << ";" << ";";
                    
                    for (int fc = 0; fc < numFuncs; ++fc) {
                        if (fc == 0) {
                            pStream << ";";  // OC_INTEG: empty
                        } else {
                            pStream << ";" << (funcCounts[fc] > 0 ? QString::number(funcCounts[fc]) : "");
                        }
                    }
                    pStream << "\n";
                }
                log(QString("Projekt-Summe CSV: %1 ASPs (kein Los)").arg(aspMap.size()));
            }
            projektFile.close();
        }
        
        // Generate Projekt_Summe sheets
        QString drawingsDir = projectPath(OpenCirtConfig::ZEICHNUNGEN_DIR);
        drawingsDir.replace("\\", "/");
        
        QString projektCsvSlash = projektCsvPath;
        projektCsvSlash.replace("\\", "/");
        
        int projektSheets = calculateSheetCount(projektRowCount);
        int dpOffset = 0;
        
        for (int sheet = 1; sheet <= projektSheets; ++sheet) {
            bool isFirstSheet = (sheet == 1);
            int maxDp = isFirstSheet
                ? OpenCirtConfig::MAX_DP_FIRST_SHEET
                : OpenCirtConfig::MAX_DP_FOLLOW_SHEET;
            int dpThisSheet = qMin(maxDp, projektRowCount - dpOffset);
            
            QString sumName = QString("0001 Projekt_Summe_%1.dwg")
                              .arg(sheet, 2, 10, QChar('0'));
            QString sumPath = drawingsDir + "/" + sumName;
            
            scr += QString("; --- Projekt-Summe Blatt %1/%2 (%3 %4) ---\n")
                   .arg(sheet).arg(projektSheets).arg(dpThisSheet)
                   .arg(hasLosStructure ? "Lose" : "ASPs");
            
            scr += QString("(progn (vl-file-copy \"%1\" \"%2\" T)(princ))\n")
                   .arg(vorlage, sumPath);
            scr += QString("_.OPEN \"%1\"\n").arg(sumPath);
            scr += QString("(progn (setq *oc-log-path* \"%1/fillgafl_log.txt\")(princ))\n").arg(tempDir);
            scr += QString("(progn (load \"%1\")(princ))\n").arg(fillLspPath);
            scr += QString("(progn (setq *oc-fill-csv-path* \"%1\")(princ))\n")
                   .arg(projektCsvSlash);
            scr += QString("(progn (setq *oc-fill-ref-csv-path* \"%1\")(princ))\n")
                   .arg(refCsvPath);
            scr += QString("(progn (setq *oc-fill-sheet-num* %1)(princ))\n").arg(sheet);
            scr += QString("(progn (setq *oc-fill-start-row* %1)(princ))\n").arg(dpOffset);
            scr += QString("(progn (setq *oc-fill-dp-count* %1)(princ))\n").arg(dpThisSheet);
            scr += "(progn (FillGaFl)(princ))\n";
            scr += lispSave();
            scr += lispClose();
            
            dpOffset += dpThisSheet;
        }
        
        log(QString("Summen-SCR generiert: %1 ASP + %2 Los + %3 Projekt-Summen")
            .arg(aspSumSheetTotal).arg(losSumSheetTotal).arg(projektSheets));
    }
    
    return scr;
}

// ============================================================================
// Inhaltsverzeichnis Generation
// ============================================================================

// Forward declaration (defined below near DSD generation)
static QString readZeichnungsnummer(const QString& dwgPath);

QString OpenCirtTab::findInhaltBlockVorlage() {
    // Find OC_VORLAGE_EINTRAG_INHALT_DIN_A2*.dwg ignoring version
    QString vorlagenDir = projectPath(OpenCirtConfig::VORLAGEN_DIR);
    QDir dir(vorlagenDir);
    QStringList filters;
    filters << "OC_VORLAGE_EINTRAG_INHALT_DIN_A2*.dwg";
    QStringList matches = dir.entryList(filters, QDir::Files);
    if (!matches.isEmpty()) {
        QString path = vorlagenDir + "/" + matches.first();
        path.replace("\\", "/");
        return path;
    }
    return QString();
}

int OpenCirtTab::cleanupInhalt() {
    QString drawingsDir = projectPath(OpenCirtConfig::ZEICHNUNGEN_DIR);
    int deletedCount = 0;
    
    QDirIterator it(drawingsDir, QStringList() << "*_Inhalt_*.dwg" << "*_Inhalt_*.bak"
                    << "0000 Projekt_Inhalt*",
                    QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        QString path = it.next();
        if (QFile::remove(path)) {
            deletedCount++;
        }
    }
    
    if (deletedCount > 0)
        log(QString("Inhalt-Cleanup: %1 Dateien geloescht").arg(deletedCount));
    return deletedCount;
}

/**
 * @brief Build TOC entries from the ordered DWG list.
 *
 * Iterates all DWGs in publish order, skips Deckblatt/Summe/GA-FL/Inhalt,
 * and creates one TocEntry per content drawing. Group headers (Los, ASP,
 * Gewerk changes) are inserted as separate entries with only the group
 * field filled.
 *
 * Page numbers account for: Deckblatt(1) + Inhalt(tocPageCount) + Summe(n)
 * appearing before content.
 */
QVector<OpenCirtTab::TocEntry> OpenCirtTab::buildTocEntries(
    const QStringList& orderedDwgs, int tocPageCount)
{
    QVector<TocEntry> entries;
    QString drawingsDir = projectPath(OpenCirtConfig::ZEICHNUNGEN_DIR);
    drawingsDir.replace("\\", "/");
    
    QString lastLos, lastAsp, lastGewerk;
    
    for (int i = 0; i < orderedDwgs.size(); ++i) {
        QString dwgPath = orderedDwgs[i];
        dwgPath.replace("\\", "/");
        QString fileName = QFileInfo(dwgPath).completeBaseName();
        
        // Skip non-content files
        if (fileName.contains("_Deckblatt") || fileName.contains("_Summe_") ||
            fileName.contains("_GA_FL_") || fileName.contains("_Inhalt_") ||
            fileName.startsWith("0000 Projekt_Deckblatt") ||
            fileName.startsWith("0000 Projekt_Inhalt") ||
            fileName.startsWith("0001 Projekt_Summe")) {
            continue;
        }
        
        // Parse hierarchy from folder path
        // Full:  .../Zeichnungen/Los/ASP/Gewerk/Anlage/filename.dwg = 5 parts
        // Short: .../Zeichnungen/Los/ASP/Gewerk/filename.dwg = 4 parts
        QString relPath = dwgPath.mid(drawingsDir.length() + 1);
        QStringList parts = relPath.split("/");
        
        // Need at least: Los/ASP/Gewerk/filename.dwg = 4 parts
        if (parts.size() < 4) continue;
        
        QString losFolder = parts[0];       // "01 Los 1 Anlagenautomation"
        QString aspFolder = parts[1];       // "01 ASP01"
        QString gewerkFolder = parts[2];    // "01 RLT"
        
        // Anlage subfolder is optional (5+ parts means Anlage exists)
        QString anlageFolder;
        if (parts.size() >= 5) {
            anlageFolder = parts[3];        // "00 TKA1010"
        }
        
        QString losName = folderDisplayName(losFolder);
        QString aspName = folderDisplayName(aspFolder);
        QString gewerkName = folderDisplayName(gewerkFolder);
        QString anlageName = anlageFolder.isEmpty() ? QString() : folderDisplayName(anlageFolder);
        
        // Page number: position i in orderedDwgs, shifted by tocPageCount
        // because Inhalt pages are inserted between Deckblatt(pos 0) and rest
        // Final: Deckblatt(1), Inhalt(2..1+toc), then pos 1+ shifted by toc
        int pageNum = i + 1 + tocPageCount;
        
        // Insert group headers on change
        if (losName != lastLos) {
            TocEntry losEntry;
            losEntry.los = losName;
            entries.append(losEntry);
            lastLos = losName;
            lastAsp.clear();
            lastGewerk.clear();
        }
        
        if (aspName != lastAsp) {
            TocEntry aspEntry;
            aspEntry.asp = aspName;
            entries.append(aspEntry);
            lastAsp = aspName;
            lastGewerk.clear();
        }
        
        if (gewerkName != lastGewerk) {
            TocEntry gewerkEntry;
            gewerkEntry.gewerk = gewerkName;
            entries.append(gewerkEntry);
            lastGewerk = gewerkName;
        }
        
        // Content entry: Anlage + ZEICHNUNGSNUMMER + page
        TocEntry entry;
        entry.anlage = anlageName;
        
        // Read ZEICHNUNGSNUMMER from DWG, fallback to filename
        QString zeichnungsNr = readZeichnungsnummer(dwgPath);
        entry.zeichnungsNr = zeichnungsNr.isEmpty() ? fileName : zeichnungsNr;
        entry.seite = pageNum;
        
        entries.append(entry);
    }
    
    return entries;
}

/**
 * @brief Generate SCR to create Inhaltsverzeichnis DWG pages.
 *
 * For each page: copy DIN-A2 template, thaw Inhalt layers, freeze
 * Trennlinien layers, INSERT block rows at 15mm intervals from
 * (31,384,0) downward, set attributes via LISP.
 */
QString OpenCirtTab::generateInhaltScr(
    const QVector<TocEntry>& entries, int tocPageCount)
{
    QString scr;
    
    QString vorlage = findDeckblattVorlage();  // Same DIN-A2 template
    if (vorlage.isEmpty()) {
        logError("Inhaltsverzeichnis: DIN-A2 Vorlage nicht gefunden");
        return scr;
    }
    
    QString blockVorlage = findInhaltBlockVorlage();
    if (blockVorlage.isEmpty()) {
        logError("Inhaltsverzeichnis: Eintrags-Blockvorlage nicht gefunden");
        return scr;
    }
    
    QString drawingsDir = projectPath(OpenCirtConfig::ZEICHNUNGEN_DIR);
    drawingsDir.replace("\\", "/");
    
    static const int MAX_ROWS_PER_PAGE = 21;
    static const double START_Y = 384.0;
    static const double ROW_HEIGHT = 15.0;
    static const double INSERT_X = 31.0;
    
    int entryIdx = 0;
    
    for (int page = 1; page <= tocPageCount; ++page) {
        QString targetName = QString("0000 Projekt_Inhalt_%1.dwg")
                             .arg(page, 2, 10, QChar('0'));
        QString targetPath = drawingsDir + "/" + targetName;
        
        scr += QString("; --- Inhaltsverzeichnis Seite %1/%2 ---\n")
               .arg(page).arg(tocPageCount);
        
        // 1. Copy template
        scr += QString("(progn (vl-file-copy \"%1\" \"%2\" T)(princ))\n")
               .arg(vorlage, targetPath);
        
        // 2. Open
        scr += QString("_.OPEN \"%1\"\n").arg(targetPath);
        
        // 3. Layer management: thaw Inhalt layers, freeze Trennlinien + Deckblatt
        // Direct VLA property manipulation - reliable in SCR context
        scr += "(progn\n"
               "  (setq layers (vla-get-Layers (vla-get-ActiveDocument (vlax-get-acad-object))))\n"
               "  (foreach ln '(\"GA-Inhaltsverzeichnis-Beschriftung\"\n"
               "                \"GA-Inhaltsverzeichnis-Daten\")\n"
               "    (if (tblsearch \"LAYER\" ln)\n"
               "      (progn\n"
               "        (setq lo (vla-item layers ln))\n"
               "        (vla-put-Freeze lo :vlax-false)\n"
               "        (vla-put-LayerOn lo :vlax-true)\n"
               "      )\n"
               "    )\n"
               "  )\n"
               "  (foreach ln '(\"GA-Trennlinie-BAS\" \"GA-Trennlinie-LVB\"\n"
               "                \"GA-Trennlinie-Regeldiagramme\" \"GA-Trennlinie-Regelstruktur\"\n"
               "                \"GA-Deckblatt\" \"GA-Konstruktionslinie\"\n"
               "                \"GA-MBE-Grafik\" \"GA-Inhaltsverzeichnis-Rahmen\")\n"
               "    (if (tblsearch \"LAYER\" ln)\n"
               "      (progn\n"
               "        (setq lo (vla-item layers ln))\n"
               "        (vla-put-Freeze lo :vlax-true)\n"
               "      )\n"
               "    )\n"
               "  )\n"
               "  (princ)\n"
               ")\n";
        
        // 4. Set active layer + suppress attribute prompts during INSERT
        scr += "(progn (command \"_.LAYER\" \"S\" \"GA-Inhaltsverzeichnis-Daten\" \"\")(princ))\n";
        scr += "(progn (setvar \"ATTREQ\" 0)(princ))\n";
        scr += "(progn (setvar \"ATTDIA\" 0)(princ))\n";
        
        // 4b. Pre-load block definition: insert+delete dummy to avoid
        // BricsCAD first-external-INSERT offset issues (Units prompt etc.)
        scr += QString(
            "(progn\n"
            "  (setvar \"INSUNITS\" 0)\n"
            "  (setvar \"INSUNITSDEFSOURCE\" 0)\n"
            "  (setvar \"INSUNITSDEFTARGET\" 0)\n"
            "  (command \"_.INSERT\" \"%1\" \"0,0,0\" \"1\" \"1\" \"0\")\n"
            "  (command \"_.ERASE\" (entlast) \"\")\n"
            "  (princ)\n"
            ")\n"
        ).arg(blockVorlage);
        
        // 5. Insert rows for this page
        int rowsThisPage = qMin(MAX_ROWS_PER_PAGE, entries.size() - entryIdx);
        
        for (int row = 0; row < rowsThisPage && entryIdx < entries.size(); ++row, ++entryIdx) {
            const TocEntry& e = entries[entryIdx];
            double yPos = START_Y - (row * ROW_HEIGHT);
            
            // INSERT block (scale 1, rotation 0)
            // Multi-line format for SCR robustness with long paths
            scr += QString("_.INSERT \"%1\"\n").arg(blockVorlage);
            scr += QString("%1,%2,0\n")
                   .arg(INSERT_X, 0, 'f', 1)
                   .arg(yPos, 0, 'f', 1);
            scr += "1\n1\n0\n";
            
            // Set attributes via LISP on the last inserted entity
            // Build attribute value pairs
            QString lispSetAttrs;
            lispSetAttrs += "(progn\n";
            lispSetAttrs += "  (setq ent (entlast))\n";
            lispSetAttrs += "  (setq obj (vlax-ename->vla-object ent))\n";
            lispSetAttrs += "  (if (and (vlax-property-available-p obj 'HasAttributes)\n";
            lispSetAttrs += "           (= (vla-get-HasAttributes obj) :vlax-true))\n";
            lispSetAttrs += "    (foreach att (vlax-invoke obj 'GetAttributes)\n";
            lispSetAttrs += "      (cond\n";
            
            if (!e.los.isEmpty()) {
                QString escaped = e.los;
                escaped.replace("\\", "\\\\");
                escaped.replace("\"", "");
                lispSetAttrs += QString("        ((= (strcase (vla-get-TagString att)) \"OC_INHALT_LOS\")\n"
                                        "         (vla-put-TextString att \"%1\"))\n").arg(escaped);
            }
            if (!e.asp.isEmpty()) {
                lispSetAttrs += QString("        ((= (strcase (vla-get-TagString att)) \"OC_INHALT_ASP\")\n"
                                        "         (vla-put-TextString att \"%1\"))\n").arg(e.asp);
            }
            if (!e.gewerk.isEmpty()) {
                lispSetAttrs += QString("        ((= (strcase (vla-get-TagString att)) \"OC_INHALT_GEWERK\")\n"
                                        "         (vla-put-TextString att \"%1\"))\n").arg(e.gewerk);
            }
            if (!e.anlage.isEmpty()) {
                lispSetAttrs += QString("        ((= (strcase (vla-get-TagString att)) \"OC_INHALT_ANLAGE\")\n"
                                        "         (vla-put-TextString att \"%1\"))\n").arg(e.anlage);
            }
            if (!e.zeichnungsNr.isEmpty()) {
                QString escaped = e.zeichnungsNr;
                escaped.replace("\\", "\\\\");
                escaped.replace("\"", "");
                lispSetAttrs += QString("        ((= (strcase (vla-get-TagString att)) \"OC_INHALT_ZEICHNUNGSNUMMER\")\n"
                                        "         (vla-put-TextString att \"%1\"))\n").arg(escaped);
            }
            if (e.seite > 0) {
                lispSetAttrs += QString("        ((= (strcase (vla-get-TagString att)) \"OC_INHALT_SEITE\")\n"
                                        "         (vla-put-TextString att \"%1\"))\n").arg(e.seite);
            }
            
            lispSetAttrs += "      )\n";  // close cond
            lispSetAttrs += "    )\n";    // close foreach
            lispSetAttrs += "  )\n";      // close if
            lispSetAttrs += "  (princ)\n";
            lispSetAttrs += ")\n";
            
            scr += lispSetAttrs;
        }
        
        // 6. Restore attribute settings, save and close
        scr += "(progn (setvar \"ATTREQ\" 1)(princ))\n";
        scr += "(progn (setvar \"ATTDIA\" 1)(princ))\n";
        scr += lispSave();
        scr += lispClose();
    }
    
    log(QString("Inhalt-SCR: %1 Seiten, %2 Eintraege").arg(tocPageCount).arg(entries.size()));
    return scr;
}

// ============================================================================
// PDF Publish - DSD Generation & PUBLISH Command
// ============================================================================

/**
 * @brief Collect all DWGs in correct publish order.
 *
 * Order per folder level:
 *   1. Deckblatt (0000 prefix, non-Summe files)
 *   2. Subfolders recursively (sorted)
 *   3. Summe sheets (contain "_Summe")
 *
 * This ensures: Projekt_Deckblatt -> ASP contents -> Projekt_Summe
 * Within ASP:   ASP_Deckblatt -> Gewerk contents -> ASP_Summe
 * Within Gewerk: Gewerk_Deckblatt -> Source + GA-FL pairs
 */
QStringList OpenCirtTab::collectOrderedDwgsForPublish() {
    QStringList result;
    QString drawingsDir = projectPath(OpenCirtConfig::ZEICHNUNGEN_DIR);
    
    // Recursive lambda: process one folder level
    std::function<void(const QString&)> processFolder;
    processFolder = [&](const QString& folderPath) {
        QDir dir(folderPath);
        
        // 1. Collect DWGs in this folder, split into 3 groups:
        //    - Deckblatt files (0000 prefix, no _Summe) -> first
        //    - Content files (everything else, no _Summe) -> after Deckblatt
        //    - Summe files (contain _Summe) -> last at this level
        QStringList allDwgs = dir.entryList({"*.dwg"}, QDir::Files, QDir::Name | QDir::IgnoreCase);
        QStringList deckblattFiles;
        QStringList contentFiles;
        QStringList summeFiles;
        
        for (const QString& f : allDwgs) {
            if (f.contains("_Summe", Qt::CaseInsensitive)) {
                summeFiles << folderPath + "/" + f;
            } else if (f.startsWith("0000")) {
                deckblattFiles << folderPath + "/" + f;
            } else {
                contentFiles << folderPath + "/" + f;
            }
        }
        
        // 2. Deckblatt first (0000 prefix)
        result.append(deckblattFiles);
        
        // 3. Summe directly after Deckblatt
        result.append(summeFiles);
        
        // 4. Content files (sorted alphabetically)
        result.append(contentFiles);
        
        // 5. Recurse into subfolders (sorted)
        QStringList subDirs = dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name | QDir::IgnoreCase);
        for (const QString& sub : subDirs) {
            processFolder(folderPath + "/" + sub);
        }
    };
    
    processFolder(drawingsDir);
    return result;
}

/**
 * @brief Read the first paperspace layout name from a DWG via side-database.
 *
 * Opens the DWG as a read-only side-database (not in the editor),
 * iterates the layout dictionary, and returns the name of the
 * paperspace layout with the lowest tab order (i.e. the first tab).
 *
 * @param dwgPath Full path to the DWG file
 * @return Layout name (e.g. "DIN-A2"), or "Layout1" as fallback
 */
static QString getFirstPaperSpaceLayoutName(const QString& dwgPath)
{
    AcDbDatabase* pDb = new AcDbDatabase(false, false);
    std::wstring wpath = dwgPath.toStdWString();

    Acad::ErrorStatus es = pDb->readDwgFile(
        wpath.c_str(), AcDbDatabase::kForReadAndAllShare, false);
    if (es != Acad::eOk) {
        delete pDb;
        return QStringLiteral("Layout1");
    }

    AcDbDictionary* pLayoutDict = nullptr;
    es = pDb->getLayoutDictionary(pLayoutDict, AcDb::kForRead);
    if (es != Acad::eOk || !pLayoutDict) {
        delete pDb;
        return QStringLiteral("Layout1");
    }

    QString bestName = QStringLiteral("Layout1");
    int bestTab = INT_MAX;

    AcDbDictionaryIterator* pIter = pLayoutDict->newIterator();
    while (pIter && !pIter->done()) {
        AcDbObject* pObj = nullptr;
        if (pIter->getObject(pObj, AcDb::kForRead) == Acad::eOk && pObj) {
            AcDbLayout* pLayout = AcDbLayout::cast(pObj);
            if (pLayout) {
                const ACHAR* name = nullptr;
                pLayout->getLayoutName(name);
                if (name) {
                    QString qName = QString::fromWCharArray(name);
                    if (qName.compare(QStringLiteral("Model"), Qt::CaseInsensitive) != 0) {
                        int tab = pLayout->getTabOrder();
                        if (tab < bestTab) {
                            bestTab = tab;
                            bestName = qName;
                        }
                    }
                }
            }
            pObj->close();
        }
        pIter->next();
    }
    delete pIter;
    pLayoutDict->close();
    delete pDb;

    return bestName;
}

/**
 * @brief Read the ZEICHNUNGSNUMMER attribute from ModelSpace via side-database.
 *
 * Opens the DWG as a read-only side-database, iterates all block references
 * in ModelSpace, finds one whose block name starts with "OC_RSH_Plankopf_quer"
 * (version suffix ignored), and returns the value of the ZEICHNUNGSNUMMER attribute.
 *
 * @param dwgPath Full path to the DWG file
 * @return Attribute value, or empty string if not found
 */
static QString readZeichnungsnummer(const QString& dwgPath)
{
    AcDbDatabase* pDb = new AcDbDatabase(false, false);
    std::wstring wpath = dwgPath.toStdWString();

    Acad::ErrorStatus es = pDb->readDwgFile(
        wpath.c_str(), AcDbDatabase::kForReadAndAllShare, false);
    if (es != Acad::eOk) {
        delete pDb;
        return QString();
    }

    QString result;

    // Open ModelSpace block table record
    AcDbBlockTable* pBT = nullptr;
    es = pDb->getBlockTable(pBT, AcDb::kForRead);
    if (es != Acad::eOk || !pBT) {
        delete pDb;
        return QString();
    }

    AcDbBlockTableRecord* pMS = nullptr;
    es = pBT->getAt(ACDB_MODEL_SPACE, pMS, AcDb::kForRead);
    pBT->close();
    if (es != Acad::eOk || !pMS) {
        delete pDb;
        return QString();
    }

    // Iterate all entities in ModelSpace
    AcDbBlockTableRecordIterator* pIter = nullptr;
    pMS->newIterator(pIter);

    while (pIter && !pIter->done()) {
        AcDbEntity* pEnt = nullptr;
        if (pIter->getEntity(pEnt, AcDb::kForRead) == Acad::eOk && pEnt) {
            AcDbBlockReference* pRef = AcDbBlockReference::cast(pEnt);
            if (pRef) {
                // Get block name
                AcDbObjectId blockId = pRef->blockTableRecord();
                AcDbBlockTableRecord* pBlkRec = nullptr;
                if (acdbOpenObject(pBlkRec, blockId, AcDb::kForRead) == Acad::eOk && pBlkRec) {
                    const ACHAR* blkName = nullptr;
                    pBlkRec->getName(blkName);
                    QString qBlkName = blkName ? QString::fromWCharArray(blkName) : QString();
                    pBlkRec->close();

                    // Check if block name starts with "OC_RSH_Plankopf_quer" (ignore version)
                    if (qBlkName.startsWith(QStringLiteral("OC_RSH_Plankopf_quer"), Qt::CaseInsensitive)) {
                        // Iterate attributes
                        AcDbObjectIterator* pAttIter = pRef->attributeIterator();
                        while (pAttIter && !pAttIter->done()) {
                            AcDbObject* pAttObj = nullptr;
                            if (acdbOpenObject(pAttObj, pAttIter->objectId(), AcDb::kForRead) == Acad::eOk && pAttObj) {
                                AcDbAttribute* pAtt = AcDbAttribute::cast(pAttObj);
                                if (pAtt) {
                                    const ACHAR* tag = pAtt->tag();
                                    if (tag) {
                                        QString qTag = QString::fromWCharArray(tag);
                                        if (qTag.compare(QStringLiteral("ZEICHNUNGSNUMMER"), Qt::CaseInsensitive) == 0) {
                                            const ACHAR* val = pAtt->textString();
                                            if (val) {
                                                result = QString::fromWCharArray(val).trimmed();
                                            }
                                        }
                                    }
                                }
                                pAttObj->close();
                            }
                            pAttIter->step();
                        }
                        delete pAttIter;

                        // Found the Plankopf block, stop searching
                        if (!result.isEmpty()) {
                            pEnt->close();
                            break;
                        }
                    }
                }
            }
            pEnt->close();
        }
        pIter->step();
    }
    delete pIter;
    pMS->close();
    delete pDb;

    return result;
}

/**
 * @brief Generate a DSD (Drawing Set Description) file for PUBLISH command.
 *
 * DSD is a Windows INI-style format:
 *   [DWF6Version] / [DWF6MinorVersion]
 *   [DWF6Sheet:SheetName] per drawing
 *   [Target] with Type=6 for multi-sheet PDF
 *   [PdfOptions] with CreateBookmarks=TRUE
 *
 * For each DWG, the actual paperspace layout name is read from the file
 * via a side-database (not "Layout1" hardcoded).
 *
 * @param orderedDwgs List of DWG paths in desired publish order
 * @return Path to generated DSD file, or empty on error
 */
QString OpenCirtTab::generateDsdFile(const QStringList& orderedDwgs) {
    // Output PDF path: project root / 06- Plot / Projektname.pdf
    QString projektName = QDir(m_projectRoot).dirName();
    QString plotDir = m_projectRoot + "/06- Plot";
    QDir().mkpath(plotDir);  // Create if not exists
    QString pdfPath = plotDir + "/" + projektName + ".pdf";
    pdfPath.replace("/", "\\");
    
    QString outDir = plotDir;
    outDir.replace("/", "\\");
    
    QString dsd;
    dsd += "[DWF6Version]\n";
    dsd += "Ver=1\n";
    dsd += "[DWF6MinorVersion]\n";
    dsd += "MinorVer=1\n";
    
    int sheetIdx = 0;
    for (const QString& dwgPath : orderedDwgs) {
        QString fileName = QFileInfo(dwgPath).completeBaseName();
        QString dwgWin = dwgPath;
        dwgWin.replace("/", "\\");
        
        // Read actual layout name from DWG (e.g. "DIN-A2", "DIN-A1")
        QString layoutName = getFirstPaperSpaceLayoutName(dwgPath);
        
        // Read ZEICHNUNGSNUMMER attribute from ModelSpace Plankopf block
        // Use as bookmark title if available, fallback to filename
        QString zeichnungsNr = readZeichnungsnummer(dwgPath);
        QString displayName = zeichnungsNr.isEmpty() ? fileName : zeichnungsNr;
        
        // Sheet name: 4-digit sequential number + display name
        // Guarantees uniqueness in DSD (INI format: duplicate sections overwrite!)
        // Also serves as readable bookmark title in the PDF table of contents
        QString sheetName = QString("%1-%2")
            .arg(sheetIdx + 1, 4, 10, QChar('0'))
            .arg(displayName);
        
        dsd += QString("[DWF6Sheet:%1]\n").arg(sheetName);
        dsd += QString("DWG=%1\n").arg(dwgWin);
        dsd += QString("Layout=%1\n").arg(layoutName);
        dsd += "Setup=\n";
        dsd += QString("OriginalSheetPath=%1\n").arg(dwgWin);
        dsd += "Has Plot Port=0\n";
        dsd += "Has3DDWF=0\n";
        
        log(QString("  Sheet %1: %2 -> \"%3\" (Layout \"%4\")")
            .arg(sheetIdx + 1, 3).arg(fileName, displayName, layoutName));
        sheetIdx++;
    }
    
    // Target section: Type=6 = multi-sheet PDF
    dsd += "[Target]\n";
    dsd += "Type=6\n";
    dsd += QString("DWF=%1\n").arg(pdfPath);
    dsd += QString("OUT=%1\n").arg(outDir);
    dsd += "PWD=\n";
    
    // MRU sections (required by BricsCAD)
    dsd += "[MRU Local]\n";
    dsd += "MRU=0\n";
    dsd += "[MRU Sheet List]\n";
    dsd += "MRU=0\n";
    
    // PDF options: bookmarks = table of contents
    dsd += "[PdfOptions]\n";
    dsd += "IncludeHyperlinks=TRUE\n";
    dsd += "CreateBookmarks=TRUE\n";
    dsd += "CaptureFontsInDrawing=TRUE\n";
    dsd += "ConvertTextToGeometry=FALSE\n";
    
    // Write DSD to temp file
    QString tempPath = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    QString dsdPath = tempPath + "/OpenCirt_publish.dsd";
    
    QFile file(dsdPath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        logError(QString("DSD-Datei konnte nicht geschrieben werden: %1").arg(dsdPath));
        return QString();
    }
    
    QTextStream stream(&file);
    stream << dsd;
    file.close();
    
    log(QString("DSD-Datei erzeugt: %1 Blaetter, Ausgabe: %2")
        .arg(sheetIdx).arg(QFileInfo(pdfPath).fileName()));
    
    return dsdPath;
}

void OpenCirtTab::onPublishPdf() {
    QStringList errors;
    if (!validateProjectStructure(errors)) {
        QMessageBox::warning(this, "Projektstruktur",
            "Projektstruktur unvollstaendig:\n\n" + errors.join("\n"));
        return;
    }
    
    log("=== PDF PUBLIZIEREN ===");
    
    // Step 0: Cleanup old Inhalt DWGs
    cleanupInhalt();
    
    // Step 1: Collect DWGs WITHOUT Inhalt pages (they don't exist yet)
    QStringList orderedDwgs = collectOrderedDwgsForPublish();
    
    if (orderedDwgs.isEmpty()) {
        logError("Keine DWG-Dateien zum Publizieren gefunden");
        QMessageBox::warning(this, "PDF publizieren",
            "Keine DWG-Dateien im Zeichnungsordner gefunden.");
        return;
    }
    
    // Step 2: Calculate TOC size
    // First pass: count content entries + group headers to determine page count
    // We need a preliminary pass without tocPageCount to count entries,
    // then calculate tocPageCount from entry count.
    QVector<TocEntry> prelimEntries = buildTocEntries(orderedDwgs, 0);
    
    static const int MAX_ROWS_PER_PAGE = 21;
    int tocPageCount = 0;
    if (!prelimEntries.isEmpty()) {
        tocPageCount = (prelimEntries.size() + MAX_ROWS_PER_PAGE - 1) / MAX_ROWS_PER_PAGE;
    }
    
    // Step 3: Rebuild entries with correct page numbers (shifted by tocPageCount)
    QVector<TocEntry> entries = buildTocEntries(orderedDwgs, tocPageCount);
    
    // Recalculate in case group headers changed the page count
    int recalcPages = (entries.size() + MAX_ROWS_PER_PAGE - 1) / MAX_ROWS_PER_PAGE;
    if (recalcPages != tocPageCount) {
        tocPageCount = recalcPages;
        entries = buildTocEntries(orderedDwgs, tocPageCount);
    }
    
    // Show summary
    QString projektName = QDir(m_projectRoot).dirName();
    QString pdfName = projektName + ".pdf";
    QString plotDir = m_projectRoot + "/06- Plot";
    int totalPages = orderedDwgs.size() + tocPageCount;
    
    QMessageBox::StandardButton reply = QMessageBox::question(this,
        "PDF publizieren",
        QString("%1 Zeichnungen + %2 Inhaltsseiten = %3 Blaetter\n"
                "werden als Multi-Sheet PDF publiziert.\n\n"
                "Ausgabedatei: %4\n"
                "Speicherort: %5\n\n"
                "Schritt 1: Inhaltsverzeichnis erzeugen\n"
                "Schritt 2: PDF automatisch publizieren\n\n"
                "Fortfahren?")
        .arg(orderedDwgs.size())
        .arg(tocPageCount)
        .arg(totalPages)
        .arg(pdfName)
        .arg(plotDir),
        QMessageBox::Yes | QMessageBox::Cancel);
    
    if (reply != QMessageBox::Yes) return;
    
    log(QString("Inhaltsverzeichnis: %1 Eintraege auf %2 Seiten")
        .arg(entries.size()).arg(tocPageCount));
    
    if (tocPageCount > 0) {
        // Generate Inhalt SCR (asynchronous via _.SCRIPT)
        QString inhaltScr = generateInhaltScr(entries, tocPageCount);
        if (inhaltScr.trimmed().isEmpty()) {
            logError("Inhalt-SCR-Generierung fehlgeschlagen");
            return;
        }
        
        // Add system variable setup/restore + completion marker
        QString fullScr;
        fullScr += "(progn (setvar \"FILEDIA\" 0)(princ))\n";
        fullScr += "(progn (setvar \"CMDECHO\" 0)(princ))\n";
        fullScr += "(progn (setvar \"EXPERT\" 5)(princ))\n";
        fullScr += inhaltScr;
        fullScr += "(progn (setvar \"FILEDIA\" 1)(princ))\n";
        fullScr += "(progn (setvar \"CMDECHO\" 1)(princ))\n";
        fullScr += "(progn (setvar \"EXPERT\" 0)(princ))\n";
        
        // Completion marker
        QString tempPath = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
        m_publishMarkerPath = tempPath + "/OpenCirt_inhalt_complete.marker";
        QString markerSlash = m_publishMarkerPath;
        markerSlash.replace("\\", "/");
        QFile::remove(m_publishMarkerPath);  // Clean old marker
        
        fullScr += QString("(progn (setq f (open \"%1\" \"w\"))(write-line \"INHALT_COMPLETE\" f)(close f)(princ))\n")
                   .arg(markerSlash);
        
        // Store DWGs for later (publish after Inhalt is done)
        m_pendingPublishDwgs = orderedDwgs;
        
        // Execute SCR and start polling
        if (executeScrFile(fullScr, "Inhaltsverzeichnis")) {
            m_btnPublish->setEnabled(false);
            m_btnPublish->setText("Inhalt wird erstellt...");
            m_publishTimer->start();
            logSuccess("Inhaltsverzeichnis wird erzeugt. PDF-Publish startet automatisch.");
        }
    } else {
        // No TOC entries -> publish directly
        log("Keine Inhaltseintraege gefunden - publiziere ohne Inhaltsverzeichnis.");
        launchPublish(orderedDwgs);
    }
}

/**
 * @brief Poll for Inhalt SCR completion, then launch PDF publish.
 */
void OpenCirtTab::onPublishPollTimer() {
    if (m_publishMarkerPath.isEmpty()) {
        m_publishTimer->stop();
        return;
    }
    
    if (!QFile::exists(m_publishMarkerPath)) {
        return;  // Still running
    }
    
    // Inhalt generation complete
    m_publishTimer->stop();
    QFile::remove(m_publishMarkerPath);
    m_publishMarkerPath.clear();
    
    logSuccess("Inhaltsverzeichnis erzeugt.");
    
    // Re-collect DWGs (now includes the new Inhalt pages)
    QStringList finalDwgs = collectOrderedDwgsForPublish();
    
    log(QString("Publish-Reihenfolge (%1 Blaetter):").arg(finalDwgs.size()));
    QString drawingsDir = projectPath(OpenCirtConfig::ZEICHNUNGEN_DIR);
    for (int i = 0; i < finalDwgs.size(); ++i) {
        QString rel = finalDwgs[i];
        if (rel.startsWith(drawingsDir)) {
            rel = rel.mid(drawingsDir.length() + 1);
        }
        log(QString("  %1. %2").arg(i + 1, 3).arg(rel));
    }
    
    launchPublish(finalDwgs);
    
    m_btnPublish->setText("PDF publizieren");
    m_btnPublish->setEnabled(true);
    m_pendingPublishDwgs.clear();
}

/**
 * @brief Launch BricsCAD PDF publish with DSD file.
 */
void OpenCirtTab::launchPublish(const QStringList& orderedDwgs) {
    // Generate DSD file
    QString dsdPath = generateDsdFile(orderedDwgs);
    if (dsdPath.isEmpty()) {
        logError("DSD-Generierung fehlgeschlagen");
        return;
    }
    
    // Launch a separate BricsCAD instance with /pl switch (headless publish)
    QString bricscadExe = QCoreApplication::applicationFilePath();
    QString firstDwg = orderedDwgs.first();
    
    QString dsdPathWin = dsdPath;
    dsdPathWin.replace("/", "\\");
    QString dwgPathWin = firstDwg;
    dwgPathWin.replace("/", "\\");
    
    QStringList args;
    args << "/pl" << dwgPathWin << dsdPathWin;
    
    QString projektName = QDir(m_projectRoot).dirName();
    QString pdfName = projektName + ".pdf";
    
    log(QString("Starte BricsCAD-Instanz: %1").arg(bricscadExe));
    log(QString("  /pl \"%1\" \"%2\"").arg(dwgPathWin, dsdPathWin));
    
    bool started = QProcess::startDetached(bricscadExe, args);
    if (started) {
        logSuccess(QString("PDF-Publish gestartet in separater BricsCAD-Instanz - Ausgabe: %1").arg(pdfName));
    } else {
        logError("BricsCAD-Instanz konnte nicht gestartet werden");
    }
}

} // namespace BatchProcessing
