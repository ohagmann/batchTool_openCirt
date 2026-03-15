# OpenCirt – Bedienungsanleitung

**Version:** 1.0  
**Stand:** März 2026  
**Für:** BricsCAD V26 mit Batchtool/OpenCirt Plugin

---

## Inhaltsverzeichnis

1. [Überblick](#1-überblick)
2. [Installation](#2-installation)
3. [Projektstruktur anlegen](#3-projektstruktur-anlegen)
4. [Symbole einfügen und Zeichnungen erstellen](#4-symbole-einfügen-und-zeichnungen-erstellen)
5. [Plankopf-Daten setzen](#5-plankopf-daten-setzen)
6. [BMK-Nummerierung](#6-bmk-nummerierung)
7. [BAS-Generierung](#7-bas-generierung)
8. [GA-Funktionslisten erstellen](#8-ga-funktionslisten-erstellen)
9. [Deckblätter generieren](#9-deckblätter-generieren)
10. [Inhaltsverzeichnis und PDF-Publish](#10-inhaltsverzeichnis-und-pdf-publish)
11. [Gesamtprojekt erstellen](#11-gesamtprojekt-erstellen)
12. [Tipps und Fehlerbehebung](#12-tipps-und-fehlerbehebung)

---

## 1. Überblick

OpenCirt automatisiert die Erstellung von GA-Planungsunterlagen in BricsCAD. Aus Schema-Zeichnungen mit standardisierten Symbolen werden automatisch erzeugt:

- **BMK-Nummern** (Betriebsmittelkennzeichen) für alle Datenpunkte
- **BAS-Bezeichner** (Benutzeradressierungssystem) nach frei konfigurierbarer Vorlage
- **GA-Funktionslisten** mit allen Datenpunkten, GA-Funktionen und BAS-Adressen
- **Summenblätter** (Projekt-, Los-, ASP, Gewerke- und Anlagensummen)
- **Deckblätter** für jede Hierarchie-Ebene (Projekt, Los, ASP, Gewerk, Anlage)
- **Inhaltsverzeichnis** mit automatischer Seitennummerierung
- **PDF-Gesamtdokument** als Multi-Sheet-PDF

Der gesamte Ablauf von der Schema-Zeichnung bis zum fertigen PDF-Planungspaket ist automatisiert.

---

## 2. Installation

### 2.1 Plugin laden

1. Die Datei `batchtool.brx` aus dem Ordner `00- BricsCAD Plugin/00- Windows Version/` in einen festen Speicherort kopieren (z.B. `C:\BricsCAD Plugins\`).
2. BricsCAD V26 starten.
3. Befehl `APPLOAD` eingeben.
4. Zur Datei `batchtool.brx` navigieren und „Laden" klicken.
5. In der Kommandozeile erscheint: *„Batch Processing Plugin geladen. Befehl: BATCHTOOL"*

### 2.2 Automatisch bei jedem Start laden

1. `APPLOAD` aufrufen.
2. Unten auf „Inhalt..." (Startup Suite) klicken.
3. `batchtool.brx` zur Startup Suite hinzufügen.

### 2.3 Plugin öffnen

Befehl in der Kommandozeile: `BATCHTOOL`

Das Hauptfenster öffnet sich mit den Tabs: General, Text, Attributes, Layers, LISP, **OpenCirt**.

---

## 3. Projektstruktur anlegen

Jedes OpenCirt-Projekt folgt einer festen Ordnerstruktur. Dieses Sample-Projekt dient als Vorlage – kopieren Sie den gesamten `04- OC SAMPLE`-Ordner und benennen Sie ihn um.

### 3.1 Ordnerstruktur

```
Projektname/
├── 00- BricsCAD Plugin/        Plugin-Binary (nur Referenz)
├── 01- Referenzen/             Konfigurationsdateien
│   ├── BAS.csv                 BAS-Aufbau (Segmente)
│   ├── GA_FL_VORLAGE.ods       Datenpunkt-Referenztabelle
│   ├── plankopfdaten.csv       Plankopf-Stammdaten
│   └── opencirt_config.json    Plugin-Konfiguration (automatisch)
├── 02- Skripte/                LISP-Skripte (nicht ändern!)
│   ├── BmkNummerierung.lsp
│   ├── ExtractDP.lsp
│   ├── FillGaFl.lsp
│   ├── GenBas.lsp
│   └── TextBreitenAnpassenBloecke.lsp
├── 03- Blockbibliothek/        Symbole für Schema-Zeichnungen
│   ├── 00- Vorlagensymbol/     Basis-Vorlage zum Erstellen eigener Symbole
│   ├── 01- Pfeile/             Datenpunkt-Pfeile (AI, AO, BI, BO)
│   └── ...                     Weitere Symbole (Ventilator, Meldung etc.)
├── 04- Vorlagen/               DWG-Vorlagen
│   ├── OC_VORLAGE_DIN_A2_V12.dwg           Rahmen/Plankopf DIN A2
│   ├── OC_VORLAGE_GA_FL.dwg                GA-FL Blattvorlage
│   └── OC_VORLAGE_EINTRAG_INHALT_DIN_A2_V_4.dwg  Inhaltsverzeichnis-Block
├── 05- Projekt Zeichnungen/    Hier entstehen die Zeichnungen
│   └── 01 Los 1/
│       └── 01 ASP01/
│           ├── 01 GA/          Gewerk: Gebäudeautomation
│           ├── 02 RLT/         Gewerk: Raumlufttechnik
│           ├── 03 HZG/         Gewerk: Heizung
│           └── ...
└── 06- Plot/                   Ausgabe (PDF)
```

### 3.2 Ordnerhierarchie der Zeichnungen

Die Ordnerstruktur unter `05- Projekt Zeichnungen/` folgt der GA-Hierarchie:

```
Los → ASP → Gewerk → Anlage
```

**Beispiel:**
```
05- Projekt Zeichnungen/
└── 01 Los 1/
    └── 01 ASP01/
        ├── 01 GA/
        │   └── 01 SSK-1000/     ← Anlage
        │       └── 00 SSK interne Datenpunkte.dwg
        ├── 02 RLT/
        │   └── 01 TKA-1000/     ← Anlage
        │       └── 00 Hauptanlage.dwg
        └── 03 HZG/
            └── 01 WPU-1000/     ← Anlage
                └── 00 Wärmepumpe.dwg
```

**Wichtig:** Die Ordnernamen werden automatisch ausgewertet:
Die Nummerierung am Anfang (01, 02, ...) bestimmt die Sortierung, in allen Ebenen.

---

## 4. Symbole einfügen und Zeichnungen erstellen

### 4.1 Neue Zeichnung anlegen

1. Öffnen Sie die Vorlage `OC_VORLAGE_DIN_A2_V12.dwg` aus dem Ordner `04- Vorlagen/`.
2. Speichern Sie die Datei im passenden Ordner unter `05- Projekt Zeichnungen/`, z.B.:
   ```
   05- Projekt Zeichnungen/01 Los 1/01 ASP01/02 RLT/01 TKA-1000/00 Hauptanlage.dwg
   ```

### 4.2 Symbole aus der Blockbibliothek einfügen

Die Blockbibliothek (`03- Blockbibliothek/`) enthält vorgefertigte Symbole mit allen OpenCirt-Attributen.

**Symbol einfügen:**

1. In BricsCAD den Befehl `INSERT` (oder `EINFÜGE`) oder über die GUI "Block einfügen"verwenden.
2. Auf „Durchsuchen" klicken und zum Ordner `03- Blockbibliothek/` navigieren.
3. Das gewünschte Symbol auswählen, z.B. `OC_VORLAGE_VENTILATOR_OBEN_UNTEN_RLT_V3.dwg`.
4. Einfügepunkt in der Zeichnung bestimmen.

**Verfügbare Symboltypen:**

| Symbol | Beschreibung |
|---|---|
| `Symbolvorlage_20_DP_V_1_0.dwg` | Basis-Vorlage mit 20 Datenpunkten (zum Erstellen eigener Symbole) |
| `OC_VORLAGE_DP_PFEIL_AI_V1.dwg` | Datenpunkt-Pfeil: Analogeingang (AI) |
| `OC_VORLAGE_DP_PFEIL_AO_V1.dwg` | Datenpunkt-Pfeil: Analogausgang (AO) |
| `OC_VORLAGE_DP_PFEIL_BI_V1.dwg` | Datenpunkt-Pfeil: Binäreingang (BI) |
| `OC_VORLAGE_DP_PFEIL_BO_V1.dwg` | Datenpunkt-Pfeil: Binärausgang (BO) |
| `OC_VORLAGE_PFEIL_V1.dwg` | Allgemeiner Pfeil |
| `OC_VORLAGE_VENTILATOR_OBEN_UNTEN_RLT_V3.dwg` | Ventilator-Symbol (RLT) |
| `OC_VORLAGE_1_HW_MELDUNG_GA_V1.dwg` | Hardware-Meldung |

### 4.3 Eigene Symbole erstellen

1. Kopieren Sie `Symbolvorlage_20_DP_V_1_0.dwg` aus `03- Blockbibliothek/00- Vorlagensymbol/`.
2. Öffnen Sie die Kopie in BricsCAD.
3. Zeichnen Sie Ihre Grafik.
4. Die vorhandenen OC-Attribute bleiben erhalten – sie werden automatisch von den OpenCirt-Skripten befüllt.
5. Nicht benötigte Datenpunkte deaktivieren: Attribut `OC_FL_AKTIV_n` auf leer setzen (nur Datenpunkte mit Aktiv-Kennzeichen werden verarbeitet).

**Tipp:** Falls nach dem Bearbeiten eines Blocks die Attribut-Reihenfolge im Eigenschaftenfenster durcheinander ist, können Sie die ATTDEFs im Block-Editor (BEDIT) manuell löschen und in der gewünschten Reihenfolge neu anlegen.

### 4.4 Wichtige OC-Attribute in den Symbolen

Jedes OpenCirt-Symbol enthält folgende Attribute pro Datenpunkt (n = 1..20):

| Attribut | Beschreibung | Beispiel |
|---|---|---|
| `OC_BEZEICHNUNG` | Bezeichnung des Geräts/Symbols | „ZUL-Ventilator" |
| `OC_AKS` | Anlagenkennzeichen (wird von BMK befüllt) | „ZUV-" |
| `OC_FL_AKTIV_n` | Datenpunkt n aktiv? | „ja" / „" (leer = inaktiv) |
| `OC_REF_DP_n` | Referenzname für GA-FL-Vorlage (ODS-Lookup) | „MW_HW" |
| `OC_FCODE_DP_n` | Funktionscode | „MW_01" |
| `OC_INTEG_DP_n` | Integrationsart | „BACnet" |
| `OC_KOMMENTAR_DP_n` | Kommentar | „Zulufttemperatur" |
| `OC_BAS_DP_n` | BAS-Adresse (wird automatisch generiert) | „BSP-ASP01-RLT-TKA-1000-TZU-01-MW_01" |

### 4.5 Datenpunkt-Pfeile

Die Datenpunkt-Pfeile (`01- Pfeile/`) werden an die Symbole angehängt und zeigen die Signalrichtung:

- **AI** (Analog Input): Messwerte lesen (Temperatur, Druck, ...)
- **AO** (Analog Output): Stellsignale ausgeben (Ventilstellung, Drehzahl, ...)
- **BI** (Binary Input): Meldungen lesen (Ein/Aus, Störung, ...)
- **BO** (Binary Output): Schaltbefehle ausgeben (Ein/Aus, ...)

---

## 5. Plankopf-Daten setzen

Die Funktion „Plankopf-Daten setzen" befüllt die Plankopf-Blöcke aller Zeichnungen mit den Stammdaten aus `plankopfdaten.csv`.

### 5.1 plankopfdaten.csv anpassen

Öffnen Sie `01- Referenzen/plankopfdaten.csv` in einem Texteditor und passen Sie die Werte an:

```csv
Attributname;Wert;Erläuterung
AN1;Ihre Firma GmbH;Auftragnehmer Zeile 1
AN2;Musterstraße 1;Auftragnehmer Zeile 2
...
AG1;Bauherr GmbH;Auftraggeber Zeile 1
...
PR1;Projektname;Projekt Zeile 1
PR2;Projektadresse;Projekt Zeile 2
...
ERSTELLER;Max Mustermann;Ersteller
ERSTELLDATUM;01.04.2026;Erstelldatum
```

**Wichtig:** `PR1` wird auf dem Projekt-Deckblatt als Projektname angezeigt.

### 5.2 Ausführung

1. Im BATCHTOOL den Tab **OpenCirt** öffnen.
2. Im Tab **General** den Projektordner auswählen (der Ordner, der `05- Projekt Zeichnungen/` enthält).
3. Auf **„Plankopf-Daten setzen"** klicken.
4. Alle Zeichnungen werden geöffnet, die CSV-Daten in die Plankopf-Attribute geschrieben, gespeichert und geschlossen.

Zusätzlich werden automatisch aus dem Ordnerpfad die Attribute **ASP**, **GEWERK** und **ANLAGE** im Plankopf gesetzt.

---

## 6. BMK-Nummerierung

Die BMK-Nummerierung vergibt automatisch fortlaufende Betriebsmittelkennzeichen für alle Symbole mit dem Attribut `OC_AKS`.

### 6.1 Funktionsweise

- Jeder Block mit Attribut `OC_AKS` wird erkannt.
- Das Präfix (z.B. „BSK-") wird beibehalten, die Nummer wird automatisch angehängt: `BSK-01`, `BSK-02`, ...
- Die Sortierung erfolgt spaltenweise: links → rechts, innerhalb einer Spalte.
- Zähler werden zwischen Zeichnungen weitergegeben (Datei `bmk_counters.tmp`).
### 6.2 Steuerung pro Zeichnung

Das Plankopf-Attribut **FREITEXT_05** steuert den Modus:

| Wert | Verhalten |
|---|---|
| `NEUSTARTEN` (oder leer) | Zähler beginnen bei 01 |
| `FORTSETZEN` | Zähler aus vorheriger Zeichnung übernehmen |

### 6.3 Sperren einzelner Blöcke

Wenn ein Block ein Attribut `OC_AKS_LOCK` hat und dieses auf „JA", „TRUE" oder „X" gesetzt ist, wird der Block von der Nummerierung übersprungen.

### 6.4 Ausführung

1. Im OpenCirt-Tab auf **„BMK erstellen"** klicken.
2. Alle Zeichnungen werden sequenziell verarbeitet.

---

## 7. BAS-Generierung

Die BAS-Generierung baut für jeden aktiven Datenpunkt eine Benutzeradresse (BAS-String) zusammen und schreibt sie in das Attribut `OC_BAS_DP_n`.

### 7.1 BAS.csv konfigurieren

Die Datei `01- Referenzen/BAS.csv` definiert den Aufbau des BAS-Strings. Jede Zeile ist ein Segment:

```csv
"Testprojekt"     ← Statischer Text (in Anführungszeichen)
-                  ← Trennzeichen (Bindestrich)
ASP                ← Attributwert aus dem Plankopf
-
GEWERK
-
ANLAGE
-
ORTSKENNZEICHEN
-
OC_AKS             ← Attributwert aus dem Block
-
OC_FCODE_DP        ← Endet mit _DP → wird pro Datenpunkt zu OC_FCODE_DP_1, _2, ...
```

**Ergebnis-Beispiel:** `BSP-ASP01-RLT-TKA-1000-ZUV-01-FR_01`

### 7.2 Segment-Typen

| Typ | Format | Beschreibung |
|---|---|---|
| Statischer Text | `"Text"` | Wird 1:1 übernommen |
| Trennzeichen | `-` | Bindestrich als Separator |
| Plankopf-Attribut | `ASP`, `GEWERK`, etc. | Wert wird aus dem Plankopf gelesen |
| Block-Attribut | `OC_AKS`, etc. | Wert wird aus dem Symbol-Block gelesen |
| Datenpunkt-Attribut | `OC_FCODE_DP` | Endet mit `_DP` → wird zu `_DP_1`, `_DP_2`, ... pro Datenpunkt |

### 7.3 Ausführung

1. Im OpenCirt-Tab auf **„BAS generieren"** klicken.
2. Die BAS.csv wird eingelesen, alle aktiven Datenpunkte werden verarbeitet.
3. Ergebnis wird in `OC_BAS_DP_n` geschrieben.

---

## 8. GA-Funktionslisten erstellen

Die GA-FL-Erstellung ist der Kern von OpenCirt. Sie läuft in zwei Phasen:

### 8.1 Phase 1: Datenextraktion

- Alle Quell-DWGs werden geöffnet.
- Das LISP-Skript `ExtractDP.lsp` liest die Datenpunkte aus (OC_BMK, OC_AKS, OC_REF_DP, OC_BAS_DP, Funktionswerte, ...).
- Die Daten werden als CSV-Dateien im temp-Verzeichnis gespeichert.

### 8.2 Phase 2: GA-FL-Blätter erzeugen

- Für jede Quell-DWG wird eine GA-FL-DWG erzeugt (Kopie von `OC_VORLAGE_GA_FL.dwg`).
- Das LISP-Skript `FillGaFl.lsp` füllt die Blätter mit den extrahierten Daten.
- Zusätzlich werden Summenblätter generiert (ASP-Summe, Los-Summe, Projekt-Summe).

### 8.3 GA_FL_VORLAGE.ods

Die Datei `01- Referenzen/GA_FL_VORLAGE.ods` ist die Referenztabelle. Sie definiert, welche Spalten und Werte für jeden Datenpunkttyp (Referenz-DP) in der GA-FL erscheinen. Das Attribut `OC_REF_DP_n` im Symbol verweist auf eine Zeile in dieser Tabelle.

### 8.4 Ausführung

1. Im OpenCirt-Tab auf **„GA-FL erstellen"** klicken.
2. **Erster Klick:** Phase 1 (Extraktion) läuft.
3. Warten bis Phase 1 abgeschlossen ist (Meldung im Log).
4. **Zweiter Klick:** Phase 2 (Erzeugung und Befüllung) läuft automatisch.

Die erzeugten GA-FL-Dateien werden im jeweiligen Anlage-Ordner gespeichert:
```
01 TKA-1000/
├── 00 Hauptanlage.dwg            ← Quell-Zeichnung
└── 00 Hauptanlage_GA_FL_1.dwg    ← Erzeugte GA-FL
```

### 8.5 Textbreitenanpassung

Nach der GA-FL-Erstellung kann optional die Textbreitenanpassung ausgeführt werden. Sie korrigiert zu breite Texte in den GA-FL-Blöcken, damit alle Einträge sauber in die Spalten passen.

---

## 9. Deckblätter generieren

Die Funktion „Deckblätter erstellen" erzeugt automatisch Deckblätter für jede Hierarchie-Ebene.

### 9.1 Erzeugte Deckblätter

| Ebene | Dateiname | Inhalt |
|---|---|---|
| Projekt | `0000 Projekt_Deckblatt_B.dwg` | Projektname (aus PR1) |
| Los | `0000 Los 1_Deckblatt.dwg` | Los-Bezeichnung |
| ASP | `0000 ASP01_Deckblatt.dwg` | ASP-Kennung |
| Gewerk | `0000 RLT_Deckblatt.dwg` | Gewerk-Bezeichnung |
| Anlage | `0000 TKA-1000_Deckblatt.dwg` | Anlagen-Kennung |

Die Deckblätter werden aus der Vorlage `OC_VORLAGE_DIN_A2_V12.dwg` erzeugt. Der Layer „GA-Deckblatt" wird aufgetaut, Trennlinien-Layer werden eingefroren.

### 9.2 Ausführung

Im OpenCirt-Tab auf **„Deckblätter erstellen"** klicken.

---

## 10. Inhaltsverzeichnis und PDF-Publish

### 10.1 Inhaltsverzeichnis

Das Inhaltsverzeichnis wird automatisch aus allen Zeichnungen im Projekt generiert. Jeder Eintrag enthält:

- Los, ASP, Gewerk, Anlage
- Zeichnungsnummer
- Seitenzahl

Die Einträge werden als Blöcke (`OC_VORLAGE_EINTRAG_INHALT_DIN_A2_V_4.dwg`) eingefügt – 21 Einträge pro Seite, bei Bedarf werden automatisch weitere Seiten erzeugt.

### 10.2 PDF-Publish

Nach dem Inhaltsverzeichnis wird automatisch ein Multi-Sheet-PDF im Ordner `06- Plot/` erzeugt. Das PDF enthält alle Zeichnungen in der korrekten Reihenfolge:

1. Projekt-Deckblatt
2. Inhaltsverzeichnis
3. ASP-Deckblätter → Gewerk-Deckblätter → Anlage-Deckblätter → Schema-Zeichnungen → GA-FLs → Summenblätter

### 10.3 Ausführung

Im OpenCirt-Tab auf **„PDF Publish"** klicken. Inhaltsverzeichnis und PDF werden nacheinander erzeugt.

---

## 11. Gesamtprojekt erstellen

Die Funktion „Gesamtprojekt" führt alle Schritte in der korrekten Reihenfolge automatisch aus:

1. Projektstruktur bereinigen (alte GA-FLs, Deckblätter, Inhaltsverzeichnisse löschen)
2. Plankopf-Daten setzen (aus CSV)
3. BMK-Nummerierung (optional, per Checkbox)
4. BAS-Generierung (optional, per Checkbox)
5. GA-FL Phase 1: Datenextraktion
6. GA-FL Phase 2: Erzeugung und Befüllung
7. Summenblätter generieren
8. Textbreitenanpassung
9. Deckblätter erstellen
10. Inhaltsverzeichnis erstellen

Der PDF-Publish muss dann noch separat angestoßen werden.

### Checkboxen im OpenCirt-Tab

| Option | Beschreibung |
|---|---|
| BMK-Nummerierung einschliessen | BMK-Vergabe vor GA-FL-Generierung |
| BAS-Generierung einschliessen | BAS-Adressen vor GA-FL-Generierung erzeugen |

### Ausführung

Im OpenCirt-Tab auf **„Gesamtprojekt"** klicken und die Warnung bestätigen. Der Prozess läuft vollautomatisch – BricsCAD während der Verarbeitung nicht manuell bedienen!

---

## 12. Tipps und Fehlerbehebung

### Allgemeine Hinweise

- **Backups:** Vor jedem Batch-Lauf (ACHTUNG NICHT openCirt Erzeugung!!) werden automatisch Backups erstellt (konfigurierbar im General-Tab). Trotzdem empfiehlt sich eine zusätzliche manuelle Sicherung.
- **Nicht bedienen:** Während eines Batch-Laufs BricsCAD nicht manuell bedienen – die Verarbeitung läuft über SCR-Skripte in der aktuellen Instanz.
- **Aktuelle Zeichnung:** Die aktuell geöffnete Zeichnung darf nicht in der Batch-Liste enthalten sein.

### Häufige Fehler

| Problem | Ursache | Lösung |
|---|---|---|
| „Projektstruktur ungültig" | Ordner fehlen | Alle 4 Ordner müssen existieren: 01- Referenzen, 02- Skripte, 04- Vorlagen, 05- Projekt Zeichnungen |
| „LISP-Skript nicht gefunden" | Skripte fehlen in 02- Skripte/ | Alle 5 .lsp-Dateien aus dem Sample-Projekt kopieren |
| „BAS.csv nicht gefunden" | BAS.csv fehlt oder falsch benannt | Datei muss exakt `BAS.csv` heißen und in `01- Referenzen/` liegen |
| „GA_FL_VORLAGE.ods nicht gefunden" | ODS-Datei fehlt | `GA_FL_VORLAGE.ods` in `01- Referenzen/` ablegen |
| Keine Datenpunkte erkannt | OC_FL_AKTIV_n nicht gesetzt | Mindestens ein Datenpunkt muss aktiv sein (OC_FL_AKTIV_n = „ja") |
| BMK-Nummern beginnen nicht bei 01 | FREITEXT_05 = FORTSETZEN | Auf „NEUSTARTEN" setzen oder bmk_counters.tmp löschen |
| Deckblatt zeigt „Projekt" statt Projektname | PR1 leer | In plankopfdaten.csv den Wert für PR1 eintragen |

### Attribut-Reihenfolge korrigieren

Wenn die Reihenfolge der Attribute im Eigenschaftenfenster durcheinander ist (z.B. nach manuellem Bearbeiten eines Blocks):

1. Block im Block-Editor öffnen (BEDIT).
2. `ATTDISP` auf `EIN` setzen, damit alle Attribute sichtbar sind.
3. ATTDEFs in der gewünschten Reihenfolge neu anlegen (die Erstellungsreihenfolge bestimmt die Anzeigereihenfolge).
4. Block-Editor schließen und speichern.

---

## Lizenz

OpenCirt steht unter der Business Source License 1.1 (BSL 1.1). Nutzung für interne Zwecke und kommerzielle Projekte ist erlaubt. Verkauf als eigenständiges Produkt und SaaS-Angebote sind untersagt. Ab 2030-03-02 wird die Software unter AGPLv3 verfügbar.

Siehe [LICENSE](../LICENSE) und [ADDITIONAL_TERMS](../ADDITIONAL_TERMS) im Repository.

---

*OpenCirt – Open Source GA-Planungsautomatisierung für BricsCAD*  
*© 2026 Oliver Hagmann*
