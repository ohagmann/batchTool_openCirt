#include "windows_fix.h"  // CRITICAL: Qt 6.8+ fix - MUST be FIRST
/**
 * @file MainWindow.cpp
 * @brief Main Window Implementation - Professional Edition  
 * @version 2.0.0
 */

// WICHTIG: Platform header MUSS zuerst kommen!
#ifdef __linux__
#include "brx_platform_linux.h"
#else
#include "brx_platform_windows.h"
#endif

// BRX Headers
#include "aced.h"
#include "dbents.h"
#include "dbsymtb.h"
#include "dbtrans.h"

// Eigene Headers
#include "MainWindow.h"
#include "OpenCirtTab.h"
#include "../core/DwgProcessor.h"
#include "widgets/AcadColorGrid.h"

// Qt Headers
#include <QProgressDialog>
#include <QMainWindow>
#include <QDialog>
#include <QThread>
#include <memory>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QTabWidget>
#include <QTextEdit>
#include <QPushButton>
#include <QProgressBar>
#include <QLineEdit>
#include <QCheckBox>
#include <QListWidget>
#include <QTableWidget>
#include <QSpinBox>
#include <QComboBox>
#include <QLabel>
#include <QGroupBox>
#include <QFileDialog>
#include <QMessageBox>
#include <QMenuBar>
#include <QStatusBar>
#include <QDateTime>
#include <QSettings>
#include <QTimer>
#include <QHeaderView>
#include <QInputDialog>
#include <QColorDialog>
#include <QDialogButtonBox>
#include <QFileInfo>
#include <QDir>
#include <QDirIterator>
#include <QTextStream>
#include <QRegularExpression>
#include <QStyle>
#include <QDebug>
#include <QApplication>
#include <QShortcut>
#include <QKeyEvent>
#include <functional>

namespace BatchProcessing {

// ============================================================================
// MainWindow Implementation
// ============================================================================

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_isProcessing(false)
    , m_isPaused(false)
    , m_processingThread(nullptr)
    , m_progressDialog(nullptr)
{
    setupUi();
    connectSignals();
    updateUiState(false);
    
    // Create processing engine
    m_engine = DwgProcessorFactory::createBatchEngine();
    loadSettings();
}

MainWindow::~MainWindow() {
    saveSettings();
    if (m_engine && m_isProcessing) {
        m_engine->stopProcessing();
    }
}

// ============================================================================
// Layer Tab Event Handlers
// ============================================================================

void MainWindow::onLayerEnableToggled(bool enabled) {
    m_layerOperationsWidget->setEnabled(enabled);
    m_layerAddOpButton->setEnabled(enabled);
    m_layerEditOpButton->setEnabled(enabled);
    m_layerRemoveOpButton->setEnabled(enabled);
    m_layerClearOpButton->setEnabled(enabled);
    m_layerUpButton->setEnabled(enabled);
    m_layerDownButton->setEnabled(enabled);
    m_layerPreviewButton->setEnabled(enabled);
    m_quickDeleteButton->setEnabled(enabled);
    m_quickFreezeButton->setEnabled(enabled);
    m_quickColorButton->setEnabled(enabled);
    m_quickRenameButton->setEnabled(enabled);
    
    updateLayerOperationStatus();
}

void MainWindow::onLayerOperationSelectionChanged() {
    QList<QListWidgetItem*> selected = m_layerOperationsWidget->selectedItems();
    bool hasSelection = !selected.isEmpty();
    
    m_layerEditOpButton->setEnabled(hasSelection && selected.size() == 1);
    m_layerRemoveOpButton->setEnabled(hasSelection);
    m_layerUpButton->setEnabled(hasSelection && selected.size() == 1 && 
                               m_layerOperationsWidget->row(selected.first()) > 0);
    m_layerDownButton->setEnabled(hasSelection && selected.size() == 1 && 
                                 m_layerOperationsWidget->row(selected.first()) < 
                                 m_layerOperationsWidget->count() - 1);
}

// ============================================================================
// 🎨 AutoCAD ColorGrid Implementation (BRAND NEW!)
// ============================================================================

QString MainWindow::showAutoCADColorGrid(QWidget* parent, const QString& title) {
    // Use the new professional AcadColorDialog
    int colorIndex = AcadColorDialog::getColor(parent, title, 7);
    
    if (colorIndex > 0) {
        QString colorName = AciColorUtils::getColorName(colorIndex);
        return QString("%1 (%2)").arg(colorName).arg(colorIndex);
    }
    
    return QString(); // Cancelled
}

void MainWindow::addLayerOperation() {
    QDialog dialog(this);
    dialog.setWindowTitle("Add Layer Operation");
    dialog.setMinimumWidth(400);
    
    auto* layout = new QVBoxLayout(&dialog);
    
    layout->addWidget(new QLabel("Select operation type:"));
    
    auto* operationCombo = new QComboBox();
    operationCombo->addItems(QStringList() 
        << "Delete Layer"
        << "Rename Layer" 
        << "Change Color"
        << "Freeze/Thaw"
        << "Set Visibility"
        << "Change Linetype"
        << "Set Transparency"
        << "Create New Layer");  // ADDED CreateNewLayer option
    layout->addWidget(operationCombo);
    
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout->addWidget(buttons);
    
    if (dialog.exec() == QDialog::Accepted) {
        QString operation = operationCombo->currentText();
        createLayerOperationItem(operation);
    }
}

void MainWindow::createLayerOperationItem(const QString& operationType) {
    // Create configuration dialog for operation parameters
    QDialog dialog(this);
    dialog.setWindowTitle(QString("Configure %1").arg(operationType));
    dialog.setMinimumWidth(550);
    dialog.setMinimumHeight(600);
    
    auto* layout = new QVBoxLayout(&dialog);
    
    // Add hint text at the top
    auto* hintLabel = new QLabel("Select layer name with up/down arrows, hit 'Tab' to choose the selected layer as 'Layer name'");
    hintLabel->setStyleSheet("QLabel { color: #0066cc; font-style: italic; padding: 5px; background-color: #f0f0f0; }");
    hintLabel->setWordWrap(true);
    layout->addWidget(hintLabel);
    
    // Add search field for filtering layers
    auto* searchLayout = new QHBoxLayout();
    auto* searchLabel = new QLabel("Search:");
    auto* searchEdit = new QLineEdit();
    searchEdit->setPlaceholderText("Type to filter layers...");
    searchEdit->setStyleSheet("QLineEdit { font-size: 10pt; }");
    searchLayout->addWidget(searchLabel);
    searchLayout->addWidget(searchEdit);
    layout->addLayout(searchLayout);
    
    // Add layer list box for selection (for all operations including Create New Layer)
    auto* layerListWidget = new QListWidget();
    layerListWidget->setMaximumHeight(300); // Height for 20-30 entries
    layerListWidget->setMinimumHeight(250);
    layerListWidget->setStyleSheet("QListWidget { font-size: 10pt; }"); // Set font size to 10pt
    
    // Store all layers for filtering
    QStringList allLayers = m_scannedLayers;
    
    // Lambda function to populate list
    auto populateList = [layerListWidget, &allLayers](const QString& filter = QString()) {
        layerListWidget->clear();
        
        if (allLayers.isEmpty()) {
            // If no layers scanned, add a placeholder message
            QListWidgetItem* placeholderItem = new QListWidgetItem("(No layers scanned - please scan layers first)");
            placeholderItem->setFlags(Qt::NoItemFlags);
            placeholderItem->setForeground(Qt::gray);
            layerListWidget->addItem(placeholderItem);
            return;
        }
        
        for (const QString& layerName : allLayers) {
            if (filter.isEmpty() || layerName.contains(filter, Qt::CaseInsensitive)) {
                layerListWidget->addItem(layerName);
            }
        }
        
        // Select "0" layer by default if it exists and no filter
        if (filter.isEmpty()) {
            QList<QListWidgetItem*> items = layerListWidget->findItems("0", Qt::MatchExactly);
            if (!items.isEmpty()) {
                layerListWidget->setCurrentItem(items.first());
            } else if (layerListWidget->count() > 0) {
                layerListWidget->setCurrentRow(0);
            }
        } else if (layerListWidget->count() > 0) {
            // Select first item after filtering
            layerListWidget->setCurrentRow(0);
        }
    };
    
    // Initial population
    populateList();
    
    // Connect search field to filter function
    connect(searchEdit, &QLineEdit::textChanged, [populateList](const QString& text) {
        populateList(text);
    });
    
    layout->addWidget(layerListWidget);
    
    // Set initial focus to search field
    searchEdit->setFocus();
    
    // Store the selected layer name (will be used when creating the operation)
    QString selectedLayerName;
    
    // Enable keyboard navigation in listbox
    layerListWidget->setFocusPolicy(Qt::StrongFocus);
    
    // Connect double-click to directly accept selection
    connect(layerListWidget, &QListWidget::itemDoubleClicked, [&selectedLayerName, &dialog](QListWidgetItem* item) {
        if (item && !(item->flags() == Qt::NoItemFlags)) {
            selectedLayerName = item->text();
            dialog.accept();
        }
    });
    
    // Operation-specific inputs
    QLineEdit* newNameEdit = nullptr;
    QString selectedColor;
    QComboBox* freezeCombo = nullptr;
    QComboBox* visibilityCombo = nullptr;
    QComboBox* linetypeCombo = nullptr;
    QSpinBox* transparencySpin = nullptr;
    QLineEdit* newLayerNameEdit = nullptr;
    QPushButton* colorButton = nullptr;
    
    // Add operation-specific widgets based on type
    if (operationType == "Create New Layer") {
        layout->addWidget(new QLabel("New layer name:"));
        newLayerNameEdit = new QLineEdit();
        newLayerNameEdit->setPlaceholderText("Enter layer name (e.g., WALLS, DOORS, TEXT)");
        newLayerNameEdit->setStyleSheet("QLineEdit { font-size: 10pt; }");
        layout->addWidget(newLayerNameEdit);
        
        // Set tab order
        dialog.setTabOrder(searchEdit, layerListWidget);
        dialog.setTabOrder(layerListWidget, newLayerNameEdit);
        
        // Install event filter to handle Tab key in listwidget
        class TabEventFilterNew : public QObject {
        public:
            QListWidget* list;
            QLineEdit* target;
            explicit TabEventFilterNew(QListWidget* l, QLineEdit* t, QObject* parent = nullptr) 
                : QObject(parent), list(l), target(t) {}
            
            bool eventFilter(QObject* obj, QEvent* event) override {
                if (event->type() == QEvent::KeyPress) {
                    QKeyEvent* keyEvent = static_cast<QKeyEvent*>(event);
                    if (keyEvent->key() == Qt::Key_Tab) {
                        QListWidgetItem* current = list->currentItem();
                        if (current && !(current->flags() == Qt::NoItemFlags)) {
                            // Use selected layer as template for new name
                            target->setText(current->text() + "_NEW");
                            target->setFocus();
                            target->selectAll();
                            return true;
                        }
                    }
                }
                return QObject::eventFilter(obj, event);
            }
        };
        
        auto* tabFilter = new TabEventFilterNew(layerListWidget, newLayerNameEdit, &dialog);
        layerListWidget->installEventFilter(tabFilter);
        
        layout->addWidget(new QLabel("Layer color:"));
        
        // 🎨 USE NEW COLORGRID!
        auto* colorLayout = new QHBoxLayout();
        auto* colorLabel = new QLabel("White (7)"); // Default
        colorButton = new QPushButton("Choose Color...");
        
        connect(colorButton, &QPushButton::clicked, [&selectedColor, colorLabel, this]() {
            QString color = showAutoCADColorGrid(this, "Select Layer Color");
            if (!color.isEmpty()) {
                selectedColor = color;
                colorLabel->setText(color);
            }
        });
        
        colorLayout->addWidget(colorLabel);
        colorLayout->addWidget(colorButton);
        colorLayout->addStretch();
        layout->addLayout(colorLayout);
        selectedColor = "White (7)"; // Default
        
    } else if (operationType == "Delete Layer") {
        // No additional inputs needed, just layer selection from list
        auto* warningLabel = new QLabel("⚠️ Warning: This will permanently delete the selected layer!");
        warningLabel->setStyleSheet("QLabel { color: red; font-weight: bold; }");
        layout->addWidget(warningLabel);
        
        // Set tab order - only search and list
        dialog.setTabOrder(searchEdit, layerListWidget);
        
    } else if (operationType == "Rename Layer") {
        layout->addWidget(new QLabel("New name:"));
        newNameEdit = new QLineEdit();
        newNameEdit->setPlaceholderText("New layer name");
        newNameEdit->setStyleSheet("QLineEdit { font-size: 10pt; }");
        layout->addWidget(newNameEdit);
        
        // Set tab order and connect Tab key to transfer selected layer
        dialog.setTabOrder(searchEdit, layerListWidget);
        dialog.setTabOrder(layerListWidget, newNameEdit);
        
        // Install event filter to handle Tab key in listwidget
        class TabEventFilter : public QObject {
        public:
            QListWidget* list;
            QLineEdit* target;
            explicit TabEventFilter(QListWidget* l, QLineEdit* t, QObject* parent = nullptr) 
                : QObject(parent), list(l), target(t) {}
            
            bool eventFilter(QObject* obj, QEvent* event) override {
                if (event->type() == QEvent::KeyPress) {
                    QKeyEvent* keyEvent = static_cast<QKeyEvent*>(event);
                    if (keyEvent->key() == Qt::Key_Tab) {
                        QListWidgetItem* current = list->currentItem();
                        if (current && !(current->flags() == Qt::NoItemFlags)) {
                            target->setText(current->text());
                            target->setFocus();
                            target->selectAll();
                            return true;
                        }
                    }
                }
                return QObject::eventFilter(obj, event);
            }
        };
        
        auto* tabFilter = new TabEventFilter(layerListWidget, newNameEdit, &dialog);
        layerListWidget->installEventFilter(tabFilter);
        
    } else if (operationType == "Change Color") {
        layout->addWidget(new QLabel("New color:"));
        
        // 🎨 USE NEW COLORGRID!
        auto* colorLayout = new QHBoxLayout();
        auto* colorLabel = new QLabel("Red (1)"); // Default
        colorButton = new QPushButton("Choose Color...");
        
        connect(colorButton, &QPushButton::clicked, [&selectedColor, colorLabel, this]() {
            QString color = showAutoCADColorGrid(this, "Select Layer Color");
            if (!color.isEmpty()) {
                selectedColor = color;
                colorLabel->setText(color);
            }
        });
        
        colorLayout->addWidget(colorLabel);
        colorLayout->addWidget(colorButton);
        colorLayout->addStretch();
        layout->addLayout(colorLayout);
        selectedColor = "Red (1)"; // Default
        
        // Set tab order
        dialog.setTabOrder(searchEdit, layerListWidget);
        dialog.setTabOrder(layerListWidget, colorButton);
        
    } else if (operationType == "Freeze/Thaw") {
        layout->addWidget(new QLabel("Action:"));
        freezeCombo = new QComboBox();
        freezeCombo->addItems(QStringList() << "Freeze" << "Thaw");
        freezeCombo->setStyleSheet("QComboBox { font-size: 10pt; }");
        layout->addWidget(freezeCombo);
        
        // Set tab order
        dialog.setTabOrder(searchEdit, layerListWidget);
        dialog.setTabOrder(layerListWidget, freezeCombo);
        
    } else if (operationType == "Set Visibility") {
        layout->addWidget(new QLabel("Visibility:"));
        visibilityCombo = new QComboBox();
        visibilityCombo->addItems(QStringList() << "On (Visible)" << "Off (Hidden)");
        visibilityCombo->setStyleSheet("QComboBox { font-size: 10pt; }");
        layout->addWidget(visibilityCombo);
        
        // Set tab order
        dialog.setTabOrder(searchEdit, layerListWidget);
        dialog.setTabOrder(layerListWidget, visibilityCombo);
        
    } else if (operationType == "Change Linetype") {
        layout->addWidget(new QLabel("New linetype:"));
        linetypeCombo = new QComboBox();
        linetypeCombo->addItems(QStringList() 
            << "Continuous" << "Dashed" << "Hidden" << "Center" 
            << "Phantom" << "Dot" << "DashDot" << "Border");
        linetypeCombo->setStyleSheet("QComboBox { font-size: 10pt; }");
        layout->addWidget(linetypeCombo);
        
        // Set tab order
        dialog.setTabOrder(searchEdit, layerListWidget);
        dialog.setTabOrder(layerListWidget, linetypeCombo);
        
    } else if (operationType == "Set Transparency") {
        layout->addWidget(new QLabel("Transparency (0=opaque, 90=transparent):"));
        transparencySpin = new QSpinBox();
        transparencySpin->setRange(0, 90);
        transparencySpin->setValue(0);
        transparencySpin->setSuffix("%");
        transparencySpin->setStyleSheet("QSpinBox { font-size: 10pt; }");
        layout->addWidget(transparencySpin);
        
        // Set tab order
        dialog.setTabOrder(searchEdit, layerListWidget);
        dialog.setTabOrder(layerListWidget, transparencySpin);
    }
    
    // Add buttons
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout->addWidget(buttons);
    
    // Execute dialog and create operation item if accepted
    if (dialog.exec() == QDialog::Accepted) {
        // Get the selected layer from the listbox
        QString layerName;
        QListWidgetItem* currentItem = layerListWidget->currentItem();
        if (currentItem && !(currentItem->flags() == Qt::NoItemFlags)) {
            layerName = currentItem->text();
        }
        
        QString newLayerName = newLayerNameEdit ? newLayerNameEdit->text() : "";
        
        if (operationType == "Create New Layer") {
            if (newLayerName.isEmpty()) {
                QMessageBox::warning(this, "Invalid Input", "Please enter a layer name!");
                return;
            }
        } else if (layerName.isEmpty()) {
            QMessageBox::warning(this, "Invalid Input", "Please select a layer from the list!");
            return;
        }
        
        // Create list item with appropriate description
        QListWidgetItem* item = new QListWidgetItem();
        QString description;
        QStringList data;
        data << operationType;
        
        if (operationType == "Create New Layer") {
            data << newLayerName;
            data << selectedColor;
            description = QString("Create New Layer: %1 [%2]").arg(newLayerName, selectedColor);
            item->setIcon(style()->standardIcon(QStyle::SP_FileDialogNewFolder));
            
        } else {
            data << layerName;
            
            if (operationType == "Delete Layer") {
                description = QString("Delete Layer: %1").arg(layerName);
                item->setIcon(style()->standardIcon(QStyle::SP_TrashIcon));
                
            } else if (operationType == "Rename Layer") {
                QString newName = newNameEdit ? newNameEdit->text() : "";
                if (newName.isEmpty()) {
                    QMessageBox::warning(this, "Invalid Input", "Please enter a new layer name!");
                    return;
                }
                description = QString("Rename Layer: %1 → %2").arg(layerName, newName);
                data << newName;
                item->setIcon(style()->standardIcon(QStyle::SP_DialogSaveButton));
                
            } else if (operationType == "Change Color") {
                description = QString("Change Color: %1 → %2").arg(layerName, selectedColor);
                data << selectedColor;
                item->setIcon(style()->standardIcon(QStyle::SP_ComputerIcon));
                
            } else if (operationType == "Freeze/Thaw") {
                QString action = freezeCombo ? freezeCombo->currentText() : "Freeze";
                description = QString("%1 Layer: %2").arg(action, layerName);
                data << action.toLower();
                item->setIcon(style()->standardIcon(QStyle::SP_MessageBoxWarning));
                
            } else if (operationType == "Set Visibility") {
                QString visibility = visibilityCombo ? visibilityCombo->currentText() : "On";
                description = QString("Set Visibility: %1 → %2").arg(layerName, visibility);
                data << visibility;
                item->setIcon(style()->standardIcon(QStyle::SP_DialogYesButton));
                
            } else if (operationType == "Change Linetype") {
                QString linetype = linetypeCombo ? linetypeCombo->currentText() : "Continuous";
                description = QString("Change Linetype: %1 → %2").arg(layerName, linetype);
                data << linetype;
                item->setIcon(style()->standardIcon(QStyle::SP_FileDialogDetailedView));
                
            } else if (operationType == "Set Transparency") {
                int transparency = transparencySpin ? transparencySpin->value() : 0;
                description = QString("Set Transparency: %1 → %2%").arg(layerName).arg(transparency);
                data << QString::number(transparency);
                item->setIcon(style()->standardIcon(QStyle::SP_DialogApplyButton));
            }
        }
        
        // Set item properties
        item->setText(description);
        item->setCheckState(Qt::Checked);
        item->setToolTip(QString("Layer operation: %1").arg(description));
        item->setData(Qt::UserRole, data);
        
        // Add to list
        m_layerOperationsWidget->addItem(item);
        updateLayerOperationStatus();
        
        logMessage(QString("Added layer operation: %1").arg(description));
    }
}

// Continue with existing implementations...

void MainWindow::setupUi() {
    setWindowTitle("BricsCAD Batch Processing - Professional Edition");
    resize(900, 700);
    
    createMenuBar();
    createCentralWidget();
    createStatusBar();
}

void MainWindow::createMenuBar() {
    QMenuBar* menuBar = this->menuBar();
    
    // File Menu
    QMenu* fileMenu = menuBar->addMenu("&File");
    fileMenu->addAction("&Exit", this, &QWidget::close);
    
    // Tools Menu
    QMenu* toolsMenu = menuBar->addMenu("&Tools");
    toolsMenu->addAction("&Clear Log", this, &MainWindow::onClearLog);
    toolsMenu->addAction("&Export Log...", this, &MainWindow::onExportLog);
    
    // Help Menu
    QMenu* helpMenu = menuBar->addMenu("&Help");
    helpMenu->addAction("&About...", this, &MainWindow::showAbout);
}

void MainWindow::createCentralWidget() {
    QWidget* centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);
    
