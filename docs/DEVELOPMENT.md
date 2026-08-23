# Entwickler-Hinweise

## Architektur

Das Plugin ist als BRX-Modul (.brx) aufgebaut, das als Shared Library in BricsCAD geladen wird. Die GUI basiert auf Qt6 (nicht MFC), was eine moderne Widget-Bibliothek und Signal/Slot-Kommunikation ermöglicht.

### Schichtenmodell

```
┌─────────────────────────────────────────┐
│  BricsCAD V26 (Host-Applikation)        │
├─────────────────────────────────────────┤
│  plugin/   BRX Entry Point              │
│            acrxEntryPoint, Commands      │
├─────────────────────────────────────────┤
│  ui/       Qt6 GUI                      │
│            MainWindow (Tabs),            │
│            OpenCirtTab (GA-Automation),  │
│            Theming (COLORTHEME)          │
├─────────────────────────────────────────┤
│  core/     Verarbeitungslogik           │
│            DwgProcessor (BRX API),       │
│            LispProcessExecutor (SCR)     │
├─────────────────────────────────────────┤
│  data/     Datenstrukturen              │
│            ProcessingOptions,            │
│            LispScriptManager             │
└─────────────────────────────────────────┘
```

### Kernkomponenten

**BatchProcessingEngine** (`core/DwgProcessor.cpp`) – Orchestriert die Batch-Verarbeitung. Öffnet DWG-Dateien über die BRX-Seitendatenbank (AcDbDatabase), führt Text-/Attribut-/Layer-Operationen durch und speichert die Änderungen. LISP-Verarbeitung wird an den LispProcessExecutor delegiert.

**LispProcessExecutor** (`core/LispProcessExecutor.cpp`) – Generiert eine SCR-Datei mit dem Standard-CAD-Batch-Pattern (`_.OPEN → load → call → _QSAVE → _.CLOSE`) und führt sie über `acedCommand(_.SCRIPT)` in der aktuellen BricsCAD-Instanz aus. Systemvariablen (FILEDIA, CMDECHO, EXPERT) werden vor der Ausführung via `acedSetVar` gesetzt und am Ende der SCR wiederhergestellt.

**OpenCirtTab** (`ui/OpenCirtTab.cpp`) – GA-Planungsautomatisierung. Generiert inline LISP-Code als Strings in SCR-Dateien für Plankopf-, BMK-, BAS- und GA-FL-Workflows. Verwendet VLA-Objekte für Attributmanipulation und Marker-Dateien für asynchrone Phasen-Koordination (Phase 1: Extraktion → Phase 2: Generierung via Qt-Timer-Polling).

**MainWindow** (`ui/MainWindow.cpp`) – Qt6-Hauptfenster mit QTabWidget. Jeder Tab konfiguriert einen Verarbeitungsbereich. Die Verarbeitung wird über die BatchProcessingEngine gestartet, Fortschritt über Signals/Slots kommuniziert. Das **Processing Log** am unteren Rand ist die einzige Protokollansicht; Tabs schreiben über das Signal `logMessage` hinein statt eine eigene zu führen.

**Theming** (`ui/Theming.cpp`) – Gleicht die Oberfläche an BricsCADs Hell-/Dunkeleinstellung an. Zwei Punkte sind dabei nicht offensichtlich:

*BricsCADs Oberfläche ist MFC-basiert, nicht Qt.* Es gibt also keine Qt-Palette des Hosts zu erben. Maßgeblich ist stattdessen die Systemvariable `COLORTHEME` (0 = dunkel, 1 = hell), gelesen über `acedGetVar`.

*Der Stil muss Fusion sein.* Der ab Qt 6.8 auf Windows 11 voreingestellte `windows11`-Stil zeichnet Flächen, Rahmen und abgerundete Ecken selbst und ignoriert die Palette weitgehend – eine dunkle Palette bliebe dort wirkungslos. Fusion respektiert die Palette vollständig und zeichnet eckig, was zugleich BricsCADs Erscheinungsbild entspricht.

