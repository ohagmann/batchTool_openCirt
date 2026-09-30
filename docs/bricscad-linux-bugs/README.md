# BricsCAD für Linux: zwei Fehler in der LISP-Umgebung

Beim Portieren von batchTool / openCirt nach Linux sind zwei Fehler in
BricsCAD V26.2.07 für Linux aufgefallen. Beide liegen in BricsCAD und treten
ohne das Plugin in einem leeren Benutzerprofil auf. Unter Windows gibt es sie
nicht. Für die bekannten Probleme unter Windows siehe
[KNOWN_ISSUES.md](../../KNOWN_ISSUES.md).

| Ordner | Fehler | Folge für openCirt |
|---|---|---|
| `1-lisp-heap` | Ab dem dritten Dokument einer Sitzung, dessen LISP-Code eine Speicherbereinigung braucht, bricht LISP mit „out of LISP 'Heap' memory" ab | Der LISP-Tab scheitert ab der dritten Zeichnung mit rechenintensivem Skript. Bis 1.6 scheiterte daran auch der Gesamtlauf; seit 1.7.0 läuft er ohne LISP |
| `2-vla-wrong-object` | vla-Funktionen liefern nach einigen geschlossenen Dokumenten ein Objekt des falschen Typs | Das LISP-Skript `OC_PROJECT_BUILD` bricht nach 20 bis 230 Blättern ab (deshalb läuft der Projektaufbau seit 1.6.0 im Plugin selbst, ohne LISP); bis 1.6 scheiterten die vla-Schritte des Gesamtlaufs in rund 5 % der Zeichnungen, seit 1.7.0 gibt es sie nicht mehr |

## Einreichen bei Bricsys

Jeder Ordner ist ein eigener Fehlerbericht:

- `REPORT.md` enthält den Text auf Englisch, fertig zum Einfügen in die
  Support-Anfrage.
- Die übrigen Dateien des Ordners sind die Anhänge.

Beide Berichte getrennt einreichen, es sind zwei unabhängige Fehler.

## Selbst nachstellen

Fehler 1: BricsCAD starten, Befehl `SCRIPT`, Datei
`1-lisp-heap/repro_heap.scr`. Das Ergebnis steht in der Befehlszeile und in
`repro_heap_result.txt` im Ordner aus `TEMPPREFIX` (üblich: `/tmp/BricsCAD`).

Fehler 2: `2-vla-wrong-object/repro_vla.lsp` laden, Befehl `REPRO_VLA`,
als Ordner den Pfad von `2-vla-wrong-object` angeben. Nach einem Fehlschlag
bleibt die letzte Testzeichnung offen, BricsCAD danach beenden.

Stand der Prüfung: 29.09.2026, BricsCAD V26.2.07. In den Versionshinweisen
bis V26.2.08 ist zu beiden Fehlern keine Korrektur genannt.
