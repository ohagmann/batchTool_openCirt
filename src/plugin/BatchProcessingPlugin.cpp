#include "windows_fix.h"  // CRITICAL: Qt 6.8+ fix - MUST be FIRST
/**
 * @file BatchProcessingPlugin.cpp
 * @brief BRX Plugin Entry Point and Registration
 */

// WICHTIG: Platform header MUSS zuerst kommen!
#ifdef __linux__
#include "brx_platform_linux.h"
#else
#include "brx_platform_windows.h"
#endif

// Dann erst die anderen BRX headers
#include "aced.h"
#include "AcRx/AcRxDynamicLinker.h"
#include "BatchProcessingPlugin.h"
#include "Commands.h"
#include "../ui/MainWindow.h"
#include "../ui/OpenCirtTab.h"
#include "../ui/Theming.h"

#include <QApplication>
#include <QDebug>

// Global variables
static bool g_isInitialized = false;
static QApplication* g_qApp = nullptr;
static BatchProcessing::MainWindow* g_mainWindow = nullptr;

// ============================================================================
// BRX Entry Points
// ============================================================================

extern "C" BRX_EXPORT
AcRx::AppRetCode acrxEntryPoint(AcRx::AppMsgCode msg, void* pAppId)
{
    switch (msg) {
        case AcRx::kInitAppMsg:
            // Registriere die Applikation
            acrxDynamicLinker->unlockApplication(pAppId);
            acrxDynamicLinker->registerAppMDIAware(pAppId);
            
            // Initialisiere Qt falls noch nicht geschehen
            if (!g_qApp) {
                static int argc = 1;
                static char* argv[] = { (char*)"BatchProcessing", nullptr };
                g_qApp = new QApplication(argc, argv);
                g_qApp->setApplicationName("BatchProcessing");
                g_qApp->setOrganizationName("openCirt");
            }
            
            // Registriere Commands
            BatchProcessing::Commands::registerCommands();
            
            acutPrintf(_T("\nBatch Processing Plugin geladen. Befehl: BATCHTOOL\n"));
            g_isInitialized = true;
            break;
            
        case AcRx::kUnloadAppMsg:
            // Cleanup
            if (g_mainWindow) {
                g_mainWindow->close();
                delete g_mainWindow;
                g_mainWindow = nullptr;
            }
            
            // Unregister commands
            BatchProcessing::Commands::unregisterCommands();
            
            if (g_qApp) {
                delete g_qApp;
                g_qApp = nullptr;
            }
            
            g_isInitialized = false;
            acutPrintf(_T("\nBatch Processing Plugin entladen.\n"));
            break;
            
        case AcRx::kLoadDwgMsg:
            // Neue Zeichnung geladen
            break;
            
        case AcRx::kUnloadDwgMsg:
            // Zeichnung wird entladen
            break;
            
        case AcRx::kInvkSubrMsg:
            // Kommando wurde aufgerufen
            break;
            
        default:
            break;
    }
    
    return AcRx::kRetOK;
}

// API Version - provided by drx_entrypoint.lib, do NOT define here
// extern "C" int acrxGetApiVersion() { return ACRX_APIVERSION; }

// ============================================================================
// Command Implementations
// ============================================================================

namespace BatchProcessing {
namespace Commands {

void registerCommands() {
    acedRegCmds->addCommand(
        _T("BATCH_PROCESSING_CMDS"),
        _T("BATCHTOOL"),
        _T("BATCHTOOL"),
        ACRX_CMD_MODAL,
        batchProcessCommand
    );

    // Wird vom Phase-2-Skript des Gesamtlaufs aufgerufen, sobald das letzte
    // GA-FL-Blatt gespeichert ist (siehe OpenCirtTab::preparePhase3).
    acedRegCmds->addCommand(
        _T("BATCH_PROCESSING_CMDS"),
        _T("OC_PHASE3_PREPARE"),
        _T("OC_PHASE3_PREPARE"),
        ACRX_CMD_MODAL,
        phase3PrepareCommand
    );
}

// OC_PHASE3_PREPARE: Summen-Phase des Gesamtlaufs vorbereiten
void phase3PrepareCommand() {
    if (!g_mainWindow || !g_mainWindow->openCirtTab()) {
        acutPrintf(_T("\nOC_PHASE3_PREPARE: Batchtool-Fenster nicht initialisiert.\n"));
        return;
    }
    g_mainWindow->openCirtTab()->preparePhase3();
}

void unregisterCommands() {
    acedRegCmds->removeGroup(_T("BATCH_PROCESSING_CMDS"));
}

// BATCHPROCESS Command Implementation
void batchProcessCommand() {
    if (!g_isInitialized) {
        acutPrintf(_T("\nPlugin nicht korrekt initialisiert!\n"));
        return;
    }
    
    acutPrintf(_T("\nStarte Batch Processing...\n"));
    
    // Create and show main window
    if (!g_mainWindow) {
        g_mainWindow = new BatchProcessing::MainWindow();
    }

    // Thema bei jedem Oeffnen neu anwenden: schaltet der Anwender BricsCAD
    // zwischenzeitlich zwischen hell und dunkel um, folgt das Fenster.
    g_mainWindow->applyTheme();

    g_mainWindow->show();
    g_mainWindow->raise();
    g_mainWindow->activateWindow();
    
    // Process Qt events
    if (g_qApp) {
        g_qApp->processEvents();
    }
}

} // namespace Commands
} // namespace BatchProcessing

// ============================================================================
// Additional Helper Functions
// ============================================================================

bool isPluginLoaded() {
    return g_isInitialized;
}

QApplication* getQApplication() {
    return g_qApp;
}
