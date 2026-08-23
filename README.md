# batchTool / openCirt – BricsCAD Batch Processing & GA-Planungssoftware Plugin

Ein BRX-Plugin (C++/Qt6) für die automatisierte Massenverarbeitung von DWG-Dateien und zur Erstellung von Planungsunterlagen für die Gebäudeautomation (nach VDI3814) in BricsCAD V26. Statt Zeichnungen einzeln zu öffnen und manuell zu bearbeiten, können wiederkehrende Aufgaben über beliebig viele Dateien in einem Durchgang erledigt werden. Dazu können Texte, Attributwerte, Layer-Operationen und Lisp-Skripte genutzt werden. Im openCirt Tab können außerdem alle Aufgaben für die Erstellung von GA-Automationsschemata inkl. GA-FL erledigt werden. openCirt ist DIE freie GA-Planungssoftware für alle - kostenlos, hocheffizient und einfach zu bedienen.

> ## ⚠️ Hinweis: Bildschirmflackern (Photosensitivität)
>
> Bei Batch-Läufen (LISP-Verarbeitung) und beim PDF-Publish werden Zeichnungen in schneller Folge im sichtbaren BricsCAD-Fenster geöffnet, verarbeitet, gespeichert und geschlossen. Dabei entsteht ein **rasches, großflächiges Flackern** des Bildschirms.
>
> Solche schnellen Hell-Dunkel-Wechsel können bei Menschen mit **photosensitiver Epilepsie** Anfälle auslösen und auch bei nicht betroffenen Personen Unwohlsein, Kopfschmerzen oder Augenbelastung verursachen. Viele Betroffene wissen nichts von ihrer Empfindlichkeit, bis ein Anfall auftritt.
>
> **Empfehlung:** Während eines laufenden Batch- oder Publish-Vorgangs nicht dauerhaft auf den Bildschirm schauen, das Fenster minimieren oder den Arbeitsplatz verlassen. Personen mit bekannter Photosensitivität sollten den Lauf nicht beobachten.
>
> Technischer Hintergrund: BricsCAD bietet für diese Verarbeitung keinen vollständig unsichtbaren (headless) Modus; das Skript läuft im Vordergrund-Editor, weshalb der Bildaufbau sichtbar ist. Reine Text-, Attribut- und Layer-Operationen laufen dagegen datenbankseitig ohne Bildaufbau und flackern nicht.

## Features

Das Plugin bietet sechs Funktionsbereiche als Tabs im Hauptfenster:

**General** – Quellordner, Dateifilter, Backup-Konfiguration
**Text** – Suchen/Ersetzen in DBText und MText (inkl. Regex, Mehrfachersetzung)
**Attributes** – Blockattribute gezielt ändern (nach Block, Tag, Sichtbarkeit filterbar)
**Layers** – Layer löschen, umbenennen, einfrieren, Farbe/Linientyp/Transparenz ändern
**LISP** – Eigene LISP-Skripte automatisiert auf alle DWG-Dateien anwenden
**OpenCirt** – GA-Planungsautomatisierung (Plankopf, BMK, BAS, GA-FL, Summenblätter, Deckblätter, Inhaltsverzeichnis, Sensorliste, Datenpunkt-/IO-Export, PDF-Publish)

## Voraussetzungen

- **BricsCAD V26** (Windows, 64-Bit)
- **BRX SDK V26** (separat von Bricsys zu beziehen, siehe unten)
- **Qt 6.8+** (MSVC 2022, 64-Bit)
- **CMake 3.20+**
- **Visual Studio 2022** (MSVC v143 Toolset)

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
1. Beendet laufende BricsCAD-Instanzen – solange BricsCAD läuft, hält es `batchtool.brx` geöffnet und der Build scheitert am Linker. Zuerst wird BricsCAD regulär zum Beenden aufgefordert (Speichern-Rückfragen erscheinen wie gewohnt), erst nach 30 Sekunden folgt eine Rückfrage zum harten Beenden. `CLEAN_BUILD.bat /force` überspringt diese Rückfrage
2. Löscht alte Build-Artefakte
3. CMake-Konfiguration (Visual Studio 17 2022, x64, Release)
4. MSBuild-Kompilierung