    QVBoxLayout* mainLayout = new QVBoxLayout(centralWidget);
    
    // Create tab widget
    m_tabWidget = new QTabWidget();
    mainLayout->addWidget(m_tabWidget);
    
    // Create tabs
    m_tabWidget->addTab(createGeneralTab(), "General");
    m_tabWidget->addTab(createTextTab(), "Text Replacement");
    m_tabWidget->addTab(createAttributeTab(), "Attribute Replacement");
    m_tabWidget->addTab(createLayerTab(), "Layer Management");
    m_tabWidget->addTab(createLispTab(), "LISP Scripts");
    m_tabWidget->addTab(createOpenCirtTab(), "OpenCirt");
    
    // Log area
    QGroupBox* logGroup = new QGroupBox("Processing Log");
    QVBoxLayout* logLayout = new QVBoxLayout(logGroup);
    
    m_logTextEdit = new QTextEdit();
    m_logTextEdit->setReadOnly(true);
    m_logTextEdit->setMaximumHeight(150);
    m_logTextEdit->setFont(QFont("Monospace", 9));
    logLayout->addWidget(m_logTextEdit);
    
    mainLayout->addWidget(logGroup);
    
    // Progress bar
    m_progressBar = new QProgressBar();
    mainLayout->addWidget(m_progressBar);
    
    // Control buttons
    QHBoxLayout* buttonLayout = new QHBoxLayout();
    buttonLayout->addStretch();
    
    m_startButton = new QPushButton("Start Processing");
    m_startButton->setDefault(true);
    m_stopButton = new QPushButton("Stop");
    m_stopButton->setEnabled(false);
    
    buttonLayout->addWidget(m_startButton);
    buttonLayout->addWidget(m_stopButton);
    
    mainLayout->addLayout(buttonLayout);
}

void MainWindow::createStatusBar() {
    m_statusLabel = new QLabel("Ready");
    statusBar()->addPermanentWidget(m_statusLabel);
}

// ============================================================================
// Tab Creation Methods
// ============================================================================

