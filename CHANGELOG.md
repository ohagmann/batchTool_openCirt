# Changelog

Alle wesentlichen Änderungen am Batchtool / OpenCirt Plugin werden in dieser Datei dokumentiert.

Format basiert auf [Keep a Changelog](https://keepachangelog.com/de/1.1.0/).
Versionierung: Bump bei Änderungen am Plugin-Binary (C++/GUI). Kein Bump bei reinen Änderungen an Vorlagen, LISP-Skripten, Dokumentation oder Repo-Konfiguration.

## [Unreleased]

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
