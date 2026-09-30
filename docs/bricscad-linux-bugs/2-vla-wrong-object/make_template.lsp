;;; make_template.lsp - builds the neutral test drawing for repro_vla.lsp:
;;; one block "RV_BLOCK" with 20 attributes, inserted 20 times.
(defun rv-make-template (path / i k x y)
  (entmake '((0 . "BLOCK") (2 . "RV_BLOCK") (70 . 2) (10 0.0 0.0 0.0)))
  (entmake '((0 . "LINE") (8 . "0") (10 0.0 0.0 0.0) (11 50.0 0.0 0.0)))
  (setq i 0)
  (repeat 20
    (entmake (list '(0 . "ATTDEF") '(8 . "0")
                   (list 10 0.0 (* -4.0 (1+ i)) 0.0) '(40 . 2.5)
                   '(1 . "") (cons 3 (strcat "Tag " (itoa i)))
                   (cons 2 (strcat "RV_TAG_" (itoa i))) '(70 . 0)))
    (setq i (1+ i)))
  (entmake '((0 . "ENDBLK")))
  (setq k 0)
  (repeat 20
    (setq x (* 60.0 (rem k 5)) y (* 100.0 (/ k 5)))
    (entmake (list '(0 . "INSERT") '(8 . "0") '(2 . "RV_BLOCK") '(66 . 1) (list 10 x y 0.0)))
    (setq i 0)
    (repeat 20
      (entmake (list '(0 . "ATTRIB") '(8 . "0")
                     (list 10 x (- y (* 4.0 (1+ i))) 0.0) '(40 . 2.5)
                     (cons 1 (strcat "value " (itoa k) "/" (itoa i)))
                     (cons 2 (strcat "RV_TAG_" (itoa i))) '(70 . 0)))
      (setq i (1+ i)))
    (entmake '((0 . "SEQEND")))
    (setq k (1+ k)))
  (command "_.ZOOM" "_E")
  (command "_.SAVEAS" "" path)
  (princ))