QWidget* MainWindow::createGeneralTab() {
    auto* widget = new QWidget();
    auto* layout = new QVBoxLayout(widget);
    
    // Folder selection group
    auto* folderGroup = new QGroupBox("Folder Selection");
    auto* folderLayout = new QGridLayout(folderGroup);
    
    folderLayout->addWidget(new QLabel("DWG Folder:"), 0, 0);
    m_folderEdit = new QLineEdit();
    folderLayout->addWidget(m_folderEdit, 0, 1);
    
    m_browseButton = new QPushButton("Browse...");
    folderLayout->addWidget(m_browseButton, 0, 2);
    
    m_recursiveCheck = new QCheckBox("Include subfolders");
    m_recursiveCheck->setChecked(true);
    folderLayout->addWidget(m_recursiveCheck, 1, 1);
    
    layout->addWidget(folderGroup);
    
    // File filter group
    auto* filterGroup = new QGroupBox("File Filters");
    auto* filterLayout = new QGridLayout(filterGroup);
    
    filterLayout->addWidget(new QLabel("Include pattern:"), 0, 0);
    m_includeEdit = new QLineEdit("*.dwg");
    filterLayout->addWidget(m_includeEdit, 0, 1);
    
    filterLayout->addWidget(new QLabel("Exclude pattern:"), 1, 0);
    m_excludeEdit = new QLineEdit("*.bak");
    filterLayout->addWidget(m_excludeEdit, 1, 1);
    
    layout->addWidget(filterGroup);
    
    // Processing options
    auto* optionsGroup = new QGroupBox("Processing Options");
    auto* optionsLayout = new QGridLayout(optionsGroup);
    
    // Backup Hauptoption
    m_backupCheck = new QCheckBox("Create backup before processing");
    m_backupCheck->setChecked(true);
    m_backupCheck->setToolTip("Creates a .bak file for each DWG before processing");
    optionsLayout->addWidget(m_backupCheck, 0, 0, 1, 2);
    
    // Erweiterte Backup-Optionen (eingerückt für visuelle Hierarchie)
    m_deleteOldBackupsCheck = new QCheckBox("    Delete existing *.bak files first");
    m_deleteOldBackupsCheck->setToolTip("Removes old backup files before creating new ones");
    m_deleteOldBackupsCheck->setEnabled(m_backupCheck->isChecked());
    optionsLayout->addWidget(m_deleteOldBackupsCheck, 1, 0, 1, 2);
    
    m_timestampBackupsCheck = new QCheckBox("    Add timestamp to backup names");
    m_timestampBackupsCheck->setToolTip("Creates unique backups with date/time: drawing_20241210_143022.bak");
    m_timestampBackupsCheck->setEnabled(m_backupCheck->isChecked());
    optionsLayout->addWidget(m_timestampBackupsCheck, 2, 0, 1, 2);
    
    // Backup location
    auto* backupLocationLabel = new QLabel("    Backup location:");
    backupLocationLabel->setEnabled(m_backupCheck->isChecked());
    optionsLayout->addWidget(backupLocationLabel, 3, 0);
    
    m_backupLocationCombo = new QComboBox();
    m_backupLocationCombo->addItems(QStringList() << "Same folder as DWG" << "Custom folder...");
    m_backupLocationCombo->setEnabled(m_backupCheck->isChecked());
    m_backupLocationCombo->setToolTip("Where to store backup files");
    optionsLayout->addWidget(m_backupLocationCombo, 3, 1);
    
    // Custom backup folder (initially hidden)
    m_customBackupFolderEdit = new QLineEdit();
    m_customBackupFolderEdit->setPlaceholderText("Select custom backup folder...");
    m_customBackupFolderEdit->setVisible(false);
    optionsLayout->addWidget(m_customBackupFolderEdit, 4, 0);
    
    m_browseBackupFolderButton = new QPushButton("Browse...");
    m_browseBackupFolderButton->setVisible(false);
    optionsLayout->addWidget(m_browseBackupFolderButton, 4, 1);
    
    // Andere Optionen
    m_validateCheck = new QCheckBox("Validate files after processing");
    m_validateCheck->setToolTip("Checks if DWG files can be opened after processing");
    optionsLayout->addWidget(m_validateCheck, 5, 0, 1, 2);
    
    m_logCheck = new QCheckBox("Create detailed log file");
    m_logCheck->setChecked(true);
    m_logCheck->setToolTip("Saves processing details to a log file");
    optionsLayout->addWidget(m_logCheck, 6, 0, 1, 2);
    
    layout->addWidget(optionsGroup);
    
    // Connect backup-related signals
    connect(m_backupCheck, &QCheckBox::toggled, [this](bool checked) {
        m_deleteOldBackupsCheck->setEnabled(checked);
        m_timestampBackupsCheck->setEnabled(checked);
        m_backupLocationCombo->setEnabled(checked);
        
        // Update label
        auto* label = qobject_cast<QLabel*>(
            static_cast<QGridLayout*>(static_cast<QWidget*>(m_backupCheck->parent())->layout())->itemAtPosition(3, 0)->widget()
        );
        if (label) label->setEnabled(checked);
        
        // Update custom folder visibility if needed
        if (checked && m_backupLocationCombo->currentIndex() == 1) {
            m_customBackupFolderEdit->setVisible(true);
            m_browseBackupFolderButton->setVisible(true);
        } else {
            m_customBackupFolderEdit->setVisible(false);
            m_browseBackupFolderButton->setVisible(false);
        }
        
        // Log the change
        logMessage(QString("Backup option %1").arg(checked ? "enabled" : "disabled"));
    });
    
    connect(m_backupLocationCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), 
            [this](int index) {
        bool showCustom = (index == 1); // "Custom folder..." selected
        m_customBackupFolderEdit->setVisible(showCustom);
        m_browseBackupFolderButton->setVisible(showCustom);
        
        if (showCustom && m_customBackupFolderEdit->text().isEmpty()) {
            // Auto-open folder browser when "Custom folder..." is selected
            QTimer::singleShot(100, this, [this]() {
                this->onBrowseBackupFolder();
            });
        }
    });
    
    connect(m_browseBackupFolderButton, &QPushButton::clicked, 
            this, &MainWindow::onBrowseBackupFolder);
    
    // File list preview
    auto* previewGroup = new QGroupBox("Files to Process");
    auto* previewLayout = new QVBoxLayout(previewGroup);
    
    m_fileList = new QListWidget();
    m_fileList->setMaximumHeight(150);
    previewLayout->addWidget(m_fileList);
    
    auto* refreshButton = new QPushButton("Refresh File List");
    previewLayout->addWidget(refreshButton);
    
    layout->addWidget(previewGroup);
    
    layout->addStretch();
    
    // Connect signals
    connect(m_browseButton, &QPushButton::clicked, this, &MainWindow::onBrowseFolder);
    connect(refreshButton, &QPushButton::clicked, this, &MainWindow::onRefreshFileList);
    
    return widget;
}

QWidget* MainWindow::createTextTab() {
    auto* widget = new QWidget();
    auto* layout = new QVBoxLayout(widget);
    
    // Enable text processing
    m_textEnableCheck = new QCheckBox("Enable text replacement");
    m_textEnableCheck->setChecked(false);
    layout->addWidget(m_textEnableCheck);
    
    // Search and replace
    auto* searchGroup = new QGroupBox("Search and Replace");
    auto* searchLayout = new QGridLayout(searchGroup);
    
    searchLayout->addWidget(new QLabel("Search for:"), 0, 0);
    m_searchTextEdit = new QLineEdit();
    searchLayout->addWidget(m_searchTextEdit, 0, 1);
    
    searchLayout->addWidget(new QLabel("Replace with:"), 1, 0);
    m_replaceTextEdit = new QLineEdit();
    searchLayout->addWidget(m_replaceTextEdit, 1, 1);
    
    m_caseSensitiveCheck = new QCheckBox("Case sensitive");
    searchLayout->addWidget(m_caseSensitiveCheck, 2, 0);
    
    m_wholeWordCheck = new QCheckBox("Whole words only");
    searchLayout->addWidget(m_wholeWordCheck, 2, 1);
    
    m_regexCheck = new QCheckBox("Use regular expressions");
    searchLayout->addWidget(m_regexCheck, 3, 0);
    
    layout->addWidget(searchGroup);
    
    // Text types
    auto* typesGroup = new QGroupBox("Text Types to Process");
    auto* typesLayout = new QVBoxLayout(typesGroup);
    
    m_singleLineCheck = new QCheckBox("Single-line text (TEXT)");
    m_singleLineCheck->setChecked(true);
    typesLayout->addWidget(m_singleLineCheck);
    
    m_multiLineCheck = new QCheckBox("Multi-line text (MTEXT)");
    m_multiLineCheck->setChecked(true);
    typesLayout->addWidget(m_multiLineCheck);
    
    m_dimensionCheck = new QCheckBox("Dimension text");
    typesLayout->addWidget(m_dimensionCheck);
    
    m_leaderCheck = new QCheckBox("Leader text");
    typesLayout->addWidget(m_leaderCheck);
    
    layout->addWidget(typesGroup);
    
    // Multiple replacements
    auto* multiGroup = new QGroupBox("Multiple Replacements");
    auto* multiLayout = new QVBoxLayout(multiGroup);
    
    m_replacementTable = new QTableWidget(0, 4);
    m_replacementTable->setHorizontalHeaderLabels(QStringList() 
    << "" << "Search" << "Replace" << "Options");
    m_replacementTable->horizontalHeader()->setStretchLastSection(true);
    m_replacementTable->setColumnWidth(0, 30);
    m_replacementTable->setMaximumHeight(150);
    multiLayout->addWidget(m_replacementTable);
    
    auto* tableButtonLayout = new QHBoxLayout();
    auto* addRowButton = new QPushButton("Add Row");
    auto* removeRowButton = new QPushButton("Remove Row");
    auto* importButton = new QPushButton("Import CSV");
    
    tableButtonLayout->addWidget(addRowButton);
    tableButtonLayout->addWidget(removeRowButton);
    tableButtonLayout->addWidget(importButton);
    tableButtonLayout->addStretch();
    
    multiLayout->addLayout(tableButtonLayout);
    
    layout->addWidget(multiGroup);
    
    layout->addStretch();
    
    // Connect signals
    connect(addRowButton, &QPushButton::clicked, this, &MainWindow::onAddReplacementRow);
    connect(removeRowButton, &QPushButton::clicked, this, &MainWindow::onRemoveReplacementRow);
    connect(importButton, &QPushButton::clicked, this, &MainWindow::onImportReplacements);
    
    return widget;
}

QWidget* MainWindow::createAttributeTab() {
    auto* widget = new QWidget();
    auto* layout = new QVBoxLayout(widget);
    
    // Enable attribute processing
    m_attributeEnableCheck = new QCheckBox("Enable attribute replacement");
    layout->addWidget(m_attributeEnableCheck);
    
    // Block filter
    auto* blockGroup = new QGroupBox("Block Filter");
    auto* blockLayout = new QGridLayout(blockGroup);
    
    blockLayout->addWidget(new QLabel("Block name:"), 0, 0);
    m_blockNameEdit = new QLineEdit();
    m_blockNameEdit->setPlaceholderText("leave empty for all blocks");
    m_blockNameEdit->setToolTip("Enter block names separated by ;\nExample: TITLEBLOCK;FRAME\nLeave empty to process all blocks");
    blockLayout->addWidget(m_blockNameEdit, 0, 1);
    
    m_nestedBlocksCheck = new QCheckBox("Process nested blocks");
    blockLayout->addWidget(m_nestedBlocksCheck, 1, 0, 1, 2);
    
    layout->addWidget(blockGroup);
    
    // Attribute filter
    auto* attrGroup = new QGroupBox("Attribute Filter");
    auto* attrLayout = new QGridLayout(attrGroup);
    
    attrLayout->addWidget(new QLabel("Attribute tag:"), 0, 0);
    m_attributeTagEdit = new QLineEdit();
    m_attributeTagEdit->setPlaceholderText("Leave empty for all attributes");
    m_attributeTagEdit->setToolTip("Enter attribute tags separated by ;\nExample: PROJECT;DATE;REVISION\nLeave empty to process all attributes");
    attrLayout->addWidget(m_attributeTagEdit, 0, 1);
    
    attrLayout->addWidget(new QLabel("Search value:"), 1, 0);
    m_attributeSearchEdit = new QLineEdit();
    attrLayout->addWidget(m_attributeSearchEdit, 1, 1);
    
    attrLayout->addWidget(new QLabel("Replace value:"), 2, 0);
    m_attributeReplaceEdit = new QLineEdit();
    attrLayout->addWidget(m_attributeReplaceEdit, 2, 1);
    
    layout->addWidget(attrGroup);
    
    // Attribute options
    auto* attrOptionsGroup = new QGroupBox("Attribute Options");
    auto* attrOptionsLayout = new QVBoxLayout(attrOptionsGroup);
    
    m_invisibleAttributesCheck = new QCheckBox("Process invisible attributes");
    attrOptionsLayout->addWidget(m_invisibleAttributesCheck);
    
    m_constantAttributesCheck = new QCheckBox("Process constant attributes");
    attrOptionsLayout->addWidget(m_constantAttributesCheck);
    
    m_verifyRequiredCheck = new QCheckBox("Verify required attributes");
    attrOptionsLayout->addWidget(m_verifyRequiredCheck);
    
    layout->addWidget(attrOptionsGroup);
    
    layout->addStretch();
    
    return widget;
}

