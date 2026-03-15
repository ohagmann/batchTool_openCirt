/**
 * @file MainWindow.h
 * @brief Main Window for Batch Processing Plugin - Professional Edition
 * @version 2.0.0
 */

#ifndef MAINWINDOW_H
#define MAINWINDOW_H

// WICHTIG: Platform header MUSS zuerst kommen!
#ifdef __linux__
#include "brx_platform_linux.h"
#else
#include "brx_platform_windows.h"
#endif

#include <QMainWindow>
#include <QDir>
#include <QDialog>
#include <QThread>
#include <memory>


QT_BEGIN_NAMESPACE
class QTextEdit;
class QPushButton;
class QProgressBar;
class QLineEdit;
class QCheckBox;
class QListWidget;
class QListWidgetItem;
class QSpinBox;
class QTabWidget;
class QLabel;
class QComboBox;
class QTableWidget;
class QGroupBox;
QT_END_NAMESPACE

namespace BatchProcessing {

// Forward declarations
class ProcessingOptions;
class BatchProcessingEngine;
class ProcessingStatistics;
class ProcessingProgressDialog;
class OpenCirtTab;

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

signals:
    void startProcessing();
    void stopProcessing();
    void pauseToggled(bool paused);
    void abortRequested();

private slots:
    // General Tab Slots
    void onBrowseFolder();
    void onRefreshFileList();
    void onBrowseBackupFolder();
    
    // Text Tab Slots
    void onAddReplacementRow();
    void onRemoveReplacementRow();
    void onImportReplacements();
    
    // Layer Tab Slots
    void onLayerEnableToggled(bool enabled);
    void onLayerOperationSelectionChanged();
    void addLayerOperation();
    void editLayerOperation();
    void removeLayerOperation();
    void clearLayerOperations();
    void moveLayerOperationUp();
    void moveLayerOperationDown();
    void previewLayerOperations();
    void addQuickDeleteOperation();
    void addQuickFreezeOperation();
    void addQuickColorOperation();
    void addQuickRenameOperation();
    void scanLayers();
    void showLayerStatistics();
    void exportLayerList();
    void selectAllLayers();
    void clearLayerSelection();
    void loadLayerStandard();
    
    // LISP Tab Slots
    void onLispEnableToggled(bool enabled);
    void addLispScripts();
    void removeLispScripts();
    void clearLispScripts();
    void moveLispUp();
    void moveLispDown();
    void validateLispScripts();
    void previewLispScript();
    void onLispItemChanged(QListWidgetItem* item);
    void updateLispStatus();
    
    // Main Control Slots
    void onStartProcessing();
    void onStopProcessing();
    void onProcessingFinished(bool success, const QString& summary);
    void onProgressUpdated(int current, int total, const QString& currentFile);
    void onFileProcessed(const QString& filePath, bool success, const QString& message);
    void onErrorOccurred(const QString& error);
    
    // Menu Slots
    void onClearLog();
    void onExportLog();
    void showAbout();

private:
    void setupUi();
    void createMenuBar();
    void createCentralWidget();
    void createStatusBar();
    void connectSignals();
    
    // Tab creation methods
    QWidget* createGeneralTab();
    QWidget* createTextTab();
    QWidget* createAttributeTab();
    QWidget* createLayerTab();
    QWidget* createLispTab(); // ✅ RESTORED FROM BACKUP
    QWidget* createOpenCirtTab();
    
    // Helper methods
    void updateUiState(bool processing);
    void logMessage(const QString& message, const QString& type = "INFO");
    std::unique_ptr<ProcessingOptions> getOptionsFromUi();
    void setOptionsToUi(const ProcessingOptions& options);
    void loadSettings();
    void saveSettings();
    void updateLayerOperationStatus();
    void updateLayerSelectionStatus();
    void createLayerOperationItem(const QString& operationType);
    QString showAutoCADColorGrid(QWidget* parent = nullptr, const QString& title = "Select Layer Color");

    // Backup functions
    bool createBackup(const QString& dwgFilePath);
    bool restoreFromBackup(const QString& dwgFilePath);
    bool validateBackupSettings();

    // File scanning helpers
    QStringList findDwgFilesRecursive(const QDir& dir, 
                                      const QStringList& includePatterns,
                                      const QStringList& excludePatterns,
                                      qint64& totalSize);
    QStringList findDwgFilesNonRecursive(const QDir& dir,
                                         const QStringList& includePatterns,
                                         const QStringList& excludePatterns,
                                         qint64& totalSize);
    QString formatFileSize(qint64 bytes);
    

    // ===== UI Elements =====
    
    // Main
    QTabWidget* m_tabWidget;
    QLabel* m_statusLabel;
    QTextEdit* m_logTextEdit;
    QProgressBar* m_progressBar;
    QPushButton* m_startButton;
    QPushButton* m_stopButton;
    
