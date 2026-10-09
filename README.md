# batchTool / openCirt – BricsCAD Batch Processing & GA-Planungssoftware Plugin

> **Hinweis (Oktober 2026): Dieses Repository ist archiviert, Stand 1.7.3.** Die Entwicklung geht in zwei getrennten Plugins weiter, beide ab Version 2.0.0:
> - **[openCirt](https://github.com/ohagmann/openCirtPlugin)** – GA-Planungsautomatisierung (Befehl `OPENCIRT`, Kurzform `OC`)
> - **[batchTool](https://github.com/ohagmann/batchToolPlugin)** – DWG-Stapelverarbeitung: Text, Attribute, Layer, LISP (Befehl `BATCHTOOL`, Kurzform `BT`)
>
> Beide lassen sich gleichzeitig laden. Der CHANGELOG bis 1.7.3 bleibt hier und im openCirt-Repository erhalten.

Ein BRX-Plugin (C++/Qt6) für die automatisierte Massenverarbeitung von DWG-Dateien und zur Erstellung von Planungsunterlagen für die Gebäudeautomation (nach VDI3814) in BricsCAD V26 unter Windows und Linux. Statt Zeichnungen einzeln zu öffnen und manuell zu bearbeiten, können wiederkehrende Aufgaben über beliebig viele Dateien in einem Durchgang erledigt werden. Dazu können Texte, Attributwerte, Layer-Operationen und Lisp-Skripte genutzt werden. Im openCirt Tab können außerdem alle Aufgaben für die Erstellung von GA-Automationsschemata inkl. GA-FL erledigt werden. openCirt ist DIE freie GA-Planungssoftware für alle - kostenlos, hocheffizient und einfach zu bedienen.

> ## ⚠️ Hinweis: Bildschirmflackern (Photosensitivität)
>
> Bei Läufen im LISP-Tab und beim PDF-Publish werden Zeichnungen in schneller Folge im sichtbaren BricsCAD-Fenster geöffnet, verarbeitet, gespeichert und geschlossen. Dabei entsteht ein **rasches, großflächiges Flackern** des Bildschirms.
>
> Solche schnellen Hell-Dunkel-Wechsel können bei Menschen mit **photosensitiver Epilepsie** Anfälle auslösen und auch bei nicht betroffenen Personen Unwohlsein, Kopfschmerzen oder Augenbelastung verursachen. Viele Betroffene wissen nichts von ihrer Empfindlichkeit, bis ein Anfall auftritt.
>
> **Empfehlung:** Während eines laufenden Batch- oder Publish-Vorgangs nicht dauerhaft auf den Bildschirm schauen, das Fenster minimieren oder den Arbeitsplatz verlassen. Personen mit bekannter Photosensitivität sollten den Lauf nicht beobachten.
>
> Technischer Hintergrund: BricsCAD bietet für diese Verarbeitung keinen vollständig unsichtbaren (headless) Modus; das Skript läuft im Vordergrund-Editor, weshalb der Bildaufbau sichtbar ist. Text-, Attribut- und Layer-Operationen sowie – seit Version 1.7 – alle Schritte von „Projekt aufbauen" und „Projekt erstellen" laufen dagegen datenbankseitig ohne Bildaufbau und flackern nicht.

## Features

Das Plugin bietet sechs Funktionsbereiche als Tabs im Hauptfenster:

**General** – Quellordner, Dateifilter, Backup-Konfiguration
**Text** – Suchen/Ersetzen in DBText und MText (inkl. Regex, Mehrfachersetzung)
**Attributes** – Blockattribute gezielt ändern (nach Block, Tag, Sichtbarkeit filterbar)
**Layers** – Layer löschen, umbenennen, einfrieren, Farbe/Linientyp/Transparenz ändern
**LISP** – Eigene LISP-Skripte automatisiert auf alle DWG-Dateien anwenden
**openCirt** – GA-Planungsautomatisierung (Projektaufbau aus der Erstellliste, Plankopf, BMK, BAS, GA-FL, Summenblätter, Deckblätter, Inhaltsverzeichnis, Sensorliste, Datenpunkt-/IO-Export, PDF-Publish)

## Voraussetzungen

Windows:

- **BricsCAD V26** (Windows, 64-Bit)
- **BRX SDK V26** (separat von Bricsys zu beziehen, siehe unten)
- **Qt 6.8+** (MSVC 2022, 64-Bit)
- **CMake 3.20+**
- **Visual Studio 2022** (MSVC v143 Toolset)

Linux:

- **BricsCAD V26** (64-Bit, getestet mit V26.2.07 unter Ubuntu)
- **BRX SDK V26** – dasselbe SDK wie unter Windows, die Header sind plattformneutral
- **Qt 6.8.2** (gcc_64) – genau die Version, die BricsCAD mitbringt. Das Plugin läuft im BricsCAD-Prozess und benutzt dessen Qt-Bibliotheken; das SDK wird nur zum Bauen gebraucht
- **CMake 3.20+**, **Ninja**, **g++** mit C++17
- OpenGL-Entwicklerdateien (`libgl-dev` oder gleichwertig), die Qt beim Konfigurieren verlangt

### BRX SDK

Das BRX SDK ist proprietär und wird von Bricsys bereitgestellt. Es ist nicht Teil dieses Repositories. Nach dem Bezug muss das SDK unter `external/brx_sdk/` abgelegt werden, sodass die Struktur wie folgt aussieht:

```
external/
  brx_sdk/
    inc/         ← Header-Dateien
    inc64/
    lib64/       ← brx26.lib etc.
    docs/
```

Das SDK kann über das Bricsys Developer Network bezogen werden: https://www.bricsys.com/en-eu/developers

## Build

```cmd
CLEAN_BUILD.bat
```

Das Skript führt folgende Schritte aus:
1. Beendet laufende BricsCAD-Instanzen – solange BricsCAD läuft, hält es das geladene Plugin geöffnet und der Build scheitert am Linker. Zuerst wird BricsCAD regulär zum Beenden aufgefordert (Speichern-Rückfragen erscheinen wie gewohnt), erst nach 30 Sekunden folgt eine Rückfrage zum harten Beenden. `CLEAN_BUILD.bat /force` überspringt diese Rückfrage
2. Löscht alte Build-Artefakte
3. CMake-Konfiguration (Visual Studio 17 2022, x64, Release)
4. MSBuild-Kompilierung

Das fertige Plugin liegt anschließend unter `build_windows\Release\batchtool-<Version>.brx` (die Version stammt aus dem obersten Abschnitt von `CHANGELOG.md`, z.B. `batchtool-1.7.1.brx`) und wird zusätzlich nach `sample_project/00- BricsCAD Plugin/00- Windows Version/` gelegt; ältere Stände dort werden entfernt.

### Linux

```sh
./CLEAN_BUILD.sh
```

Das Skript löscht `build_linux`, konfiguriert mit CMake (Generator Ninja, Release) und baut. Das fertige Plugin liegt unter `build_linux/Release/batchtool-<Version>.lrx` (z.B. `batchtool-1.7.1.lrx`) und wird zusätzlich nach `sample_project/00- BricsCAD Plugin/01- Linux Version/` gelegt; ältere Stände dort werden entfernt. Ein laufendes BricsCAD wird nicht beendet; es behält die geladene Fassung, die neue gilt nach dem nächsten Start.

Alles Nötige lässt sich ohne Systemrechte im Benutzerverzeichnis einrichten. Vorgabe ist `~/.local/opt/opencirt-toolchain`:

```sh
TC=~/.local/opt/opencirt-toolchain
python3 -m venv $TC/venv
$TC/venv/bin/pip install cmake ninja aqtinstall
$TC/venv/bin/aqt install-qt linux desktop 6.8.2 linux_gcc_64 -O $TC/Qt
```

Fehlen die OpenGL-Entwicklerdateien im System, genügt es, die Pakete herunterzuladen und nach `$TC/sysroot` zu entpacken (`apt-get download libgl-dev libglx-dev libopengl-dev libegl-dev libgles-dev libglvnd-dev libvulkan-dev libxkbcommon-dev libx11-dev x11proto-dev`, dann je Paket `dpkg -x <paket>.deb $TC/sysroot`).

### Manuelle Build-Schritte

```cmd
mkdir build_windows
cd build_windows
cmake -G "Visual Studio 17 2022" -A x64 -DCMAKE_BUILD_TYPE=Release ..
cmake --build . --config Release
```

### Qt- und BricsCAD-Pfade anpassen

Die Vorgaben stehen in der Root-`CMakeLists.txt`:

| Variable | Windows | Linux |
|---|---|---|
| `QT6_DIR` | `C:/Qt/6.8.3/msvc2022_64` | `~/.local/opt/opencirt-toolchain/Qt/6.8.2/gcc_64` |
| `BRICSCAD_DIR` | `C:/Program Files/Bricsys/BricsCAD V26 de_DE` | `/opt/bricsys/bricscad/v26` |
| `OC_SYSROOT` | – | `~/.local/opt/opencirt-toolchain/sysroot` (optional) |

Abweichende Pfade lassen sich ohne Änderung der Datei setzen: beim Aufruf mit `cmake -DQT6_DIR=… -DBRICSCAD_DIR=…` oder über gleichnamige Umgebungsvariablen.

## Installation in BricsCAD

### Einmalig (zum Testen)

1. BricsCAD starten
2. Befehl: `APPLOAD`
3. Zur Datei `batchtool-<Version>.brx` (Linux: `batchtool-<Version>.lrx`) navigieren und laden
4. In der Kommandozeile erscheint: *"Batch Processing Plugin geladen. Befehl: BATCHTOOL"*

### Automatisch bei jedem Start

1. `APPLOAD` aufrufen
2. Unten auf *"Inhalt..."* (Startup Suite) klicken
3. `batchtool-<Version>.brx` (Linux: `batchtool-<Version>.lrx`) zur Startup Suite hinzufügen. Nach einem Versionswechsel den Eintrag auf die neue Datei umstellen – der Dateiname trägt die Version.

## Befehle

| Befehl | Beschreibung |
|---|---|
| `BATCHTOOL` | Öffnet das Hauptfenster |

## Bedienung

### Grundsätzlicher Ablauf

1. Im Tab **General** den Quellordner mit DWG-Dateien auswählen
2. In einem oder mehreren Tabs die gewünschten Operationen konfigurieren
3. **Start** klicken
4. Fortschritt im **Processing Log** am unteren Fensterrand beobachten – dort laufen die Meldungen aller Tabs zusammen

Das Fenster übernimmt BricsCADs Hell-/Dunkeleinstellung. Maßgeblich ist die Systemvariable `COLORTHEME`; sie wird bei jedem Aufruf von `BATCHTOOL` neu gelesen. Nach einem Themenwechsel genügt es also, das Fenster zu schließen und den Befehl erneut aufzurufen.

### Tab: General

Legt fest, welche Dateien verarbeitet werden: Quellordner, Include/Exclude-Filter, Unterordner-Option und Backup-Einstellungen (Speicherort, Zeitstempel, alte Backups löschen).

### Tab: Text

Suchen/Ersetzen in allen Textobjekten. Unterstützt Regex, Groß-/Kleinschreibung, ganze Wörter und eine Ersetzungstabelle für mehrere Paare gleichzeitig. Texttypen (einzeilig, mehrzeilig, Bemaßung, Leader) sind einzeln aktivierbar.

### Tab: Attributes

Ändert Attributwerte in Block-Referenzen. Filterbar nach Blockname und Attribut-Tag. Ein leeres Suchfeld überschreibt den kompletten Attributwert. Optionen für unsichtbare, konstante und verschachtelte Attribute.

### Tab: Layers

Layer-Operationen werden als Liste definiert und in Reihenfolge ausgeführt. Schnelloperationen: Löschen (inkl. Entitäten), Einfrieren, Farbe ändern (AutoCAD-Farbgrid), Umbenennen. Die Layer-Analyse scannt alle DWGs und listet vorhandene Layer auf.

### Tab: LISP

Führt LISP-Skripte automatisiert auf alle DWGs aus. Das Plugin erzeugt eine SCR-Datei und führt sie über `_.SCRIPT` in der aktuellen BricsCAD-Instanz aus.

**Wichtige Konvention:** Der Funktionsname im LISP-Skript muss dem Dateinamen (ohne `.lsp`) entsprechen.

```
Datei: sk3.lsp → muss Funktion (defun sk3 ...) enthalten
```

Das Plugin generiert pro DWG:
```
_.OPEN "datei.dwg"
(progn (load "sk3.lsp")(princ))
(progn (sk3)(princ))
_QSAVE
_.CLOSE
```

LISP-Vorlage:
```lisp
;;; mein_skript.lsp
(defun mein_skript ( / )
  (command "_.LAYER" "_Make" "Neu" "")
  (princ "\nmein_skript: Fertig.\n")
  (princ)
)
```

Hinweise:
- Das abschließende `(princ)` verhindert unerwünschte Ausgaben in die Kommandozeile.
- Die aktuell geöffnete Zeichnung darf nicht in der Batch-Liste enthalten sein.
- Während der Verarbeitung BricsCAD nicht manuell bedienen.

### Tab: openCirt

GA-Planungsautomatisierung (Gebäudeautomation) für TGA-Projekte. Alle Funktionen des Tabs außer dem Plotten bearbeiten die Zeichnungsdateien direkt als Side-Database – ohne sie im Editor zu öffnen, ohne LISP, unter Windows und Linux gleich. Funktionen:

Der Tab hat sechs Schaltflächen. Die eigentliche Projekterstellung läuft über einen einzigen Knopf – Plankopf, Deckblätter, BMK, BAS, GA-FL, Summen und Textbreiten sind Schritte darin und werden nicht mehr einzeln bedient.

- **Projekt aufbauen** – erzeugt die Quellzeichnungen aus der Erstellliste (CSV): Vorlage kopieren, nach Los / ASP / Gewerk / Anlage einsortieren, Attribute setzen, Stempel und Meldungsblöcke füllen. Wahlweise als Vorschau, die nur das Log schreibt. Arbeitet ohne LISP direkt auf den Zeichnungsdateien (`ProjectBuilder`) und ersetzt das LISP-Skript `OC_PROJECT_BUILD`. Beschreibung der Liste: Bedienungsanleitung Abschnitt 3.3
- **Projekt erstellen** – der Gesamtlauf in korrekter Reihenfolge:
  - *Plankopf* – CSV-basierte Plankopf-Attribute setzen (AG, AN, PR etc.)
  - *Deckblätter* – für die Los/ASP/Gewerk/Anlage-Hierarchie, inkl. ASP, Gewerk und Anlage aus der Ordnerstruktur
  - *BMK-Nummerierung* – Betriebsmittelkennzeichen automatisch vergeben
  - *BAS-Generierung* – Bauautomationssystem aus BAS.csv erzeugen
  - *GA-FL* – Funktionslisten zweiphasig: Datenextraktion aus Quell-DWGs, dann GA-FL-Blätter erzeugen und füllen
  - *Summenblätter* – Gewerk-Summe, ASP-Summe, Los-Summe, Projekt-Summe sowie eine Gewerke-Auswertung je Los über alle ASPs
  - *Textbreiten* – Breitenfaktor in GA-FL- und Summenblättern korrigieren, auch für Werte innerhalb der GA-FL-Blockdefinition
- **Projekt bereinigen** – temporäre Dateien und Backups im Zeichnungsordner löschen (`*.bak`, `*.dwl`, `*.dwl2`, `*.sv$`, `*.ac$`, `*.tmp`, `*.log`)
- **PDF publizieren** – DSD-basierter Multi-Sheet-PDF-Export inkl. Inhaltsverzeichnis (22 Einträge pro Seite, Plankopf aus `plankopfdaten.csv`). Das Plotten läuft in einer eigenen Batch-Instanz von BricsCAD
- **IO-Liste erstellen** – Export als CSV-Datei nach `06- Plot` (`CsvListWriter`, Referenz: `iomodule.csv`). Im Dialog wird nach Integrationsart gefiltert (Attribut `OC_INTEGRATIONSART_DP_n`): leer = alle Datenpunkte (`Datenpunktliste.csv`), `HW` = SPS-/DDC-Belegungsliste mit Modul- und Kanalzuordnung (`IO-Belegungsliste.csv`), `BUS;SMI` = mehrere Arten (`Datenpunktliste_BUS-SMI.csv`). Die Integrationsart steht als eigene Spalte in der Liste; *Modul-Typ* wird nur für HW-Zeilen gefüllt
- **Sensorliste erstellen** – Keyword-Matching gegen Blockattribute (`SensorKeywordLoader`, Referenz: `sensor.csv`), Ausgabe als `Sensorliste.csv` nach `06- Plot`

Die Listen sind CSV-Dateien in UTF-8 mit BOM, Trennzeichen Semikolon, Zeilenende CRLF. Zeile 1 ist die Kopfzeile. LibreOffice und Excel öffnen sie direkt; eine Vorlage ist nicht nötig.

## Projektstruktur

```
├── CMakeLists.txt              Root-Build-Konfiguration
├── CLEAN_BUILD.bat             Build-Skript Windows
├── CLEAN_BUILD.sh              Build-Skript Linux
├── KNOWN_ISSUES.md             Bekannte Probleme in BricsCAD (Windows: GDI-Objekt-Leck, Linux: LISP) – betreffen seit 1.7 nur den LISP-Tab
├── LICENSE                     BSL 1.1 Lizenz
├── .gitignore
├── docs/
│   ├── DEVELOPMENT.md          Entwickler-Hinweise
│   └── bricscad-linux-bugs/    Fehlerberichte zu BricsCAD für Linux, mit Skripten zum Nachstellen
├── tools/
│   └── Close-BricsCAD.ps1      Beendet BricsCAD vor dem Build
├── external/
│   └── brx_sdk/                BRX SDK (nicht im Repository)
├── sample_project/
│   ├── 00- BricsCAD Plugin/    Kompiliertes Plugin (batchtool-<Version>.brx für Windows, batchtool-<Version>.lrx für Linux)
│   ├── 01- Referenzen/
│   │   ├── BAS.csv             BAS-Konfiguration
│   │   ├── GA_FL_VORLAGE.ods   GA-FL Vorlage
│   │   ├── Erstellliste_VORLAGE.csv Erstellliste mit allen Spalten und Beispielzeilen
│   │   ├── iomodule.csv        IO-Modul-Referenzdaten
│   │   ├── opencirt_config.json Projektkonfiguration
│   │   ├── plankopfdaten.csv   Plankopf-Attribute
│   │   └── sensor.csv          Sensor-Referenzdaten
│   ├── 02- Skripte/            LISP-Skripte (Vorlage der Schritte des Gesamtlaufs, seit 1.7 nur noch für den LISP-Tab)
│   ├── 03- Blockbibliothek/    DWG-Blockvorlagen
│   ├── 04- Vorlagen/
│   │   ├── OC_RSH_Plankopf_quer_V21.dwg
│   │   ├── OC_VORLAGE_DIN_A0.dwg
│   │   ├── OC_VORLAGE_DIN_A2_V14.dwg
│   │   ├── OC_VORLAGE_DIN_A2_INHALTSVERZEICHNIS_V1.dwg
│   │   ├── OC_VORLAGE_DIN_A2_HISTORIE_V1.dwg
│   │   ├── OC_VORLAGE_GA_FL.dwg
│   │   └── VDI3814_GA_FL_V_1_0.dwg
│   ├── 05- Projekt Zeichnungen/
│   ├── 06- Plot/
│   └── BEDIENUNGSANLEITUNG.md  Bedienungsanleitung (12 Kapitel)
└── src/
    ├── windows_fix.h           Qt 6.8+ / Windows SDK Kompatibilität
    ├── brx_force_include.h     BRX Platform-Header
    ├── core/
    │   ├── DwgProcessor.cpp/h      DWG-Verarbeitungslogik (Text, Attribute, Layer)
    │   ├── LispProcessExecutor.cpp/h   In-Process LISP-Ausführung via _.SCRIPT
    │   ├── OpenCirtEngine.cpp/h    Schritte des Gesamtlaufs auf der Side-Database (BMK, BAS, Extraktion, GA-FL, Textbreiten)
    │   └── ProjectBuilder.cpp/h    Projektaufbau aus der Erstellliste
    ├── data/
    │   ├── ProcessingOptions.h         Datenstrukturen und Optionen
    │   ├── ProcessingOptionsImpl.cpp   LISP Script Manager
    │   └── Configuration.cpp           Settings-Persistenz
    ├── mfc_stubs/              Leere MFC/ATL-Stubs (Qt-basiert, kein MFC; nur Windows)
    ├── plugin/
    │   ├── BatchProcessingPlugin.cpp/h   BRX Entry Point (acrxEntryPoint)
    │   └── Commands.cpp/h              Befehlsregistrierung
    ├── ui/
    │   ├── MainWindow.cpp/h            Hauptfenster mit Tab-Verwaltung
    │   ├── OpenCirtTab.cpp/h           GA-Planungsautomatisierung
    │   ├── Theming.cpp/h               Hell-/Dunkelthema aus BricsCAD COLORTHEME
    │   └── widgets/
    │       └── AcadColorGrid.cpp/h     AutoCAD-Farbauswahl-Widget
    └── utils/
        ├── Logger.h                    Logging-Hilfsfunktionen
        ├── SensorKeywordLoader.cpp/h   Keyword-Abgleich für die Sensorliste
        └── CsvListWriter.cpp/h         Listen als CSV schreiben
```

## Hinweise

- Vor dem ersten produktiven Einsatz immer mit aktivierten Backups arbeiten.
- LISP-Skripte vorher manuell an einer einzelnen DWG testen.
- Layer-Analyse vor Layer-Operationen durchführen, um Tippfehler zu vermeiden.
- Unter Linux läuft der LISP-Tab nur mit leichten Skripten zuverlässig – Ursache sind Fehler in BricsCAD für Linux, siehe [KNOWN_ISSUES.md](KNOWN_ISSUES.md) Abschnitt 2. Alle übrigen Funktionen laufen unter Windows und Linux gleich.
- Für „Projekt aufbauen" und „Projekt erstellen" darf keine Zeichnung des Projekts in BricsCAD geöffnet sein.
- Mehrere Tabs können gleichzeitig aktiviert sein. Die Verarbeitung erfolgt in der Reihenfolge: Text → Attribute → Layer → LISP.

## Lizenz

Business Source License 1.1 (BSL 1.1) – siehe [LICENSE](LICENSE) und [ADDITIONAL_TERMS](ADDITIONAL_TERMS).

Kurzfassung: Nutzung für interne Zwecke, kommerzielle Projekte und Dienstleistungen ist erlaubt. Verkauf als eigenständiges Produkt, SaaS-Angebote und proprietäre Forks sind untersagt. Ab dem Change Date (2030-03-02) wird die Software unter AGPLv3 verfügbar. Nutzung auf eigenes Risiko – vor jedem Batch-Lauf Backups erstellen!

## Technologie

Entwickelt mit BRX SDK V26, Qt 6, C++17, CMake.