Farben werden nirgends fest verdrahtet. Beschriftungen bekommen über `Theming::setRole()` eine Rolle (`Muted`, `Success`, `Warning`, `ErrorBold` …), die als Qt-Property am Widget hängt. `Theming::apply()` rechnet alle Rollen neu durch – deshalb folgt auch ein bereits gebautes Fenster einem Themenwechsel. `MainWindow::applyTheme()` ergänzt das um ein Neuzeichnen des Protokolls, dessen Farben als HTML im Text stecken und sich sonst nicht mehr ändern ließen.

Aufgerufen wird `applyTheme()` bei jedem `BATCHTOOL` – ein Themenwechsel greift also nach Schließen und erneutem Öffnen des Fensters.

### Qt 6.8+ Windows-Kompatibilität

Die Datei `src/windows_fix.h` wird über `/FI` (Force-Include) in alle Kompilierungseinheiten eingebunden, einschließlich MOC-generierter Dateien. Sie löst Konflikte zwischen Qt 6.8+ internen Windows-SDK-Includes und den BRX-SDK-Headern.

### MFC-Stubs

Das Verzeichnis `src/mfc_stubs/` enthält leere Header-Dateien (`afxwin.h`, `afxext.h` etc.), die BRX-SDK-Includes befriedigen, ohne MFC-Abhängigkeiten einzuführen. Das Plugin verwendet Qt6 statt MFC.

## Build-Konfiguration

Die Root-`CMakeLists.txt` ist die einzige Build-Datei. Sie definiert:
- Qt6-Pfad, BRX-SDK-Pfad, BricsCAD-Installationspfad
- Compiler-Flags inkl. Force-Include von windows_fix.h
- Alle Source-/Header-Dateien explizit (kein GLOB) – neue Dateien müssen in `PLUGIN_SOURCES` bzw. `PLUGIN_HEADERS` eingetragen werden
- Linker-Konfiguration gegen brx26.lib und Qt6

### Lokale Pfade anpassen

Drei Variablen in `CMakeLists.txt` müssen an die lokale Umgebung angepasst werden:

```cmake
set(QT6_DIR "C:/Qt/6.8.3/msvc2022_64")        # Qt6 Installation
set(BRX_SDK_PATH "${CMAKE_CURRENT_SOURCE_DIR}/external/brx_sdk")  # BRX SDK
set(BRICSCAD_DIR "C:/Program Files/Bricsys/BricsCAD V26 de_DE")   # BricsCAD
```

### Build und laufendes BricsCAD

Solange BricsCAD läuft, hält es `batchtool.brx` geöffnet; MSBuild kann die Datei dann nicht ersetzen und der Build bricht mit einem Linker-Fehler ab. `CLEAN_BUILD.bat` ruft deshalb in Schritt 0 `tools/Close-BricsCAD.ps1` auf. Das Skript fordert BricsCAD zunächst regulär zum Beenden auf, sodass Speichern-Rückfragen wie gewohnt erscheinen, und fragt erst nach 30 Sekunden nach hartem Beenden. Mit `-Force` beziehungsweise `CLEAN_BUILD.bat /force` entfällt die Rückfrage.

Zu beachten: der Startup-Suite-Eintrag zeigt üblicherweise auf `build_windows/Release/batchtool.brx` – genau den Ordner löscht Schritt 1. Zwischen Build-Start und Build-Ende sollte BricsCAD daher nicht gestartet werden.

## Konventionen

- C++17 Standard
- Qt-Coding-Conventions (camelCase für Methoden, m_-Prefix für Member)
- Deutsche Kommentare in domänenspezifischem Code (GA-Planung)
- LISP-Funktionsname = Dateiname ohne Extension
- SCR-Dateien: keine Leerzeilen (Leerzeile = ENTER = stört Kommandos)
- Keine festen Farbwerte in der Oberfläche – Farben über `Theming::setRole()` oder die abgeleiteten Funktionen aus `Theming.h` beziehen, sonst bricht die Themenumschaltung