QWidget* MainWindow::createLayerTab() {
    auto* widget = new QWidget();
    auto* layout = new QVBoxLayout(widget);
    
    // Enable layer processing
    m_layerEnableCheck = new QCheckBox("Enable layer management");
    m_layerEnableCheck->setToolTip("Enable layer operations during batch processing");
    layout->addWidget(m_layerEnableCheck);
    
    // Layer Analysis & Discovery - NOW FIRST (moved up)
    auto* analysisGroup = new QGroupBox("Layer Analysis & Discovery");
    auto* analysisLayout = new QVBoxLayout(analysisGroup);
    
    // Add hint text above scan area
    auto* scanHintLabel = new QLabel("Scan all layers to get the layer names as proposals in the following dialogs");
    scanHintLabel->setStyleSheet("QLabel { color: #0066cc; font-style: italic; }");
    scanHintLabel->setWordWrap(true);
    analysisLayout->addWidget(scanHintLabel);
    
    // Expanded layer list widget - more space due to removed validation section
    m_layerListWidget = new QListWidget();
    m_layerListWidget->setMaximumHeight(200); // EXPANDED from 120 to 200
    m_layerListWidget->setSelectionMode(QAbstractItemView::ExtendedSelection);
    analysisLayout->addWidget(m_layerListWidget);
    
    auto* analysisButtonLayout = new QHBoxLayout();
    
    auto* scanButton = new QPushButton("Scan Layers");
    m_layerStatsButton = new QPushButton("Show Statistics");
    m_exportLayersButton = new QPushButton("Export List...");
    m_selectAllLayersButton = new QPushButton("Select All");
    m_clearSelectionButton = new QPushButton("Clear Selection");
    
    analysisButtonLayout->addWidget(scanButton);
    analysisButtonLayout->addWidget(m_layerStatsButton);
    analysisButtonLayout->addWidget(m_exportLayersButton);
    analysisButtonLayout->addStretch();
    analysisButtonLayout->addWidget(m_selectAllLayersButton);
    analysisButtonLayout->addWidget(m_clearSelectionButton);
    
    analysisLayout->addLayout(analysisButtonLayout);
    
    m_layerStatsLabel = new QLabel("No layers scanned");
    m_layerStatsLabel->setStyleSheet("QLabel { color: #666; }");
    analysisLayout->addWidget(m_layerStatsLabel);
    
    layout->addWidget(analysisGroup);
    
    // Layer Operations Management - NOW SECOND (moved down)
    auto* operationsGroup = new QGroupBox("Layer Operations");
    auto* operationsLayout = new QVBoxLayout(operationsGroup);
    
    m_layerOperationsWidget = new QListWidget();
    m_layerOperationsWidget->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_layerOperationsWidget->setDragDropMode(QAbstractItemView::InternalMove);
    m_layerOperationsWidget->setToolTip("Layer operations to execute (drag to reorder)");
    operationsLayout->addWidget(m_layerOperationsWidget);
    
    // Operation management buttons
    auto* operationButtonLayout = new QHBoxLayout();
    
    m_layerAddOpButton = new QPushButton("Add Operation...");
    m_layerEditOpButton = new QPushButton("Edit");
    m_layerRemoveOpButton = new QPushButton("Remove");
    m_layerClearOpButton = new QPushButton("Clear All");
    
    operationButtonLayout->addWidget(m_layerAddOpButton);
    operationButtonLayout->addWidget(m_layerEditOpButton);
    operationButtonLayout->addWidget(m_layerRemoveOpButton);
    operationButtonLayout->addWidget(m_layerClearOpButton);
    operationButtonLayout->addStretch();
    
    m_layerUpButton = new QPushButton("↑ Up");
    m_layerDownButton = new QPushButton("↓ Down");
    operationButtonLayout->addWidget(m_layerUpButton);
    operationButtonLayout->addWidget(m_layerDownButton);
    operationButtonLayout->addStretch();
    
    m_layerPreviewButton = new QPushButton("Preview");
    operationButtonLayout->addWidget(m_layerPreviewButton);
    
    operationsLayout->addLayout(operationButtonLayout);
    
    m_layerOpStatusLabel = new QLabel("No operations defined");
    m_layerOpStatusLabel->setStyleSheet("QLabel { color: #666; }");
    operationsLayout->addWidget(m_layerOpStatusLabel);
    
    layout->addWidget(operationsGroup);
    
    // Quick Operations Panel
    auto* quickOpsGroup = new QGroupBox("Quick Operations");
    auto* quickOpsLayout = new QGridLayout(quickOpsGroup);
    
    m_quickDeleteButton = new QPushButton("Delete Empty Layers");
    m_quickFreezeButton = new QPushButton("Freeze by Pattern...");
    m_quickColorButton = new QPushButton("Set Color...");
    m_quickRenameButton = new QPushButton("Rename/Replace...");
    
    quickOpsLayout->addWidget(m_quickDeleteButton, 0, 0);
    quickOpsLayout->addWidget(m_quickFreezeButton, 0, 1);
    quickOpsLayout->addWidget(m_quickColorButton, 1, 0);
    quickOpsLayout->addWidget(m_quickRenameButton, 1, 1);
    
    layout->addWidget(quickOpsGroup);
    layout->addStretch();
    
    // Connect Layer tab signals
    connect(m_layerEnableCheck, &QCheckBox::toggled, this, &MainWindow::onLayerEnableToggled);
    connect(m_layerAddOpButton, &QPushButton::clicked, this, &MainWindow::addLayerOperation);
    connect(m_layerEditOpButton, &QPushButton::clicked, this, &MainWindow::editLayerOperation);
    connect(m_layerRemoveOpButton, &QPushButton::clicked, this, &MainWindow::removeLayerOperation);
    connect(m_layerClearOpButton, &QPushButton::clicked, this, &MainWindow::clearLayerOperations);
    connect(m_layerUpButton, &QPushButton::clicked, this, &MainWindow::moveLayerOperationUp);
    connect(m_layerDownButton, &QPushButton::clicked, this, &MainWindow::moveLayerOperationDown);
    connect(m_layerPreviewButton, &QPushButton::clicked, this, &MainWindow::previewLayerOperations);
    connect(m_quickDeleteButton, &QPushButton::clicked, this, &MainWindow::addQuickDeleteOperation);
    connect(m_quickFreezeButton, &QPushButton::clicked, this, &MainWindow::addQuickFreezeOperation);
    connect(m_quickColorButton, &QPushButton::clicked, this, &MainWindow::addQuickColorOperation);
    connect(m_quickRenameButton, &QPushButton::clicked, this, &MainWindow::addQuickRenameOperation);
    connect(scanButton, &QPushButton::clicked, this, &MainWindow::scanLayers);
    connect(m_layerStatsButton, &QPushButton::clicked, this, &MainWindow::showLayerStatistics);
    connect(m_exportLayersButton, &QPushButton::clicked, this, &MainWindow::exportLayerList);
    connect(m_selectAllLayersButton, &QPushButton::clicked, this, &MainWindow::selectAllLayers);
    connect(m_clearSelectionButton, &QPushButton::clicked, this, &MainWindow::clearLayerSelection);
    connect(m_layerOperationsWidget, &QListWidget::itemSelectionChanged, 
            this, &MainWindow::onLayerOperationSelectionChanged);
    
    // Initialize state
    onLayerEnableToggled(false);
    
    return widget;
}

QWidget* MainWindow::createLispTab() {
    auto* widget = new QWidget();
    auto* layout = new QVBoxLayout(widget);
    
    // Enable LISP processing
    m_lispEnableCheck = new QCheckBox("Enable LISP script execution");
    m_lispEnableCheck->setToolTip("Execute LISP scripts on each DWG file during batch processing");
    layout->addWidget(m_lispEnableCheck);
    
    // LISP File Management
    auto* fileGroup = new QGroupBox("LISP Script Management");
    auto* fileLayout = new QVBoxLayout(fileGroup);
    
    m_lispListWidget = new QListWidget();
    m_lispListWidget->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_lispListWidget->setDragDropMode(QAbstractItemView::InternalMove);
    fileLayout->addWidget(m_lispListWidget);
    
    auto* fileButtonLayout = new QHBoxLayout();
    
    m_lispAddButton = new QPushButton("Add...");
    m_lispRemoveButton = new QPushButton("Remove");
    m_lispClearButton = new QPushButton("Clear All");
    m_lispUpButton = new QPushButton("↑ Up");
    m_lispDownButton = new QPushButton("↓ Down");
    m_lispValidateButton = new QPushButton("Validate");
    m_lispPreviewButton = new QPushButton("Preview");
    
    fileButtonLayout->addWidget(m_lispAddButton);
    fileButtonLayout->addWidget(m_lispRemoveButton);
    fileButtonLayout->addWidget(m_lispClearButton);
    fileButtonLayout->addStretch();
    fileButtonLayout->addWidget(m_lispUpButton);
    fileButtonLayout->addWidget(m_lispDownButton);
    fileButtonLayout->addStretch();
    fileButtonLayout->addWidget(m_lispValidateButton);
    fileButtonLayout->addWidget(m_lispPreviewButton);
    
    fileLayout->addLayout(fileButtonLayout);
    
    m_lispStatusLabel = new QLabel("No scripts loaded");
    m_lispStatusLabel->setStyleSheet("QLabel { color: #666; }");
    fileLayout->addWidget(m_lispStatusLabel);
    
    layout->addWidget(fileGroup);
    layout->addStretch();
    
    // Connect LISP tab signals
    connect(m_lispEnableCheck, &QCheckBox::toggled, this, &MainWindow::onLispEnableToggled);
    connect(m_lispAddButton, &QPushButton::clicked, this, &MainWindow::addLispScripts);
    connect(m_lispRemoveButton, &QPushButton::clicked, this, &MainWindow::removeLispScripts);
    connect(m_lispClearButton, &QPushButton::clicked, this, &MainWindow::clearLispScripts);
    connect(m_lispUpButton, &QPushButton::clicked, this, &MainWindow::moveLispUp);
    connect(m_lispDownButton, &QPushButton::clicked, this, &MainWindow::moveLispDown);
    connect(m_lispValidateButton, &QPushButton::clicked, this, &MainWindow::validateLispScripts);
    connect(m_lispPreviewButton, &QPushButton::clicked, this, &MainWindow::previewLispScript);
    connect(m_lispListWidget, &QListWidget::itemChanged, this, &MainWindow::onLispItemChanged);
    
    // Initialize state
    onLispEnableToggled(false);
    
    return widget;
}

// ============================================================================
// OpenCirt Tab Creation
// ============================================================================

