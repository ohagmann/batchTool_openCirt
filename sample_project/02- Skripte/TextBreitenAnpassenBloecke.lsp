;;; Text-Breitenfaktor NUR bei Ueberschreitung der Rechteckgrenzen anpassen
;;; - Erweitert fuer Attribute und Rechtecke in Bloecken
;;; - NUR Texte/Attribute bearbeiten, die MITTIG im Rechteck positioniert sind
;;; - NUR anpassen wenn Text ueber Grenzen hinausragt
;;; - Standardbreitenfaktor 1.0 nicht aendern wenn Text passt
;;;
;;; Version: 2.0 - Angepasst fuer C++ Plugin-Orchestrierung
;;; Konvention: Dateiname ohne .lsp = Funktionsname

(defun TextBreitenAnpassenBloecke (/ ss i ent txt-obj rect-list txt-center
                         rect-obj width-txt width-rect height-txt height-rect
                         new-width-factor current-width minpt maxpt
                         count-adjusted rect-center tolerance
                         all-text-entities all-rect-entities)

  (princ "\nSuche mittig positionierte Texte/Attribute in Rechtecken...\n")

  ;; Toleranz fuer "mittig" Check (in Zeichnungseinheiten)
  (setq tolerance 0.01)

  ;; Funktion zum Pruefen ob Punkt in Rechteck liegt
  (defun point-in-rect-p (pt minpt maxpt / x y)
    (setq x (car pt)
          y (cadr pt))
    (and (>= x (car minpt))
         (<= x (car maxpt))
         (>= y (cadr minpt))
         (<= y (cadr maxpt))))

  ;; Funktion zum Pruefen ob Punkt mittig im Rechteck ist
  (defun is-centered-p (pt minpt maxpt tolerance / center-x center-y)
    (setq center-x (/ (+ (car minpt) (car maxpt)) 2.0))
    (setq center-y (/ (+ (cadr minpt) (cadr maxpt)) 2.0))
    (and (< (abs (- (car pt) center-x)) tolerance)
         (< (abs (- (cadr pt) center-y)) tolerance)))

  ;; Funktion zum Pruefen ob Polylinie ein Rechteck ist
  (defun is-rectangle-p (ent / vert-list vertex-count coords pt1 pt2 pt3 pt4)
    (setq vert-list (entget ent))
    (setq vertex-count 0)
    (setq coords '())

    ;; Fuer LWPOLYLINE
    (if (= (cdr (assoc 0 vert-list)) "LWPOLYLINE")
      (progn
        (foreach item vert-list
          (if (= (car item) 10)
            (progn
              (setq coords (cons (cdr item) coords))
              (setq vertex-count (1+ vertex-count)))))
        (setq coords (reverse coords))))

    ;; Pruefen: Genau 4 Ecken und geschlossen?
    (if (and (= vertex-count 4)
             (= (logand (cdr (assoc 70 vert-list)) 1) 1))
      (progn
        (setq pt1 (nth 0 coords))
        (setq pt2 (nth 1 coords))
        (setq pt3 (nth 2 coords))
        (setq pt4 (nth 3 coords))

        ;; Pruefen ob achsenparalleles Rechteck
        (or
          (and (= (cadr pt1) (cadr pt2))
               (= (car pt2) (car pt3))
               (= (cadr pt3) (cadr pt4))
               (= (car pt4) (car pt1)))
          (and (= (car pt1) (car pt2))
               (= (cadr pt2) (cadr pt3))
               (= (car pt3) (car pt4))
               (= (cadr pt4) (cadr pt1)))))
      nil))

  ;; Funktion zum Sammeln aller Text-Entitaeten (inkl. Attribute in Bloecken)
  (defun collect-all-text-entities (/ ss-text ss-attrib ss-blocks block-list
                                      text-entities i ent block-ent
                                      attrib-list j attrib-ent transformed-pt
                                      block-data insert-pt rotation scale)
    (setq text-entities '())

    ;; 1. Normale Texte sammeln
    (setq ss-text (ssget "_X" '((0 . "TEXT"))))
    (if ss-text
      (progn
        (setq i 0)
        (repeat (sslength ss-text)
          (setq ent (ssname ss-text i))
          (setq text-entities (cons (list ent "TEXT" nil) text-entities))
          (setq i (1+ i)))))

    ;; 2. Normale Attribute sammeln (falls vorhanden)
    (setq ss-attrib (ssget "_X" '((0 . "ATTRIB"))))
    (if ss-attrib
      (progn
        (setq i 0)
        (repeat (sslength ss-attrib)
          (setq ent (ssname ss-attrib i))
          (setq text-entities (cons (list ent "ATTRIB" nil) text-entities))
          (setq i (1+ i)))))

    ;; 3. Bloecke durchsuchen
    (setq ss-blocks (ssget "_X" '((0 . "INSERT"))))
    (if ss-blocks
      (progn
        (setq i 0)
        (repeat (sslength ss-blocks)
          (setq block-ent (ssname ss-blocks i))
          (setq block-data (entget block-ent))

          ;; Block-Transformationsdaten
          (setq insert-pt (cdr (assoc 10 block-data)))
          (setq rotation (if (assoc 50 block-data)
                           (cdr (assoc 50 block-data))
                           0.0))
          (setq scale (if (assoc 41 block-data)
                        (cdr (assoc 41 block-data))
                        1.0))

          ;; Attribute des Blocks sammeln
          (setq attrib-list (get-block-attributes block-ent))
          (foreach attrib-ent attrib-list
            (setq text-entities
              (cons (list attrib-ent "BLOCK-ATTRIB"
                         (list insert-pt rotation scale))
                    text-entities)))

          (setq i (1+ i)))))

    text-entities)

  ;; Funktion zum Sammeln aller Rechteck-Entitaeten (inkl. in Bloecken)
  (defun collect-all-rect-entities (/ ss-poly ss-blocks rect-entities
                                      i ent block-ent block-data
                                      insert-pt rotation scale block-refs)
    (setq rect-entities '())

    ;; 1. Normale Polylinien sammeln
    (setq ss-poly (ssget "_X" '((0 . "LWPOLYLINE") (70 . 1))))
    (if ss-poly
      (progn
        (setq i 0)
        (repeat (sslength ss-poly)
          (setq ent (ssname ss-poly i))
          (if (is-rectangle-p ent)
            (setq rect-entities (cons (list ent "LWPOLYLINE" nil) rect-entities)))
          (setq i (1+ i)))))

    rect-entities)

  ;; Hilfsfunktion zum Ermitteln der Block-Attribute
  (defun get-block-attributes (block-ent / attrib-list next-ent)
    (setq attrib-list '())
    (setq next-ent (entnext block-ent))

    (while (and next-ent
                (= (cdr (assoc 0 (entget next-ent))) "ATTRIB"))
      (setq attrib-list (cons next-ent attrib-list))
      (setq next-ent (entnext next-ent)))

    (reverse attrib-list))

  ;; Funktion zum Transformieren von Block-Koordinaten
  (defun transform-block-point (pt block-data / insert-pt rotation scale
                                 x y new-x new-y cos-r sin-r)
    (if block-data
      (progn
        (setq insert-pt (nth 0 block-data))
        (setq rotation (nth 1 block-data))
        (setq scale (nth 2 block-data))

        ;; Skalierung anwenden
        (setq x (* (car pt) scale))
        (setq y (* (cadr pt) scale))

        ;; Rotation anwenden
        (setq cos-r (cos rotation))
        (setq sin-r (sin rotation))
        (setq new-x (- (* x cos-r) (* y sin-r)))
        (setq new-y (+ (* x sin-r) (* y cos-r)))

        ;; Translation anwenden
        (list (+ new-x (car insert-pt))
              (+ new-y (cadr insert-pt))))
      pt))

  ;; Funktion zum Ermitteln der tatsaechlichen Textposition
  (defun get-text-center (ent-data entity-type block-data / align-type pt)
    (setq align-type (cdr (assoc 72 ent-data)))
    (cond
      ((and align-type (> align-type 0))
       (setq pt (cdr (assoc 11 ent-data))))  ; Ausrichtungspunkt
      (t
       (setq pt (cdr (assoc 10 ent-data)))))  ; Einfuegepunkt

    ;; Transformation fuer Block-Attribute anwenden
    (if (and block-data (equal entity-type "BLOCK-ATTRIB"))
      (transform-block-point pt block-data)
      pt))

  ;; Funktion zum Berechnen der tatsaechlichen Textbreite
  (defun get-actual-text-width (ent entity-type block-data / txt-obj txt-str txt-ht width-factor
                                box minpt maxpt scale-factor)
    ;; Fuer Block-Attribute Skalierung beruecksichtigen
    (setq scale-factor 1.0)
    (if (and block-data (equal entity-type "BLOCK-ATTRIB"))
      (setq scale-factor (nth 2 block-data)))

    ;; Versuche die Bounding Box des Textes zu ermitteln
    (setq txt-obj (vlax-ename->vla-object ent))
    (vla-getboundingbox txt-obj 'minpt 'maxpt)
    (setq minpt (vlax-safearray->list minpt))
    (setq maxpt (vlax-safearray->list maxpt))
    ;; Breite der Bounding Box mit Skalierung
    (* (- (car maxpt) (car minpt)) scale-factor))

  ;; === HAUPTLOGIK ===

  ;; Alle Text-Entitaeten sammeln (inkl. Attribute in Bloecken)
  (setq all-text-entities (collect-all-text-entities))
  (princ (strcat "\n" (itoa (length all-text-entities)) " Text-Entitaeten gefunden.\n"))

  ;; Alle Rechteck-Entitaeten sammeln
  (setq all-rect-entities (collect-all-rect-entities))
  (princ (strcat "\n" (itoa (length all-rect-entities)) " Rechtecke gefunden.\n"))

  ;; Durch alle Text-Entitaeten iterieren
  (setq count-adjusted 0)

  (foreach text-entity all-text-entities
    (setq ent (nth 0 text-entity))
    (setq entity-type (nth 1 text-entity))
    (setq block-data (nth 2 text-entity))
    (setq txt-obj (entget ent))
    (setq txt-center (get-text-center txt-obj entity-type block-data))

    ;; Suche passendes Rechteck
    (foreach rect-entity all-rect-entities
      (setq rect-ent (nth 0 rect-entity))

      ;; Bounding Box des Rechtecks
      (vla-getboundingbox
        (vlax-ename->vla-object rect-ent)
        'minpt
        'maxpt)
      (setq minpt (vlax-safearray->list minpt))
      (setq maxpt (vlax-safearray->list maxpt))

      ;; Pruefen ob Text MITTIG in diesem Rechteck liegt
      (if (is-centered-p txt-center minpt maxpt tolerance)
        (progn
          (princ (strcat "\n\n" entity-type " \"" (cdr (assoc 1 txt-obj))
                       "\" ist mittig im Rechteck"))

          ;; Breiten berechnen
          (setq width-rect (- (car maxpt) (car minpt)))
          (setq width-txt (get-actual-text-width ent entity-type block-data))

          (princ (strcat "\n  Rechteckbreite: " (rtos width-rect 2 2)))
          (princ (strcat "\n  Aktuelle Textbreite: " (rtos width-txt 2 2)))

          ;; NUR anpassen wenn Text ueber Grenzen hinausragt
          (if (> width-txt width-rect)
            (progn
              ;; Aktuellen Breitenfaktor holen
              (setq current-width (cdr (assoc 41 txt-obj)))
              (if (not current-width) (setq current-width 1.0))

              ;; Neuen Breitenfaktor berechnen (95% der Rechteckbreite)
              (setq new-width-factor
                (* current-width (/ (* width-rect 0.95) width-txt)))

              ;; Breitenfaktor aktualisieren
              (if (assoc 41 txt-obj)
                (setq txt-obj
                  (subst (cons 41 new-width-factor)
                         (assoc 41 txt-obj)
                         txt-obj))
                (setq txt-obj
                  (append txt-obj (list (cons 41 new-width-factor)))))

              (entmod txt-obj)
              (entupd ent)
              (setq count-adjusted (1+ count-adjusted))

              (princ (strcat "\n  >>> ANGEPASST! Neuer Breitenfaktor: "
                           (rtos new-width-factor 2 3))))

            ;; Text passt bereits - NICHT aendern
            (princ "\n  Text passt bereits - keine Anpassung noetig"))))))

  (princ (strcat "\n\n========================================"))
  (princ (strcat "\nFERTIG!"))
  (princ (strcat "\n" (itoa count-adjusted) " Texte/Attribute wurden angepasst"))
  (princ (strcat "\n" (itoa (- (length all-text-entities) count-adjusted))
               " Texte/Attribute benoetigten keine Anpassung"))
  (princ "\n========================================\n")

  (princ)
)

;;; Hilfsfunktion zum manuellen Anpassen einzelner Texte/Attribute
(defun c:TEXTFIT1 (/ ent txt-obj poly-ent txt-center rect-center
                    minpt maxpt tolerance entity-type)

  (setq tolerance 0.01)

  (princ "\nWaehle einen Text oder Attribut aus: ")
  (setq ent (car (entsel)))

  (if ent
    (progn
      (setq txt-obj (entget ent))
      (setq entity-type (cdr (assoc 0 txt-obj)))

      ;; Pruefen ob es Text oder Attribut ist
      (if (or (= entity-type "TEXT") (= entity-type "ATTRIB"))
        (progn
          (setq txt-center (get-text-center txt-obj entity-type nil))

          (princ "\nWaehle das Rechteck aus: ")
          (setq poly-ent (car (entsel)))

          (if poly-ent
            (progn
              ;; Pruefen ob es ein Rechteck ist
              (if (is-rectangle-p poly-ent)
                (progn
                  ;; Bounding Box des Rechtecks
                  (vla-getboundingbox
                    (vlax-ename->vla-object poly-ent)
                    'minpt
                    'maxpt)
                  (setq minpt (vlax-safearray->list minpt))
                  (setq maxpt (vlax-safearray->list maxpt))

                  ;; Pruefen ob Text mittig ist
                  (if (is-centered-p txt-center minpt maxpt tolerance)
                    (progn
                      ;; Breiten berechnen
                      (setq width-rect (- (car maxpt) (car minpt)))
                      (setq width-txt (get-actual-text-width ent entity-type nil))

                      (princ (strcat "\nRechteckbreite: " (rtos width-rect 2 2)))
                      (princ (strcat "\nTextbreite: " (rtos width-txt 2 2)))

                      ;; NUR anpassen wenn noetig
                      (if (> width-txt width-rect)
                        (progn
                          (setq current-width (cdr (assoc 41 txt-obj)))
                          (if (not current-width) (setq current-width 1.0))

                          (setq new-width-factor
                            (* current-width (/ (* width-rect 0.95) width-txt)))

                          (if (assoc 41 txt-obj)
                            (setq txt-obj
                              (subst (cons 41 new-width-factor)
                                     (assoc 41 txt-obj)
                                     txt-obj))
                            (setq txt-obj
                              (append txt-obj (list (cons 41 new-width-factor)))))

                          (entmod txt-obj)
                          (entupd ent)

                          (princ (strcat "\nBreitenfaktor geaendert auf: "
                                       (rtos new-width-factor 2 3))))
                        (princ "\nText passt bereits - keine Anpassung noetig")))
                    (princ "\nText ist nicht mittig im Rechteck!")))
                (princ "\nGewaehltes Objekt ist kein Rechteck!")))
            (princ "\nKeine Umgrenzung gewaehlt.")))
        (princ "\nGewaehltes Objekt ist kein Text oder Attribut!"))
      )
    (princ "\nKein Text gewaehlt."))

  (princ)
)

;;; Testfunktion zum Anzeigen mittiger Texte/Attribute
(defun c:TEXTMITTIG (/ all-text-entities all-rect-entities txt-center minpt maxpt
                       count-centered tolerance)

  (setq tolerance 0.01)
  (setq count-centered 0)

  (princ "\nSuche mittig positionierte Texte/Attribute...\n")

  ;; Alle Entitaeten sammeln
  (setq all-text-entities (collect-all-text-entities))
  (setq all-rect-entities (collect-all-rect-entities))

  ;; Texte/Attribute pruefen
  (foreach text-entity all-text-entities
    (setq ent (nth 0 text-entity))
    (setq entity-type (nth 1 text-entity))
    (setq block-data (nth 2 text-entity))
    (setq txt-obj (entget ent))
    (setq txt-center (get-text-center txt-obj entity-type block-data))

    (foreach rect-entity all-rect-entities
      (setq rect-ent (nth 0 rect-entity))
      (vla-getboundingbox
        (vlax-ename->vla-object rect-ent)
        'minpt
        'maxpt)
      (setq minpt (vlax-safearray->list minpt))
      (setq maxpt (vlax-safearray->list maxpt))

      (if (is-centered-p txt-center minpt maxpt tolerance)
        (progn
          (setq count-centered (1+ count-centered))
          (princ (strcat "\nMittiger " entity-type " gefunden: \""
                       (cdr (assoc 1 txt-obj)) "\""))
          ;; Text highlighten
          (redraw ent 3)))))

  (princ (strcat "\n\nGesamt: " (itoa count-centered)
               " mittig positionierte Texte/Attribute gefunden.\n"))
  (princ)
)

;;; Interaktive Befehle (Aliase)
(defun c:TextBreitenAnpassenBloecke () (TextBreitenAnpassenBloecke))
(defun c:TEXTANPASSEN () (TextBreitenAnpassenBloecke))

(princ "\nTextBreitenAnpassenBloecke.lsp geladen (v2.0)")
(princ "\nBefehle: TextBreitenAnpassenBloecke, TEXTANPASSEN, TEXTFIT1, TEXTMITTIG")
(princ "\n=========================================\n")
