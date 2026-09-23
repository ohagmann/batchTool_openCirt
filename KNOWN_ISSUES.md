# Bekannte Probleme

Hier stehen Probleme, die beim Einsatz von openCirt auftreten, deren Ursache aber nicht im Plugin liegt, sondern in BricsCAD oder Windows. Für Änderungen und behobene Fehler im Plugin selbst siehe [CHANGELOG.md](CHANGELOG.md), für Bedienfehler die [Bedienungsanleitung](sample_project/BEDIENUNGSANLEITUNG.md), Abschnitt 12.

---

## 1. GDI-Objekt-Leck in BricsCAD V26 – Absturz beim zweiten Gesamtlauf in derselben Sitzung

**Betrifft:** BricsCAD V26 unter Windows, beobachtet mit V26.2.07 (Stand September 2026). Ob neuere Versionen betroffen sind, ist nicht geprüft.

### Symptom

Der erste Gesamtlauf einer Sitzung läuft sauber durch. Ein zweiter vollständiger Gesamtlauf in derselben BricsCAD-Sitzung bricht ab: BricsCAD meldet „Fehler beim Ausführen von _open" und beendet sich (APPCRASH). Im Windows-Ereignisprotokoll steht dazu ein Eintrag `GDIObjectLeak` für `bricscad.exe`. Bei einem entsprechend großen Projekt kann es bereits den ersten Lauf treffen.

### Ursache

BricsCAD gibt bei jedem Öffnen und Schließen eines Dokuments etwa zwei GDI-Objekte nicht wieder frei – rund eines je `OPEN` und eines je neu angelegter Datei. Sie bleiben bis zum Beenden von BricsCAD belegt. Das Verhalten ist in einem leeren Benutzerprofil ohne Add-ons reproduzierbar; BatchTool/openCirt, Startup-LISPs und andere Erweiterungen sind daran unbeteiligt.

Windows begrenzt GDI-Objekte auf 10.000 je Prozess. Ein Gesamtlauf öffnet jede Zeichnung mehrfach (Extraktion, BMK, BAS, GA-FL, Summen, Deckblätter, Inhaltsverzeichnis). Bei einem Projekt mit rund 3.200 Dateiöffnungen steht BricsCAD nach dem Lauf bei etwa 7.200 GDI-Objekten; der zweite Lauf überschreitet die Grenze. Faustformel für den Bedarf eines Laufs: 2 × Anzahl der Dateiöffnungen plus die Grundlast der Sitzung (einige hundert Objekte).

### Beobachten

Task-Manager → Reiter „Details" → Rechtsklick auf einen Spaltenkopf → „Spalten auswählen" → „GDI-Objekte" einblenden. Der Wert für `bricscad.exe` steigt während des Laufs und fällt danach nicht mehr.

### Abhilfe 1: BricsCAD vor jedem Gesamtlauf neu starten (Regel)

Nach jedem vollständigen Gesamtlauf BricsCAD beenden und neu starten, bevor der nächste Lauf gestartet wird. Das genügt, solange ein einzelner Lauf unter dem Limit bleibt.

### Abhilfe 2: Windows-Limit je Prozess erhöhen (Puffer)

Für sehr große Projekte oder mehrere Läufe hintereinander lässt sich das Limit je Prozess über die Registry anheben. Erfordert Administratorrechte und einen Neustart von Windows.

| | |
|---|---|
| Schlüssel | `HKEY_LOCAL_MACHINE\SOFTWARE\Microsoft\Windows NT\CurrentVersion\Windows` |
| Wert | `GDIProcessHandleQuota` (REG_DWORD) |
| Standard | 10000 |
| Zulässig | 256 bis 65536 |
| Empfehlung | 20000 – deckt zwei bis drei Läufe der oben genannten Größe |

Setzen (Eingabeaufforderung als Administrator, anschließend Windows neu starten):

```
reg add "HKLM\SOFTWARE\Microsoft\Windows NT\CurrentVersion\Windows" /v GDIProcessHandleQuota /t REG_DWORD /d 20000 /f
```

Prüfen:

```
reg query "HKLM\SOFTWARE\Microsoft\Windows NT\CurrentVersion\Windows" /v GDIProcessHandleQuota
```

Zurücksetzen: denselben `reg add`-Befehl mit `/d 10000` ausführen und neu starten.

Hinweise:

- Der Wert gilt für jeden Prozess, nicht nur für BricsCAD. Die gesamte Windows-Sitzung hat maximal 65.536 GDI-Handles; Werte weit über 20.000 erlauben einem einzelnen Prozess, die übrigen auszuhungern.
- `USERProcessHandleQuota` im selben Schlüssel (Standard 10000, zulässig 200 bis 18000) betrifft User-Objekte, nicht GDI-Objekte. Nur anheben, wenn im Task-Manager auch die Spalte „USER-Objekte" an das Limit kommt.
- Die Erhöhung beseitigt das Leck nicht, sie verschiebt nur die Grenze. Der Neustart vor jedem Lauf bleibt die verlässliche Regel.

### Was nicht hilft

- `SDI=1` zur Laufzeit setzen: `OPEN` öffnet weiterhin zusätzliche Dokumente, das Leck bleibt.
- Plugin entladen oder Startup-Suite leeren: das Leck tritt auch ohne jede Erweiterung auf.