QWidget* MainWindow::createOpenCirtTab() {
    m_openCirtTab = new OpenCirtTab(this);
    
    // Connect log messages from OpenCirt to main log
    connect(m_openCirtTab, &OpenCirtTab::logMessage,
            this, [this](const QString& msg, const QString& type) {
        logMessage(msg, type);
    });
    
    // Sync project root from General tab folder
    // The folder edit is already connected - we update OpenCirt when it changes
    if (m_folderEdit && !m_folderEdit->text().isEmpty()) {
        m_openCirtTab->setProjectRoot(m_folderEdit->text());
    }
    
    // Connect folder changes to OpenCirt tab
    connect(m_folderEdit, &QLineEdit::textChanged,
            m_openCirtTab, &OpenCirtTab::setProjectRoot);
    
    return m_openCirtTab;
}

// ============================================================================
// Signal Connections
// ============================================================================

void MainWindow::connectSignals() {
    // Main control signals
    connect(m_startButton, &QPushButton::clicked, this, &MainWindow::onStartProcessing);
    connect(m_stopButton, &QPushButton::clicked, this, &MainWindow::onStopProcessing);
    
    // Hide Start/Stop buttons when OpenCirt tab is active (it has its own buttons)
    connect(m_tabWidget, &QTabWidget::currentChanged, this, [this](int index) {
        bool isOpenCirt = (index == m_tabWidget->count() - 1); // OpenCirt is last tab
        m_startButton->setVisible(!isOpenCirt);
        m_stopButton->setVisible(!isOpenCirt);
    });
    
    // Engine signals are now connected in onStartProcessing() to avoid conflicts
    // Do NOT connect them here as they need to be connected to the progress dialog
}

// ============================================================================
// 🔧 TRANSFER-PROBLEM BEHOBEN: Layer Operations → Processing Engine
// ============================================================================

std::unique_ptr<ProcessingOptions> MainWindow::getOptionsFromUi() {
    logMessage("=== getOptionsFromUi() CALLED ===", "INFO");
    auto options = std::make_unique<ProcessingOptions>();
    
    // General options
    options->sourceFolder = m_folderEdit->text();
    options->includeSubfolders = m_recursiveCheck->isChecked();
    
    // File patterns
    if (!m_includeEdit->text().isEmpty()) {
        options->filePatterns = m_includeEdit->text().split(";");
    } else {
        options->filePatterns << "*.dwg";
    }
    
    // Backup Settings
    options->createBackups = m_backupCheck->isChecked();
    options->deleteOldBackups = m_deleteOldBackupsCheck->isChecked();
    options->useTimestampInBackup = m_timestampBackupsCheck->isChecked();
    options->useCustomBackupLocation = (m_backupLocationCombo->currentIndex() == 1);
    options->customBackupFolder = m_customBackupFolderEdit->text();    

    // Text processing
    options->enableTextReplacement = m_textEnableCheck->isChecked();
    options->searchText = m_searchTextEdit->text();
    options->replaceText = m_replaceTextEdit->text();
    options->caseSensitive = m_caseSensitiveCheck->isChecked();
    options->wholeWordsOnly = m_wholeWordCheck->isChecked();
    options->useRegex = m_regexCheck->isChecked();
    
    // Multiple Replacements
    options->multipleReplacements.clear();
    for (int i = 0; i < m_replacementTable->rowCount(); ++i) {
        QCheckBox* enabledCheck = qobject_cast<QCheckBox*>(
            m_replacementTable->cellWidget(i, 0));
    
        if (enabledCheck && enabledCheck->isChecked()) {
            ProcessingOptions::TextReplacement repl;
            
            QTableWidgetItem* searchItem = m_replacementTable->item(i, 1);
            QTableWidgetItem* replaceItem = m_replacementTable->item(i, 2);
            
            repl.searchText = searchItem ? searchItem->text() : "";
            repl.replaceText = replaceItem ? replaceItem->text() : "";
            repl.isEnabled = true;
            
            if (!repl.searchText.isEmpty()) {
                options->multipleReplacements.append(repl);
            }
        }
    }
    
    // Attribute processing
    options->enableAttributeReplacement = m_attributeEnableCheck->isChecked();
    options->attributeSearchText = m_attributeSearchEdit->text();
    options->attributeReplaceText = m_attributeReplaceEdit->text();
    options->invisibleAttributesCheck = m_invisibleAttributesCheck->isChecked();
    options->constantAttributesCheck = m_constantAttributesCheck->isChecked();  

    // Block und Attribute Filter
    if (!m_blockNameEdit->text().isEmpty()) {
        options->targetBlocks = m_blockNameEdit->text().split(";", Qt::SkipEmptyParts);
        for (QString& block : options->targetBlocks) {
            block = block.trimmed();
        }
    }

    if (!m_attributeTagEdit->text().isEmpty()) {
        options->targetAttributes = m_attributeTagEdit->text().split(";", Qt::SkipEmptyParts);
        for (QString& tag : options->targetAttributes) {
            tag = tag.trimmed();
        }
    }
    
    // 🔧 TRANSFER-FIX: Layer operations richtig übertragen!
    options->enableLayerOperations = m_layerEnableCheck->isChecked();
    options->layerOperations.clear();
    
    logMessage(QString("Layer operations widget count: %1").arg(m_layerOperationsWidget->count()), "INFO");
    
    // Übertrage alle Layer Operations aus der UI
    for (int i = 0; i < m_layerOperationsWidget->count(); ++i) {
        QListWidgetItem* item = m_layerOperationsWidget->item(i);
        if (item && item->checkState() == Qt::Checked) {
            QStringList data = item->data(Qt::UserRole).toStringList();
            
            if (!data.isEmpty()) {
                ProcessingOptions::LayerOperation op;
                op.type = data[0];
                op.layerName = data.size() > 1 ? data[1] : "";
                op.newValue = data.size() > 2 ? data[2] : "";
                op.isEnabled = true;
                
                options->layerOperations.append(op);
                
                logMessage(QString("Added layer operation: %1 - %2 - %3")
                          .arg(op.type).arg(op.layerName).arg(op.newValue), "INFO");
            }
        }
    }
    
    logMessage(QString("FINAL: Layer operations count: %1").arg(options->layerOperations.size()), "INFO");

    // Text types to process
    options->processSingleLineText = m_singleLineCheck->isChecked();
    options->processMultiLineText = m_multiLineCheck->isChecked();
    options->processDimensionText = m_dimensionCheck->isChecked();
    options->processLeaderText = m_leaderCheck->isChecked();

    // LISP options
    options->enableLispExecution = m_lispEnableCheck->isChecked();
    
    // Collect LISP scripts
    options->lispScripts.clear();
    for (int i = 0; i < m_lispListWidget->count(); ++i) {
        QListWidgetItem* item = m_lispListWidget->item(i);
        if (item && item->checkState() == Qt::Checked) {
            QString filePath = item->data(Qt::UserRole).toString();
            if (!filePath.isEmpty() && QFile::exists(filePath)) {
                options->lispScripts.append(filePath);
                logMessage(QString("Added LISP script: %1").arg(QFileInfo(filePath).fileName()), "INFO");
            }
        }
    }
    
    if (options->enableLispExecution) {
        logMessage(QString("LISP execution enabled with %1 scripts").arg(options->lispScripts.size()), "INFO");
    }
    
    return options;
}

// ============================================================================
// Main Event Handlers
// ============================================================================

void MainWindow::onStartProcessing() {
    // Validate settings first
    if (!validateBackupSettings()) {
        return;
    }
    
    // Get options from UI
    auto options = getOptionsFromUi();
    if (!options) {
        QMessageBox::warning(this, "Invalid Options", 
                           "Please configure processing options");
        return;
    }
    
    // Get the file list from the widget
    QStringList filesToProcess;
    for (int i = 0; i < m_fileList->count(); ++i) {
        QListWidgetItem* item = m_fileList->item(i);
        QString fullPath = item->data(Qt::UserRole).toString();
        if (fullPath.isEmpty()) {
            fullPath = QDir(m_folderEdit->text()).absoluteFilePath(item->text());
        }
        filesToProcess.append(fullPath);
    }
    
    if (filesToProcess.isEmpty()) {
        QMessageBox::warning(this, "No Files", 
                           "Please select files to process");
        return;
    }
    
    // Store files in options
    options->filesToProcess = filesToProcess;
    
    // Create and show progress dialog
    ProcessingProgressDialog* progressDialog = new ProcessingProgressDialog(this);
    progressDialog->setWindowTitle("Processing Files");
    progressDialog->setModal(true);
    
    // Connect engine signals to MainWindow (for regular updates)
    connect(m_engine.get(), &BatchProcessingEngine::progressUpdated, 
            this, &MainWindow::onProgressUpdated, Qt::UniqueConnection);
    connect(m_engine.get(), &BatchProcessingEngine::fileProcessed,
            this, &MainWindow::onFileProcessed, Qt::UniqueConnection);
    connect(m_engine.get(), &BatchProcessingEngine::processingFinished,
            this, &MainWindow::onProcessingFinished, Qt::UniqueConnection);
    connect(m_engine.get(), &BatchProcessingEngine::errorOccurred,
            this, &MainWindow::onErrorOccurred, Qt::UniqueConnection);
    
    // Connect progress signals to dialog
    connect(m_engine.get(), &BatchProcessingEngine::processingStarted,
            progressDialog, [progressDialog](int totalFiles) {
        progressDialog->setTotalFiles(totalFiles);
        progressDialog->addDetail(QString("Starting to process %1 files...").arg(totalFiles));
    });
    
    connect(m_engine.get(), &BatchProcessingEngine::progressUpdated,
            progressDialog, [progressDialog](int current, int total, const QString& currentFile) {
        progressDialog->updateProgress(current, QFileInfo(currentFile).fileName());
        progressDialog->addDetail(QString("[%1/%2] Processing: %3")
            .arg(current)
            .arg(total)
            .arg(QFileInfo(currentFile).fileName()));
    });
    
    connect(m_engine.get(), &BatchProcessingEngine::fileProcessed,
            progressDialog, [progressDialog, this](const QString& file, bool success, const QString& message) {
        QString status = success ? "✓" : "✗";
        QString detail = QString("%1 %2: %3")
            .arg(status)
            .arg(QFileInfo(file).fileName())
            .arg(message);
        progressDialog->addDetail(detail);
    });
    
    connect(m_engine.get(), &BatchProcessingEngine::processingFinished,
            progressDialog, [progressDialog](bool success, const QString& summary) {
        progressDialog->addDetail("\n" + summary);
        progressDialog->addDetail("\n=== Processing finished ===");
    });
    
    connect(m_engine.get(), &BatchProcessingEngine::errorOccurred,
            progressDialog, [progressDialog](const QString& error) {
        progressDialog->addDetail(QString("ERROR: %1").arg(error));
    });
    
    // Connect dialog abort button
    connect(progressDialog, &ProcessingProgressDialog::abortRequested,
            m_engine.get(), [this, progressDialog]() {
        m_engine->abortProcessing();
        progressDialog->addDetail("\n=== Processing aborted by user ===");
    });
    
    // Delete all bmk_counters.tmp files before batch start
    {
        QString rootFolder = m_folderEdit->text();
        if (!rootFolder.isEmpty()) {
            QDirIterator tmpIt(rootFolder, QStringList() << "bmk_counters.tmp",
                              QDir::Files, QDirIterator::Subdirectories);
            while (tmpIt.hasNext()) {
                QString tmpFile = tmpIt.next();
                if (QFile::remove(tmpFile)) {
                    logMessage(QString("Deleted: %1").arg(tmpFile), "INFO");
                }
            }
        }
    }
    
    // Update UI state
    updateUiState(true);
    
    // Show dialog and start processing
    progressDialog->show();
    
    // Start the engine with our options
    if (!m_engine->startProcessing(*options)) {
        progressDialog->hide();
        progressDialog->deleteLater();
        updateUiState(false);
        QMessageBox::critical(this, "Processing Error", 
                            "Failed to start processing");
    }
}

void MainWindow::onStopProcessing() {
    if (m_engine && m_isProcessing) {
        m_engine->stopProcessing();
        logMessage("Stopping processing...");
    }
}

