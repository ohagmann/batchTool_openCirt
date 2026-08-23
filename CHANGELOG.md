# Changelog

Alle wesentlichen Änderungen am Batchtool / OpenCirt Plugin werden in dieser Datei dokumentiert.

Format basiert auf [Keep a Changelog](https://keepachangelog.com/de/1.1.0/).
Versionierung: Bump bei Änderungen am Plugin-Binary (C++/GUI). Kein Bump bei reinen Änderungen an Vorlagen, LISP-Skripten, Dokumentation oder Repo-Konfiguration.

## [Unreleased]

## [1.3.1] – 2026-08-23

### Changed
- Die Erfolgsmeldung des Datenpunkt-Exports nennt jetzt die tatsächliche Zeilenzahl der Datei und schlüsselt sie auf: `972 Zeilen (869 Datenpunktzeilen + 103 Reservekanäle)`. Bisher wurde nur der Datenpunktanteil gemeldet – die Reservezeilen entstehen erst beim Auffüllen der angefangenen Module und fehlten in der Zählung, die Meldung wich damit von der Datei ab. Gezählt wird nun der Schreibzähler selbst statt einer nebenher geführten Summe, die auseinanderlaufen konnte
- Der Lesefortschritt des Exports läuft in die Statuszeile statt ins Protokoll (neues Signal `OpenCirtTab::statusMessage`, leerer Text setzt zurück). Das Protokoll führt damit nur noch Ergebnisse; die Taktung ist von 50 auf 20 Blätter verkürzt, weil eine Statuszeile das verträgt

### Removed
- Zwei veraltete `batchtool.brx`-Kopien außerhalb des Repos entfernt (Stand 15.03. und 25.03.), damit beim Laden des Moduls keine Verwechslung mehr möglich ist


## [1.3.0] – 2026-08-23

### Changed
- **Oberfläche folgt BricsCADs Hell-/Dunkeleinstellung.** Maßgeblich ist die Systemvariable `COLORTHEME`, gelesen bei jedem Aufruf von `BATCHTOOL` – nach einem Themenwechsel genügt Schließen und erneutes Öffnen. Der Stil ist jetzt Fusion: der auf Windows 11 voreingestellte Qt-Stil zeichnet Flächen und abgerundete Ecken selbst und ignoriert die Palette, weshalb dort weder ein dunkles Thema noch BricsCADs eckige Optik möglich wäre
- Keine fest verdrahteten Farbwerte mehr in der Oberfläche. Beschriftungen tragen eine Rolle (`Muted`, `Success`, `Warning`, `ErrorBold` …), die beim Themenwechsel neu berechnet wird; bisher waren die Farben für ein helles Thema geschrieben und auf dunklem Grund kaum lesbar
- Der OpenCirt-Tab führt kein eigenes Protokoll mehr. Meldungen liefen bisher doppelt – einmal im Tab, einmal im *Processing Log* –, was rund die halbe Fensterhöhe für denselben Text verbrauchte und unter dem Protokollkasten einen leeren Streifen hinterließ. Das *Processing Log* ist jetzt die einzige Ansicht, ohne Höhenbegrenzung und mit fester Beteiligung an der Fensterhöhe
- Das *Processing Log* färbt Meldungen nach Art ein (Fehler, Warnung, Erfolg). Das konnte bisher nur das entfallene Tab-Protokoll
- Trenner zwischen den Schaltflächengruppen ist ein echtes `QFrame` statt eines leeren `QLabel` mit Rahmen, das auf dunklem Grund unsichtbar blieb

### Added
- `src/ui/Theming.cpp/h` – Palette, Stil und rollenbasierte Farben, abgeleitet aus `COLORTHEME`


## [1.2.0] – 2026-08-22

### Added
- Gewerke-Summenblatt je Los: `0002 Projekt_Summe_Gewerke_<Los>_NN.dwg` im Wurzelordner der Projektzeichnungen, eine Zeile je Gewerk aggregiert über alle ASPs des Loses. Liegt in der Publish-Reihenfolge direkt hinter der Projektsumme
- Datenpunkt-Export mit Filterdialog: Filter nach Integrationsart (semikolongetrennt, Vorbelegung `HW`, leer = alle Datenpunkte). Die Spalte *Modul-Typ* wird nur für `HW`-Zeilen befüllt, alle anderen Integrationsarten belegen keinen Klemmenkanal
- Spalte *Integrationsart* in der IO-/Datenpunktliste. Reservezeilen bleiben dort leer, weil ihnen kein Datenpunkt zugrunde liegt. Setzt die um eine Spalte erweiterte `OC_VORLAGE_IO_BELEGUNG_V_1.ods` voraus (Integrationsart als vorletzte Spalte, vor Modul-Typ)
- Plankopf-Attribut `LPH` (Leistungsphase) wird mit extrahiert und damit in GA-FL- und Summenblätter übernommen (`ExtractDP.lsp`). Plankopfblöcke ohne dieses Attribut bleiben unberührt
- Warnmeldung beim Datenpunkt-Export, wenn `GA_FL_VORLAGE.ods` bei einem Datenpunkt einen IO-Referenzwert > 1 führt (ein Datenpunkt trägt genau ein AKS/BAS und belegt damit genau einen Kanal)
- Protokollausgabe der im Projekt vorhandenen Integrationsarten inklusive Anzahl – zeigt sofort, ob `OC_INTEGRATIONSART_DP_n` in den Zeichnungen gepflegt ist
- Diagnosebefehl `TEXTZELLEN` in `TextBreitenAnpassenBloecke.lsp`: listet für eine gewählte Blockreferenz alle erkannten Zellen mit Breite und Höhe

### Fixed
- **Textbreitenanpassung greift jetzt in Blockdefinitionen hinein.** Die GA-FL-Tabelle liegt als ein Block im Modellbereich; `(ssget "_X" ...)` fand die rund 1.740 Zellrechtecke innerhalb der Blockdefinition nicht, weshalb mehrstellige Zahlen in den 5 mm breiten Zählspalten nie gestaucht wurden. Die Zuordnung Text → Zelle wird nun im Blockdefinitionsraum über den Attributnamen gebildet und mit dem Blockmaßstab auf die Blockreferenz angewandt (`TextBreitenAnpassenBloecke.lsp` v3.0)
- **Integrationsart wurde nie extrahiert.** `ExtractDP.lsp` las `OC_INTEG_DP_n`; in den Datenpunktblöcken heißt das Attribut `OC_INTEGRATIONSART_DP_n` (Erstellliste Spalte 32). Die Spalte `INTEG_DP` kam dadurch in jeder Extraktion leer an. Der alte Name bleibt als Fallback erhalten
- Plankopf-Stammdaten aus `plankopfdaten.csv` werden jetzt auch in die Inhaltsverzeichnis-Seiten geschrieben; bisher blieb dort der Stand der DIN-A2-Vorlage stehen
- `ASP`, `GEWERK` und `ANLAGE` werden in Deckblättern aus der Ordnerhierarchie gefüllt; bisher wurde ausschließlich `ASP` gesetzt
- Textbreitenanpassung im Gesamtlauf erfasst jetzt auch alle Summenblätter, nicht mehr nur die GA-FL-Blätter – gerade dort stehen die aggregierten mehrstelligen Werte

### Changed
- `Deckblatt_B` (`0000 Projekt_Deckblatt_B.dwg`) wird nicht mehr erzeugt. Das Aufräumen bestehender Dateien bleibt erhalten, damit Altbestände beim nächsten Lauf verschwinden. `0000 Projekt_Deckblatt_A.dwg` bleibt wie bisher unangetastet
- OpenCirt-Tab aufgeräumt: die Schaltflächen *Plankopf-Daten setzen*, *Deckblätter erstellen*, *BMK erstellen*, *BAS erstellen*, *GA-FL erstellen* und *Textbreiten anpassen* entfallen. Alle sechs Schritte laufen als Teil von *Projekt erstellen*
- Reihenfolge der verbliebenen Schaltflächen nach Wichtigkeit im Arbeitsablauf: *Projekt erstellen*, *Projekt bereinigen*, dann getrennt durch eine Linie die Ausgaben *PDF publizieren*, *IO-Liste erstellen*, *Sensorliste erstellen*
- Reservezeilen der IO-Liste führen jetzt den ASP mit — der Klemmenplatz ist physisch vorhanden und einem Automationsschwerpunkt zugeordnet, auch wenn kein Datenpunkt darauf liegt. Anlage, BMK, BAS und Integrationsart bleiben leer
- *IO-Belegung erstellen* heißt jetzt *IO-Liste erstellen* und deckt über den Filter beide Anwendungsfälle ab. Der Ausgabename richtet sich nach dem Filter: `IO-Belegungsliste.ods` bei reinem HW-Filter, sonst `Datenpunktliste[_<Filter>].ods`
- **Der Export liest die fertigen GA-FL-Blätter statt der Extraktion.** Bisher wurden Zwischenstand (Temp-CSVs) und Referenz (`GA_FL_VORLAGE.ods`) getrennt ausgewertet und die Logik von `FillGaFl.lsp` nachgebaut — beides konnte auseinanderlaufen, und Korrekturen von Hand im Blatt kamen in der Liste nicht an. Gelesen wird jetzt über eine Side-Database, also ohne die Zeichnungen im Editor zu öffnen: kein Bildaufbau, kein Flackern, keine Dateisperre. Voraussetzung ist ein vollständiger Lauf von *Projekt erstellen*; ohne GA-FL-Blätter meldet der Export das jetzt klar statt eine unvollständige Liste zu erzeugen

## [1.1.0] – 2026-03-26

### Added
- PDF-Publish: Batch-Instanz via `/b` mit SCR-Steuerung, Marker-basierte Completion und `BACKGROUNDPLOT=0` für synchronen Publish
- IO-Belegung: ODS-Vorlagen-basierte Generierung von IO-Belegungsplänen (`OdsTemplateWriter`)
- Sensorliste: Automatische Sensorlisten-Erstellung mit Keyword-Matching (`SensorKeywordLoader`)
- Neue Vorlagen: `OC_VORLAGE_IO_BELEGUNG_V_1.ods`, `OC_VORLAGE_SENSORLISTE_V_1.ods`
- Neue Referenzdaten: `iomodule.csv`, `sensor.csv` im Sample-Projekt

### Changed
- `.gitignore` aufgeräumt und erweitert (CMake, C++, Qt, OS-Patterns)
- `.editorconfig` hinzugefügt (einheitliche Formatierung)
- `.gitattributes` hinzugefügt (Zeilenende-Normalisierung Windows/Linux)
- Vendor-Libraries pugixml und quazip als Source direkt getrackt (statt nested git repos)
- README.md: Titelzeile angepasst

## [1.0.0] – 2026-03-15

### Added
- **General-Tab**: Quellordner, Dateifilter (Include/Exclude), Unterordner-Rekursion, Backup-Konfiguration
- **Text-Tab**: Suchen/Ersetzen in DBText/MText mit Regex, Groß-/Kleinschreibung, Mehrfachersetzung
- **Attributes-Tab**: Blockattribute ändern (Filter nach Block, Tag, Sichtbarkeit)
- **Layers-Tab**: Layer löschen, umbenennen, einfrieren, Farbe/Linientyp/Transparenz ändern, Layer-Analyse
- **LISP-Tab**: Automatisierte LISP-Skript-Ausführung auf beliebig viele DWG-Dateien via SCR
- **OpenCirt-Tab**: GA-Planungsautomatisierung (Plankopf, BMK-Nummerierung, BAS-Generierung, GA-FL, Summenblätter, Deckblatt, Inhaltsverzeichnis, PDF-Publish)
- Sample-Projekt mit Vorlagen, Blockbibliothek, LISP-Skripten und Bedienungsanleitung
- Lizenz: BSL 1.1 (Licensor: Oliver Hagmann, Change Date: 2030-03-02, Change License: AGPLv3)