Das fertige Plugin liegt anschließend unter `build_windows\Release\batchtool.brx`.

### Manuelle Build-Schritte

```cmd
mkdir build_windows
cd build_windows
cmake -G "Visual Studio 17 2022" -A x64 -DCMAKE_BUILD_TYPE=Release ..
cmake --build . --config Release
```

### Qt- und BricsCAD-Pfade anpassen

Die Pfade in der Root-`CMakeLists.txt` müssen ggf. an die lokale Installation angepasst werden:

```cmake
set(QT6_DIR "C:/Qt/6.8.3/msvc2022_64")
set(BRICSCAD_DIR "C:/Program Files/Bricsys/BricsCAD V26 de_DE")
```

## Installation in BricsCAD

### Einmalig (zum Testen)

1. BricsCAD starten
2. Befehl: `APPLOAD`
3. Zur Datei `batchtool.brx` navigieren und laden
4. In der Kommandozeile erscheint: *"Batch Processing Plugin geladen. Befehl: BATCHTOOL"*

### Automatisch bei jedem Start

1. `APPLOAD` aufrufen
2. Unten auf *"Inhalt..."* (Startup Suite) klicken
3. `batchtool.brx` zur Startup Suite hinzufügen

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

### Tab: OpenCirt

GA-Planungsautomatisierung (Gebäudeautomation) für TGA-Projekte. Funktionen:

Der Tab hat fünf Schaltflächen. Die eigentliche Projekterstellung läuft über einen einzigen Knopf – Plankopf, Deckblätter, BMK, BAS, GA-FL, Summen und Textbreiten sind Schritte darin und werden nicht mehr einzeln bedient.

- **Projekt erstellen** – der Gesamtlauf in korrekter Reihenfolge:
  - *Plankopf* – CSV-basierte Plankopf-Attribute setzen (AG, AN, PR etc.)
  - *Deckblätter* – für die Los/ASP/Gewerk/Anlage-Hierarchie, inkl. ASP, Gewerk und Anlage aus der Ordnerstruktur
  - *BMK-Nummerierung* – Betriebsmittelkennzeichen automatisch vergeben
  - *BAS-Generierung* – Bauautomationssystem aus BAS.csv erzeugen
  - *GA-FL* – Funktionslisten zweiphasig: Datenextraktion aus Quell-DWGs, dann GA-FL-Blätter erzeugen und füllen
  - *Summenblätter* – Gewerk-Summe, ASP-Summe, Los-Summe, Projekt-Summe sowie eine Gewerke-Auswertung je Los über alle ASPs
  - *Textbreiten* – Breitenfaktor in GA-FL- und Summenblättern korrigieren, auch für Werte innerhalb der GA-FL-Blockdefinition
- **Projekt bereinigen** – temporäre Dateien und Backups im Zeichnungsordner löschen (`*.bak`, `*.dwl`, `*.dwl2`, `*.sv$`, `*.ac$`, `*.tmp`, `*.log`)
- **PDF publizieren** – DSD-basierter Multi-Sheet-PDF-Export inkl. Inhaltsverzeichnis (21 Einträge pro Seite, Plankopf aus `plankopfdaten.csv`)
- **IO-Liste erstellen** – ODS-Vorlagen-basierter Export (`OdsTemplateWriter`, Referenz: `iomodule.csv`). Im Dialog wird nach Integrationsart gefiltert (Attribut `OC_INTEGRATIONSART_DP_n`): leer = alle Datenpunkte, `HW` = SPS-/DDC-Belegungsliste mit Modul- und Kanalzuordnung, `BUS;SMI` = mehrere Arten. Die Integrationsart steht als eigene Spalte in der Liste; *Modul-Typ* wird nur für HW-Zeilen gefüllt
- **Sensorliste erstellen** – Keyword-Matching gegen Blockattribute (`SensorKeywordLoader`, Referenz: `sensor.csv`)