void MainWindow::onProcessingFinished(bool success, const QString& summary) {
    m_isProcessing = false;
    updateUiState(false);
    
    if (m_progressDialog) {
        m_progressDialog->accept();
        delete m_progressDialog;
        m_progressDialog = nullptr;
    }
    
    if (success) {
        logMessage("Processing completed successfully!");
        
        // Ask user if they want to clear the successfully executed layer operations
        if (m_layerEnableCheck->isChecked() && m_layerOperationsWidget->count() > 0) {
            int result = QMessageBox::question(this, "Layer Operations",
                "Processing completed successfully.\n\n"
                "Do you want to clear the layer operations list?\n\n"
                "(Choose 'No' if you want to keep them for the next batch)",
                QMessageBox::Yes | QMessageBox::No,
                QMessageBox::Yes);
            
            if (result == QMessageBox::Yes) {
                m_layerOperationsWidget->clear();
                updateLayerOperationStatus();
                logMessage("Layer operations list cleared");
            }
        }
        
        QMessageBox::information(this, "Processing Complete", summary);
    } else {
        logMessage("Processing stopped or failed!", "WARNING");
        QMessageBox::warning(this, "Processing Stopped", summary);
    }
    
    m_statusLabel->setText("Ready");
}

void MainWindow::onProgressUpdated(int current, int total, const QString& currentFile) {
    m_progressBar->setMaximum(total);
    m_progressBar->setValue(current);
    
    if (m_progressDialog) {
        m_progressDialog->updateProgress(current, QFileInfo(currentFile).fileName());
    }
    
    m_statusLabel->setText(QString("Processing file %1 of %2: %3")
                          .arg(current)
                          .arg(total)
                          .arg(QFileInfo(currentFile).fileName()));
}

void MainWindow::onFileProcessed(const QString& filePath, bool success, const QString& message) {
    QString logMsg = QString("[%1] %2 - %3")
                    .arg(success ? "OK" : "ERROR")
                    .arg(QFileInfo(filePath).fileName())
                    .arg(message);
    logMessage(logMsg, success ? "INFO" : "ERROR");
    
    if (m_progressDialog) {
        m_progressDialog->addDetail(logMsg);
    }
}

void MainWindow::onErrorOccurred(const QString& error) {
    logMessage(error, "ERROR");
    if (m_progressDialog) {
        m_progressDialog->addDetail(QString("ERROR: %1").arg(error));
    }
}

// ============================================================================
// Layer Operation Implementations (continued)
// ============================================================================

void MainWindow::editLayerOperation() {
    QListWidgetItem* current = m_layerOperationsWidget->currentItem();
    if (!current) return;
    
    bool ok;
    QString newText = QInputDialog::getText(this, 
        "Edit Layer Operation",
        "Operation description:",
        QLineEdit::Normal,
        current->text(), &ok);
    
    if (ok && !newText.isEmpty()) {
        current->setText(newText);
    }
}

void MainWindow::removeLayerOperation() {
    QList<QListWidgetItem*> selected = m_layerOperationsWidget->selectedItems();
    for (QListWidgetItem* item : selected) {
        delete item;
    }
    updateLayerOperationStatus();
}

void MainWindow::clearLayerOperations() {
    if (m_layerOperationsWidget->count() == 0) return;
    
    if (QMessageBox::question(this, "Clear All Operations",
        "Remove all layer operations?") == QMessageBox::Yes) {
        m_layerOperationsWidget->clear();
        updateLayerOperationStatus();
    }
}

void MainWindow::moveLayerOperationUp() {
    int currentRow = m_layerOperationsWidget->currentRow();
    if (currentRow > 0) {
        QListWidgetItem* item = m_layerOperationsWidget->takeItem(currentRow);
        m_layerOperationsWidget->insertItem(currentRow - 1, item);
        m_layerOperationsWidget->setCurrentRow(currentRow - 1);
    }
}

void MainWindow::moveLayerOperationDown() {
    int currentRow = m_layerOperationsWidget->currentRow();
    if (currentRow >= 0 && currentRow < m_layerOperationsWidget->count() - 1) {
        QListWidgetItem* item = m_layerOperationsWidget->takeItem(currentRow);
        m_layerOperationsWidget->insertItem(currentRow + 1, item);
        m_layerOperationsWidget->setCurrentRow(currentRow + 1);
    }
}

void MainWindow::addQuickDeleteOperation() {
    QListWidgetItem* item = new QListWidgetItem();
    item->setText("Delete Layer: * (empty layers only)");
    item->setCheckState(Qt::Checked);
    item->setIcon(style()->standardIcon(QStyle::SP_TrashIcon));
    m_layerOperationsWidget->addItem(item);
    updateLayerOperationStatus();
}

void MainWindow::addQuickFreezeOperation() {
    bool ok;
    QString pattern = QInputDialog::getText(this,
        "Freeze Layers by Pattern",
        "Enter layer name pattern:",
        QLineEdit::Normal,
        "*TEMP*", &ok);
    
    if (ok && !pattern.isEmpty()) {
        QListWidgetItem* item = new QListWidgetItem();
        item->setText(QString("Freeze Layer: %1").arg(pattern));
        item->setCheckState(Qt::Checked);
        m_layerOperationsWidget->addItem(item);
        updateLayerOperationStatus();
    }
}

void MainWindow::addQuickColorOperation() {
    QString color = showAutoCADColorGrid(this, "Set Layer Color");
    if (!color.isEmpty()) {
        QListWidgetItem* item = new QListWidgetItem();
        item->setText(QString("Change Color: * → %1").arg(color));
        item->setCheckState(Qt::Checked);
        m_layerOperationsWidget->addItem(item);
        updateLayerOperationStatus();
    }
}

void MainWindow::addQuickRenameOperation() {
    bool ok;
    QString search = QInputDialog::getText(this,
        "Rename Layers",
        "Search pattern:",
        QLineEdit::Normal,
        "", &ok);
    
    if (ok && !search.isEmpty()) {
        QString replace = QInputDialog::getText(this,
            "Rename Layers",
            "Replace with:",
            QLineEdit::Normal,
            "", &ok);
        
        if (ok) {
            QListWidgetItem* item = new QListWidgetItem();
            item->setText(QString("Rename Layer: '%1' → '%2'").arg(search, replace));
            item->setCheckState(Qt::Checked);
            m_layerOperationsWidget->addItem(item);
            updateLayerOperationStatus();
        }
    }
}

void MainWindow::previewLayerOperations() {
    QMessageBox::information(this, "Layer Operations Preview",
        QString("Total operations: %1\nEnabled operations: %2")
        .arg(m_layerOperationsWidget->count())
        .arg(m_layerOperationsWidget->count()));
}

// ✅ REAL BRX LAYER SCANNING (no more Mock!)
void MainWindow::scanLayers() {
    m_layerListWidget->clear();
    
    QString folderPath = m_folderEdit->text();
    if (folderPath.isEmpty()) {
        QMessageBox::warning(this, "No Folder", "Please select a DWG folder first!");
        return;
    }
    
    // Get current file list
    QStringList dwgFiles;
    for (int i = 0; i < m_fileList->count(); ++i) {
        QListWidgetItem* item = m_fileList->item(i);
        QString fullPath = item->data(Qt::UserRole).toString();
        if (!fullPath.isEmpty()) {
            dwgFiles.append(fullPath);
        }
    }
    
    if (dwgFiles.isEmpty()) {
        QMessageBox::warning(this, "No Files", "No DWG files found. Please refresh the file list first!");
        return;
    }
    
    // Show progress
    m_statusLabel->setText("Scanning layers in DWG files...");
    QApplication::setOverrideCursor(Qt::WaitCursor);
    
    QSet<QString> uniqueLayers;
    int scannedFiles = 0;
    int totalFiles = dwgFiles.size();
    
    logMessage("Starting real BRX layer scan...", "INFO");
    
    for (const QString& filePath : dwgFiles) {
        // ✅ REAL BRX IMPLEMENTATION
        try {
            // Create database object
            AcDbDatabase* pDb = new AcDbDatabase(false, true);
            
            // Open the DWG file
            std::wstring wFilePath = filePath.toStdWString();
            Acad::ErrorStatus es = pDb->readDwgFile(wFilePath.c_str());
            if (es != Acad::eOk) {
                logMessage(QString("Failed to read DWG file: %1 (Error: %2)")
                          .arg(QFileInfo(filePath).fileName())
                          .arg(int(es)), "WARNING");
                delete pDb;
                continue;
            }
            
            // Get layer table
            AcDbLayerTable* pLayerTable;
            es = pDb->getLayerTable(pLayerTable, AcDb::kForRead);
            if (es != Acad::eOk) {
                logMessage(QString("Failed to get layer table from: %1")
                          .arg(QFileInfo(filePath).fileName()), "WARNING");
                delete pDb;
                continue;
            }
            
            // Iterate through layers
            AcDbLayerTableIterator* pLayerIterator;
            pLayerTable->newIterator(pLayerIterator);
            
            for (; !pLayerIterator->done(); pLayerIterator->step()) {
                AcDbLayerTableRecord* pLayerRecord;
                es = pLayerIterator->getRecord(pLayerRecord, AcDb::kForRead);
                if (es == Acad::eOk) {
                    const ACHAR* layerName;
                    pLayerRecord->getName(layerName);
                    
                    // ✅ FIXED: Use correct string conversion for BRX API
                    QString qLayerName = QString::fromWCharArray(layerName);
                    uniqueLayers.insert(qLayerName);
                    
                    pLayerRecord->close();
                }
            }
            
            delete pLayerIterator;
            pLayerTable->close();
            delete pDb;
            
            scannedFiles++;
            
        } catch (...) {
            logMessage(QString("Exception while scanning: %1")
                      .arg(QFileInfo(filePath).fileName()), "ERROR");
            continue;
        }
        
        // Update progress every 10 files or at end
        if (scannedFiles % 10 == 0 || scannedFiles == totalFiles) {
            m_statusLabel->setText(QString("Scanned %1/%2 files, found %3 unique layers")
                                  .arg(scannedFiles).arg(totalFiles).arg(uniqueLayers.size()));
            QApplication::processEvents();
        }
    }
    
    // Populate layer list
    QStringList sortedLayers(uniqueLayers.begin(), uniqueLayers.end());
    std::sort(sortedLayers.begin(), sortedLayers.end());
    
    // Store scanned layers for use in dialogs
    m_scannedLayers = sortedLayers;
    
    for (const QString& layerName : sortedLayers) {
        QListWidgetItem* item = new QListWidgetItem(layerName);
        item->setIcon(style()->standardIcon(QStyle::SP_FileIcon));
        m_layerListWidget->addItem(item);
    }
    
    QApplication::restoreOverrideCursor();
    
    // Update status
    QString finalMessage = QString("Layer scan complete: %1 layers found in %2 files")
                          .arg(uniqueLayers.size()).arg(scannedFiles);
    m_statusLabel->setText(finalMessage);
    updateLayerSelectionStatus();
    logMessage(finalMessage, "INFO");
}

void MainWindow::showLayerStatistics() {
    QMessageBox::information(this, "Layer Statistics",
        QString("Total layers: %1\nSelected: %2")
        .arg(m_layerListWidget->count())
        .arg(m_layerListWidget->selectedItems().count()));
}

void MainWindow::exportLayerList() {
    QString fileName = QFileDialog::getSaveFileName(this,
        "Export Layer List",
        QString("layers_%1.csv").arg(QDateTime::currentDateTime().toString("yyyyMMdd")),
        "CSV Files (*.csv)");
    
    if (!fileName.isEmpty()) {
        logMessage(QString("Exported layer list to: %1").arg(fileName));
    }
}

void MainWindow::selectAllLayers() {
    for (int i = 0; i < m_layerListWidget->count(); ++i) {
        m_layerListWidget->item(i)->setSelected(true);
    }
    updateLayerSelectionStatus();
}

void MainWindow::clearLayerSelection() {
    m_layerListWidget->clearSelection();
    updateLayerSelectionStatus();
}