    // General Tab
    QLineEdit* m_folderEdit;
    QPushButton* m_browseButton;
    QCheckBox* m_recursiveCheck;
    QLineEdit* m_includeEdit;
    QLineEdit* m_excludeEdit;
    QCheckBox* m_backupCheck;
    QCheckBox* m_validateCheck;
    QCheckBox* m_logCheck;
    QListWidget* m_fileList;
    QCheckBox* m_deleteOldBackupsCheck;     
    QCheckBox* m_timestampBackupsCheck;     
    QComboBox* m_backupLocationCombo;       
    QLineEdit* m_customBackupFolderEdit;    
    QPushButton* m_browseBackupFolderButton;
    
    // Text Tab
    QCheckBox* m_textEnableCheck;
    QLineEdit* m_searchTextEdit;
    QLineEdit* m_replaceTextEdit;
    QCheckBox* m_caseSensitiveCheck;
    QCheckBox* m_wholeWordCheck;
    QCheckBox* m_regexCheck;
    QCheckBox* m_singleLineCheck;
    QCheckBox* m_multiLineCheck;
    QCheckBox* m_dimensionCheck;
    QCheckBox* m_leaderCheck;
    QTableWidget* m_replacementTable;
    
    // Attribute Tab
    QCheckBox* m_attributeEnableCheck;
    QLineEdit* m_blockNameEdit;
    QCheckBox* m_nestedBlocksCheck;
    QLineEdit* m_attributeTagEdit;
    QLineEdit* m_attributeSearchEdit;
    QLineEdit* m_attributeReplaceEdit;
    QCheckBox* m_invisibleAttributesCheck;
    QCheckBox* m_constantAttributesCheck;
    QCheckBox* m_verifyRequiredCheck;
    
    // Layer Tab
    QCheckBox* m_layerEnableCheck;
    QListWidget* m_layerOperationsWidget;
    QPushButton* m_layerAddOpButton;
    QPushButton* m_layerEditOpButton;
    QPushButton* m_layerRemoveOpButton;
    QPushButton* m_layerClearOpButton;
    QPushButton* m_layerUpButton;
    QPushButton* m_layerDownButton;
    QPushButton* m_layerPreviewButton;
    QLabel* m_layerOpStatusLabel;
    
    // Quick operations
    QPushButton* m_quickDeleteButton;
    QPushButton* m_quickFreezeButton;
    QPushButton* m_quickColorButton;
    QPushButton* m_quickRenameButton;
    
    // Analysis & discovery (EXPANDED - removed Standards & Validation section)
    QListWidget* m_layerListWidget;
    QPushButton* m_layerStatsButton;
    QPushButton* m_exportLayersButton;
    QPushButton* m_selectAllLayersButton;
    QPushButton* m_clearSelectionButton;
    QLabel* m_layerStatsLabel;
    
    // Store scanned layers for use in dialogs
    QStringList m_scannedLayers;
    
    // OpenCirt Tab
    OpenCirtTab* m_openCirtTab;
    
    // LISP Tab (cleaned for v5.1 in-process architecture)
    QCheckBox* m_lispEnableCheck;
    QListWidget* m_lispListWidget;
    QPushButton* m_lispAddButton;
    QPushButton* m_lispRemoveButton;
    QPushButton* m_lispClearButton;
    QPushButton* m_lispUpButton;
    QPushButton* m_lispDownButton;
    QPushButton* m_lispValidateButton;
    QPushButton* m_lispPreviewButton;
    QLabel* m_lispStatusLabel;
    QString m_lastLispDirectory;
    
    // Processing Engine
    std::unique_ptr<BatchProcessingEngine> m_engine;
    QThread* m_processingThread;
    ProcessingProgressDialog* m_progressDialog;
    bool m_isProcessing;
    bool m_isPaused;
};

/**
 * @brief Progress Dialog for Processing
 */
class ProcessingProgressDialog : public QDialog {
    Q_OBJECT
    
public:
    explicit ProcessingProgressDialog(QWidget* parent = nullptr);
    
    void setTotalFiles(int total);
    void updateProgress(int current, const QString& filename);
    void addDetail(const QString& detail);
    bool shouldAbort() const { return m_shouldAbort; }
    
signals:
    void pauseToggled(bool paused);
    void abortRequested();
    
private slots:
    void onPauseClicked();
    void onAbortClicked();
    void onBrowseBackupFolder();
    
private:
    void updateStats(int current);
    
    QLabel* m_currentFileLabel;
    QProgressBar* m_progressBar;
    QTextEdit* m_detailsText;
    QLabel* m_statsLabel;
    QPushButton* m_pauseButton;
    QPushButton* m_abortButton;
    bool m_shouldAbort;
    int m_totalFiles;
};

} // namespace BatchProcessing

#endif // MAINWINDOW_H