## Projektstruktur

```
├── CMakeLists.txt              Root-Build-Konfiguration
├── CLEAN_BUILD.bat             Build-Skript
├── LICENSE                     BSL 1.1 Lizenz
├── .gitignore
├── docs/
│   └── DEVELOPMENT.md          Entwickler-Hinweise
├── tools/
│   └── Close-BricsCAD.ps1      Beendet BricsCAD vor dem Build
├── external/
│   └── brx_sdk/                BRX SDK (nicht im Repository)
├── sample_project/
│   ├── 00- BricsCAD Plugin/    Kompiliertes BRX-Binary
│   ├── 01- Referenzen/
│   │   ├── BAS.csv             BAS-Konfiguration
│   │   ├── GA_FL_VORLAGE.ods   GA-FL Vorlage
│   │   ├── iomodule.csv        IO-Modul-Referenzdaten
│   │   ├── opencirt_config.json Projektkonfiguration
│   │   ├── plankopfdaten.csv   Plankopf-Attribute
│   │   └── sensor.csv          Sensor-Referenzdaten
│   ├── 02- Skripte/            LISP-Skripte
│   ├── 03- Blockbibliothek/    DWG-Blockvorlagen
│   ├── 04- Vorlagen/
│   │   ├── OC_VORLAGE_DIN_A2_V12.dwg
│   │   ├── OC_VORLAGE_EINTRAG_INHALT_DIN_A2_V_4.dwg
│   │   ├── OC_VORLAGE_GA_FL.dwg
│   │   ├── OC_VORLAGE_IO_BELEGUNG_V_1.ods
│   │   └── OC_VORLAGE_SENSORLISTE_V_1.ods
│   ├── 05- Projekt Zeichnungen/
│   ├── 06- Plot/
│   └── BEDIENUNGSANLEITUNG.md  Bedienungsanleitung (12 Kapitel)
└── src/
    ├── windows_fix.h           Qt 6.8+ / Windows SDK Kompatibilität
    ├── brx_force_include.h     BRX Platform-Header
    ├── core/
    │   ├── DwgProcessor.cpp/h      DWG-Verarbeitungslogik (Text, Attribute, Layer)
    │   └── LispProcessExecutor.cpp/h   In-Process LISP-Ausführung via _.SCRIPT
    ├── data/
    │   ├── ProcessingOptions.h         Datenstrukturen und Optionen
    │   ├── ProcessingOptionsImpl.cpp   LISP Script Manager
    │   └── Configuration.cpp           Settings-Persistenz
    ├── mfc_stubs/              Leere MFC/ATL-Stubs (Qt-basiert, kein MFC)
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
        └── OdsTemplateWriter.cpp/h     ODS-Vorlagen befüllen
```

## Hinweise

- Vor dem ersten produktiven Einsatz immer mit aktivierten Backups arbeiten.
- LISP-Skripte vorher manuell an einer einzelnen DWG testen.
- Layer-Analyse vor Layer-Operationen durchführen, um Tippfehler zu vermeiden.
- Mehrere Tabs können gleichzeitig aktiviert sein. Die Verarbeitung erfolgt in der Reihenfolge: Text → Attribute → Layer → LISP.

## Lizenz

Business Source License 1.1 (BSL 1.1) – siehe [LICENSE](LICENSE) und [ADDITIONAL_TERMS](ADDITIONAL_TERMS).

Kurzfassung: Nutzung für interne Zwecke, kommerzielle Projekte und Dienstleistungen ist erlaubt. Verkauf als eigenständiges Produkt, SaaS-Angebote und proprietäre Forks sind untersagt. Ab dem Change Date (2030-03-02) wird die Software unter AGPLv3 verfügbar. Nutzung auf eigenes Risiko – vor jedem Batch-Lauf Backups erstellen!

## Technologie

Entwickelt mit BRX SDK V26, Qt 6, C++17, CMake.