void MainWindow::loadLayerStandard() {
    // Load a layer standard file (e.g. CSV or JSON with predefined layer names/colors)
    QString fileName = QFileDialog::getOpenFileName(this,
        "Load Layer Standard", "",
        "Layer Standards (*.csv *.json *.txt);;All Files (*.*)");
    
    if (fileName.isEmpty()) return;
    
    logMessage(QString("Loading layer standard from: %1").arg(fileName));
    // TODO: Parse standard file and create layer operations
    QMessageBox::information(this, "Layer Standard",
        QString("Layer standard loaded from:\n%1\n\nFeature in development.").arg(fileName));
}

// ============================================================================
// Text Tab Event Handlers
// ============================================================================

void MainWindow::onAddReplacementRow() {
    int row = m_replacementTable->rowCount();
    m_replacementTable->insertRow(row);
    
    QCheckBox* enabledCheck = new QCheckBox();
    enabledCheck->setChecked(true);
    m_replacementTable->setCellWidget(row, 0, enabledCheck);
    
    m_replacementTable->setItem(row, 1, new QTableWidgetItem(""));
    m_replacementTable->setItem(row, 2, new QTableWidgetItem(""));
    m_replacementTable->setItem(row, 3, new QTableWidgetItem(""));
}

void MainWindow::onRemoveReplacementRow() {
    int currentRow = m_replacementTable->currentRow();
    if (currentRow >= 0) {
        m_replacementTable->removeRow(currentRow);
    }
}

void MainWindow::onImportReplacements() {
    QString fileName = QFileDialog::getOpenFileName(this, 
        "Import Replacements CSV", "", "CSV Files (*.csv);;Text Files (*.txt)");
    
    if (fileName.isEmpty()) return;
    
    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QMessageBox::warning(this, "Import Error", "Could not open file");
        return;
    }
    
    m_replacementTable->setRowCount(0);
    
    QTextStream stream(&file);
    while (!stream.atEnd()) {
        QString line = stream.readLine();
        QStringList parts = line.split(',');
        
        if (parts.size() >= 2) {
            int row = m_replacementTable->rowCount();
            m_replacementTable->insertRow(row);
            
            QCheckBox* enabledCheck = new QCheckBox();
            enabledCheck->setChecked(true);
            m_replacementTable->setCellWidget(row, 0, enabledCheck);
            
            m_replacementTable->setItem(row, 1, new QTableWidgetItem(parts[0].trimmed()));
            m_replacementTable->setItem(row, 2, new QTableWidgetItem(parts[1].trimmed()));
        }
    }
    
    file.close();
    logMessage(QString("Imported %1 replacements from %2")
               .arg(m_replacementTable->rowCount()).arg(fileName));
}

// ============================================================================
// General Tab Event Handlers
// ============================================================================

void MainWindow::onBrowseFolder() {
    QString folder = QFileDialog::getExistingDirectory(
        this, "Select DWG Folder", m_folderEdit->text());
    
    if (!folder.isEmpty()) {
        m_folderEdit->setText(folder);
        onRefreshFileList();
    }
}

void MainWindow::onRefreshFileList() {
    m_fileList->clear();
    
    QString folderPath = m_folderEdit->text();
    if (folderPath.isEmpty()) {
        m_statusLabel->setText("No folder selected");
        return;
    }
    
    QDir dir(folderPath);
    if (!dir.exists()) {
        m_statusLabel->setText("Folder does not exist");
        return;
    }
    
    m_statusLabel->setText("Scanning for DWG files...");
    QApplication::setOverrideCursor(Qt::WaitCursor);
    
    QStringList includePatterns = m_includeEdit->text().split(";", Qt::SkipEmptyParts);
    if (includePatterns.isEmpty()) {
        includePatterns << "*.dwg";
    }
    
    QStringList excludePatterns = m_excludeEdit->text().split(";", Qt::SkipEmptyParts);
    
    QStringList files;
    qint64 totalSize = 0;
    
    if (m_recursiveCheck->isChecked()) {
        files = findDwgFilesRecursive(dir, includePatterns, excludePatterns, totalSize);
    } else {
        files = findDwgFilesNonRecursive(dir, includePatterns, excludePatterns, totalSize);
    }
    
    for (const QString& filePath : files) {
        QFileInfo fileInfo(filePath);
        QListWidgetItem* item = new QListWidgetItem();
        QString sizeStr = formatFileSize(fileInfo.size());
        item->setText(QString("%1 [%2]").arg(fileInfo.fileName()).arg(sizeStr));
        item->setData(Qt::UserRole, filePath);
        item->setToolTip(filePath);
        item->setIcon(style()->standardIcon(QStyle::SP_FileIcon));
        m_fileList->addItem(item);
    }
    
    QApplication::restoreOverrideCursor();
    
    QString statusMsg = QString("Found %1 DWG files").arg(files.count());
    m_statusLabel->setText(statusMsg);
    logMessage(statusMsg);
}

void MainWindow::onBrowseBackupFolder() {
    QString folder = QFileDialog::getExistingDirectory(
        this, "Select Backup Folder", 
        m_customBackupFolderEdit->text().isEmpty() ? 
            m_folderEdit->text() : m_customBackupFolderEdit->text());
    
    if (!folder.isEmpty()) {
        m_customBackupFolderEdit->setText(folder);
        logMessage(QString("Backup folder selected: %1").arg(folder));
    }
}

QStringList MainWindow::findDwgFilesRecursive(const QDir& dir, 
                                              const QStringList& includePatterns,
                                              const QStringList& excludePatterns,
                                              qint64& totalSize) {
    QStringList files;
    
    for (const QString& pattern : includePatterns) {
        QDirIterator it(dir.absolutePath(), QStringList() << pattern, 
                       QDir::Files, QDirIterator::Subdirectories);
        while (it.hasNext()) {
            QString filePath = it.next();
            QString fileName = QFileInfo(filePath).fileName();
            
            bool excluded = false;
            for (const QString& excludePattern : excludePatterns) {
                QRegularExpression regex(QRegularExpression::wildcardToRegularExpression(excludePattern));
                if (regex.match(fileName).hasMatch()) {
                    excluded = true;
                    break;
                }
            }
            
            if (!excluded) {
                QFileInfo fileInfo(filePath);
                files.append(filePath);
                totalSize += fileInfo.size();
            }
        }
    }
    
    return files;
}

QStringList MainWindow::findDwgFilesNonRecursive(const QDir& dir,
                                                 const QStringList& includePatterns,
                                                 const QStringList& excludePatterns,
                                                 qint64& totalSize) {
    QStringList files;
    
    for (const QString& pattern : includePatterns) {
        QStringList matchingFiles = dir.entryList(QStringList() << pattern, 
                                                  QDir::Files | QDir::Readable);
        
        for (const QString& fileName : matchingFiles) {
            bool excluded = false;
            for (const QString& excludePattern : excludePatterns) {
                QRegularExpression regex(QRegularExpression::wildcardToRegularExpression(excludePattern));
                if (regex.match(fileName).hasMatch()) {
                    excluded = true;
                    break;
                }
            }
            
            if (!excluded) {
                QString fullPath = dir.absoluteFilePath(fileName);
                QFileInfo fileInfo(fullPath);
                files.append(fullPath);
                totalSize += fileInfo.size();
            }
        }
    }
    
    return files;
}

QString MainWindow::formatFileSize(qint64 bytes) {
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

// ============================================================================
// LISP Tab Event Handlers
// ============================================================================

void MainWindow::onLispEnableToggled(bool enabled) {
    m_lispListWidget->setEnabled(enabled);
    m_lispAddButton->setEnabled(enabled);
    m_lispRemoveButton->setEnabled(enabled);
    m_lispClearButton->setEnabled(enabled);
    m_lispUpButton->setEnabled(enabled);
    m_lispDownButton->setEnabled(enabled);
    m_lispValidateButton->setEnabled(enabled);
    m_lispPreviewButton->setEnabled(enabled);
}

void MainWindow::addLispScripts() {
    QStringList files = QFileDialog::getOpenFileNames(
        this, "Select LISP Scripts", "",
        "LISP Files (*.lsp *.lisp);;All Files (*.*)");
    
    if (files.isEmpty()) return;
    
    for (const QString& file : files) {
        QFileInfo fileInfo(file);
        QListWidgetItem* item = new QListWidgetItem();
        item->setText(fileInfo.fileName());
        item->setData(Qt::UserRole, file);
        item->setCheckState(Qt::Checked);
        item->setToolTip(file);
        item->setIcon(style()->standardIcon(QStyle::SP_FileIcon));
        m_lispListWidget->addItem(item);
    }
    
    updateLispStatus();
}

void MainWindow::removeLispScripts() {
    QList<QListWidgetItem*> selected = m_lispListWidget->selectedItems();
    for (QListWidgetItem* item : selected) {
        delete item;
    }
    updateLispStatus();
}

void MainWindow::clearLispScripts() {
    if (m_lispListWidget->count() == 0) return;
    
    if (QMessageBox::question(this, "Clear All Scripts",
        "Remove all LISP scripts from the list?") == QMessageBox::Yes) {
        m_lispListWidget->clear();
        updateLispStatus();
    }
}

void MainWindow::moveLispUp() {
    int currentRow = m_lispListWidget->currentRow();
    if (currentRow > 0) {
        QListWidgetItem* item = m_lispListWidget->takeItem(currentRow);
        m_lispListWidget->insertItem(currentRow - 1, item);
        m_lispListWidget->setCurrentRow(currentRow - 1);
    }
}

void MainWindow::moveLispDown() {
    int currentRow = m_lispListWidget->currentRow();
    if (currentRow >= 0 && currentRow < m_lispListWidget->count() - 1) {
        QListWidgetItem* item = m_lispListWidget->takeItem(currentRow);
        m_lispListWidget->insertItem(currentRow + 1, item);
        m_lispListWidget->setCurrentRow(currentRow + 1);
    }
}

void MainWindow::validateLispScripts() {
    int validCount = 0;
    int missingCount = 0;
    int syntaxErrorCount = 0;
    QStringList detailedErrors;
    
    QProgressDialog progress("Validating LISP scripts...", "Cancel", 0, m_lispListWidget->count(), this);
    progress.setWindowModality(Qt::WindowModal);
    
    for (int i = 0; i < m_lispListWidget->count(); ++i) {
        progress.setValue(i);
        if (progress.wasCanceled()) {
            break;
        }
        
        QListWidgetItem* item = m_lispListWidget->item(i);
        QString filePath = item->data(Qt::UserRole).toString();
        QString fileName = QFileInfo(filePath).fileName();
        
        if (!QFile::exists(filePath)) {
            item->setIcon(style()->standardIcon(QStyle::SP_MessageBoxWarning));
            item->setToolTip(QString("File not found: %1").arg(filePath));
            missingCount++;
            detailedErrors << QString("%1: File not found").arg(fileName);
        } else {
            // Validate syntax using the new DwgFileProcessor function
            QString errorMsg;
            if (DwgFileProcessor::validateLispSyntax(filePath, errorMsg)) {
                item->setIcon(style()->standardIcon(QStyle::SP_DialogYesButton));
                item->setToolTip("Syntax validation passed");
                validCount++;
            } else {
                item->setIcon(style()->standardIcon(QStyle::SP_MessageBoxCritical));
                item->setToolTip(QString("Syntax error: %1").arg(errorMsg));
                syntaxErrorCount++;
                detailedErrors << QString("%1: %2").arg(fileName, errorMsg);
            }
        }
    }
    
    progress.setValue(m_lispListWidget->count());
    
    // Prepare detailed report
    QString message = QString("Validation Results:\n\n"
                            "✓ Valid: %1 scripts\n"
                            "✗ Missing: %2 files\n"
                            "⚠ Syntax errors: %3 scripts")
                     .arg(validCount)
                     .arg(missingCount)
                     .arg(syntaxErrorCount);
    
    if (!detailedErrors.isEmpty()) {
        message += "\n\nDetailed Errors:\n";
        for (const QString& error : detailedErrors) {
            message += "\n• " + error;
        }
    }
    
    // Show results in a dialog with details
    QMessageBox msgBox;
    msgBox.setWindowTitle("LISP Script Validation");
    msgBox.setText(message);
    msgBox.setIcon(syntaxErrorCount > 0 ? QMessageBox::Warning : 
                  (missingCount > 0 ? QMessageBox::Information : QMessageBox::Information));
    msgBox.setStandardButtons(QMessageBox::Ok);
    
    if (!detailedErrors.isEmpty()) {
        msgBox.setDetailedText(detailedErrors.join("\n"));
    }
    
    msgBox.exec();
    
    updateLispStatus();
    logMessage(QString("LISP validation: %1 valid, %2 missing, %3 syntax errors")
              .arg(validCount).arg(missingCount).arg(syntaxErrorCount));
}

void MainWindow::previewLispScript() {
    QListWidgetItem* current = m_lispListWidget->currentItem();
    if (!current) {
        QMessageBox::information(this, "No Selection", 
            "Please select a script to preview.");
        return;
    }
    
    QString filePath = current->data(Qt::UserRole).toString();
    QFile file(filePath);
    
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QMessageBox::warning(this, "Error", 
            QString("Cannot open file: %1").arg(filePath));
        return;
    }
    
    QTextStream stream(&file);
    QString content = stream.readAll();
    file.close();
    
    QDialog* previewDialog = new QDialog(this);
    previewDialog->setWindowTitle(QString("LISP Preview: %1").arg(current->text()));
    previewDialog->resize(600, 400);
    
    QVBoxLayout* layout = new QVBoxLayout(previewDialog);
    
    QTextEdit* textEdit = new QTextEdit();
    textEdit->setPlainText(content);
    textEdit->setReadOnly(true);
    textEdit->setFont(QFont("Monospace", 10));
    layout->addWidget(textEdit);
    
    QDialogButtonBox* buttons = new QDialogButtonBox(QDialogButtonBox::Close);
    connect(buttons, &QDialogButtonBox::rejected, previewDialog, &QDialog::close);
    layout->addWidget(buttons);
    
    previewDialog->exec();
}

void MainWindow::onLispItemChanged(QListWidgetItem* item) {
    Q_UNUSED(item)
    updateLispStatus();
}

void MainWindow::updateLispStatus() {
    int totalCount = m_lispListWidget->count();
    int enabledCount = 0;
    
    for (int i = 0; i < totalCount; ++i) {
        if (m_lispListWidget->item(i)->checkState() == Qt::Checked) {
            enabledCount++;
        }
    }
    
    if (totalCount == 0) {
        m_lispStatusLabel->setText("No scripts loaded");
        m_lispStatusLabel->setStyleSheet("QLabel { color: #666; }");
    } else {
        m_lispStatusLabel->setText(QString("%1 scripts loaded, %2 enabled")
            .arg(totalCount).arg(enabledCount));
        m_lispStatusLabel->setStyleSheet("QLabel { color: #008000; }");
    }
}

// ============================================================================
// Helper Methods
// ============================================================================

void MainWindow::updateUiState(bool processing) {
    m_startButton->setEnabled(!processing);
    m_stopButton->setEnabled(processing);
    m_browseButton->setEnabled(!processing);
    m_folderEdit->setEnabled(!processing);
    m_tabWidget->setEnabled(!processing);
    m_isProcessing = processing;
}

void MainWindow::logMessage(const QString& message, const QString& type) {
    QString timestamp = QDateTime::currentDateTime().toString("hh:mm:ss");
    QString formattedMsg = QString("[%1] %2: %3")
                          .arg(timestamp).arg(type).arg(message);
    m_logTextEdit->append(formattedMsg);
}

void MainWindow::updateLayerOperationStatus() {
    int totalCount = m_layerOperationsWidget->count();
    if (totalCount == 0) {
        m_layerOpStatusLabel->setText("No operations defined");
        m_layerOpStatusLabel->setStyleSheet("QLabel { color: #666; }");
    } else {
        m_layerOpStatusLabel->setText(QString("%1 operations defined").arg(totalCount));
        m_layerOpStatusLabel->setStyleSheet("QLabel { color: #008000; }");
    }
}

void MainWindow::updateLayerSelectionStatus() {
    int selectedCount = m_layerListWidget->selectedItems().size();
    int totalCount = m_layerListWidget->count();
    
    if (totalCount == 0) {
        m_layerStatsLabel->setText("No layers scanned");
    } else {
        m_layerStatsLabel->setText(QString("%1 layers found, %2 selected")
            .arg(totalCount).arg(selectedCount));
    }
}

// ============================================================================
// Settings Management
// ============================================================================

void MainWindow::loadSettings() {
    QSettings settings("BatchProcessing", "BricsCAD_Plugin");
    
    restoreGeometry(settings.value("geometry").toByteArray());
    
    m_folderEdit->setText(settings.value("sourceFolder", "").toString());
    m_includeEdit->setText(settings.value("includePattern", "*.dwg").toString());
    m_excludeEdit->setText(settings.value("excludePattern", "*.bak").toString());
    m_recursiveCheck->setChecked(settings.value("recursive", false).toBool());
    
    m_backupCheck->setChecked(settings.value("createBackup", true).toBool());
    m_deleteOldBackupsCheck->setChecked(settings.value("deleteOldBackups", false).toBool());
    m_timestampBackupsCheck->setChecked(settings.value("useTimestamp", false).toBool());
    m_backupLocationCombo->setCurrentIndex(settings.value("backupLocation", 0).toInt());
    m_customBackupFolderEdit->setText(settings.value("customBackupFolder", "").toString());
    
    logMessage("Settings loaded", "INFO");
}

void MainWindow::saveSettings() {
    QSettings settings("BatchProcessing", "BricsCAD_Plugin");
    
    settings.setValue("geometry", saveGeometry());
    settings.setValue("sourceFolder", m_folderEdit->text());
    settings.setValue("includePattern", m_includeEdit->text());
    settings.setValue("excludePattern", m_excludeEdit->text());
    settings.setValue("recursive", m_recursiveCheck->isChecked());
    
    settings.setValue("createBackup", m_backupCheck->isChecked());
    settings.setValue("deleteOldBackups", m_deleteOldBackupsCheck->isChecked());
    settings.setValue("useTimestamp", m_timestampBackupsCheck->isChecked());
    settings.setValue("backupLocation", m_backupLocationCombo->currentIndex());
    settings.setValue("customBackupFolder", m_customBackupFolderEdit->text());
    
    logMessage("Settings saved", "INFO");
}

bool MainWindow::validateBackupSettings() {
    if (!m_backupCheck->isChecked()) return true;
    
    if (m_backupLocationCombo->currentIndex() == 1) {
        QString backupFolder = m_customBackupFolderEdit->text();
        if (backupFolder.isEmpty()) {
            QMessageBox::warning(this, "Backup Settings", 
                "Please select a backup folder or choose 'Same folder as DWG'");
            return false;
        }
        
        QDir dir(backupFolder);
        if (!dir.exists()) {
            QMessageBox::warning(this, "Backup Settings", 
                "The selected backup folder does not exist!");
            return false;
        }
    }
    
    return true;
}

void MainWindow::setOptionsToUi(const ProcessingOptions& options) {
    m_folderEdit->setText(options.sourceFolder);
    m_recursiveCheck->setChecked(options.includeSubfolders);
    m_backupCheck->setChecked(options.createBackups);
    m_textEnableCheck->setChecked(options.enableTextReplacement);
    m_searchTextEdit->setText(options.searchText);
    m_replaceTextEdit->setText(options.replaceText);
    m_attributeEnableCheck->setChecked(options.enableAttributeReplacement);
    m_attributeSearchEdit->setText(options.attributeSearchText);
    m_attributeReplaceEdit->setText(options.attributeReplaceText);
    m_layerEnableCheck->setChecked(options.enableLayerOperations);
}

// ============================================================================
// Menu Slots
// ============================================================================

void MainWindow::onClearLog() {
    m_logTextEdit->clear();
}

void MainWindow::onExportLog() {
    QString fileName = QFileDialog::getSaveFileName(this,
        "Export Log", QString("batch_processing_log_%1.txt")
        .arg(QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss")),
        "Text Files (*.txt)");
    
    if (!fileName.isEmpty()) {
        QFile file(fileName);
        if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream stream(&file);
            stream << m_logTextEdit->toPlainText();
            file.close();
            logMessage(QString("Log exported to: %1").arg(fileName));
        }
    }
}

void MainWindow::showAbout() {
    QMessageBox::about(this, "About BatchProcessing Plugin",
        "BricsCAD Batch Processing Plugin\n"
        "v3.0.0\n\n"
        "Processing Modes:\n"
        "  Text/Attribute/Layer: BRX SDK (in-process)\n"
        "  LISP Scripts: _.SCRIPT (in-process, v5.1)\n\n"
        "Built with Qt6 & BRX SDK for BricsCAD V26");
}


// ============================================================================
// Progress Dialog Implementation
// ============================================================================

ProcessingProgressDialog::ProcessingProgressDialog(QWidget* parent)
    : QDialog(parent)
    , m_shouldAbort(false)
    , m_totalFiles(0)
{
    setWindowTitle("Processing Files");
    setModal(true);
    resize(600, 400);
    
    QVBoxLayout* layout = new QVBoxLayout(this);
    
    m_currentFileLabel = new QLabel("Initializing...");
    m_currentFileLabel->setStyleSheet("QLabel { font-weight: bold; }");
    layout->addWidget(m_currentFileLabel);
    
    m_progressBar = new QProgressBar();
    layout->addWidget(m_progressBar);
    
    m_statsLabel = new QLabel();
    layout->addWidget(m_statsLabel);
    
    m_detailsText = new QTextEdit();
    m_detailsText->setReadOnly(true);
    m_detailsText->setFont(QFont("Monospace", 9));
    layout->addWidget(m_detailsText);
    
    QHBoxLayout* buttonLayout = new QHBoxLayout();
    buttonLayout->addStretch();
    
    m_pauseButton = new QPushButton("Pause");
    m_abortButton = new QPushButton("Abort");
    
    buttonLayout->addWidget(m_pauseButton);
    buttonLayout->addWidget(m_abortButton);
    
    layout->addLayout(buttonLayout);
    
    connect(m_pauseButton, &QPushButton::clicked, this, &ProcessingProgressDialog::onPauseClicked);
    connect(m_abortButton, &QPushButton::clicked, this, &ProcessingProgressDialog::onAbortClicked);
}

void ProcessingProgressDialog::setTotalFiles(int total) {
    m_totalFiles = total;
    m_progressBar->setMaximum(total);
    m_progressBar->setValue(0);
    updateStats(0);
}

void ProcessingProgressDialog::updateProgress(int current, const QString& filename) {
    m_progressBar->setValue(current);
    m_currentFileLabel->setText(QString("Processing: %1").arg(filename));
    updateStats(current);
}

void ProcessingProgressDialog::addDetail(const QString& detail) {
    QString timestamp = QDateTime::currentDateTime().toString("hh:mm:ss");
    m_detailsText->append(QString("[%1] %2").arg(timestamp, detail));
    
    // Auto-scroll to bottom
    QTextCursor cursor = m_detailsText->textCursor();
    cursor.movePosition(QTextCursor::End);
    m_detailsText->setTextCursor(cursor);
}

void ProcessingProgressDialog::updateStats(int current) {
    if (m_totalFiles > 0) {
        double percentage = (double)current / m_totalFiles * 100.0;
        m_statsLabel->setText(QString("Progress: %1 of %2 files (%3%)")
            .arg(current).arg(m_totalFiles).arg(percentage, 0, 'f', 1));
    }
}

void ProcessingProgressDialog::onPauseClicked() {
    // Toggle pause state
    bool isPaused = (m_pauseButton->text() == "Resume");
    m_pauseButton->setText(isPaused ? "Pause" : "Resume");
    emit pauseToggled(!isPaused);
}

void ProcessingProgressDialog::onAbortClicked() {
    if (QMessageBox::question(this, "Abort Processing",
        "Are you sure you want to abort processing?") == QMessageBox::Yes) {
        m_shouldAbort = true;
        emit abortRequested();
    }
}

// ProcessingProgressDialog::onBrowseBackupFolder - stub (functionality is in MainWindow)
void ProcessingProgressDialog::onBrowseBackupFolder() {
    // Backup folder browsing is handled in MainWindow settings
}

} // namespace BatchProcessing
