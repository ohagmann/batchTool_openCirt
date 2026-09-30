;;; repro_vla.lsp - BricsCAD V26 Linux: a document returned by (vla-Open) is
;;; taken for an object of an earlier, already closed document
;;; ("Automation Error DISP_E_UNKNOWNNAME; [IAcadAttributeReference] identifier
;;;  name not recognised in [MODELSPACE] property").
;;;
;;; Usage:
;;;   1. put repro_vla.lsp and repro_vla_template.dwg into the same folder
;;;   2. start BricsCAD, (load "<folder>/repro_vla.lsp")
;;;   3. command REPRO_VLA, enter the folder when asked
;;; The command opens a copy of the template 300 times with (vla-Open), reads
;;; the tag of every attribute and closes the document without saving.
;;; It stops at the first automation error and reports the iteration.
;;; After a failure the document of that iteration stays open, because it
;;; cannot be addressed any more; close BricsCAD before the next run.
;;; Result: printed and written to repro_vla_result.txt in the TEMPPREFIX folder.

(vl-load-com)

(setq *rv-count* 0)

;; one document: open, read the tag of every attribute, close without saving
(defun rv-one (docs work / doc ms k ent ar)
  (setq doc (vla-Open docs work :vlax-false))
  (setq ms (vla-get-ModelSpace doc))
  (setq k 0)
  (repeat (vla-get-Count ms)
    (setq ent (vla-Item ms k))
    (if (= (vla-get-ObjectName ent) "AcDbBlockReference")
      (progn
        (setq ar (vla-GetAttributes ent))
        (foreach a (vlax-safearray->list (vlax-variant-value ar))
          (vla-get-TagString a)
          (setq *rv-count* (1+ *rv-count*)))))
    (setq k (1+ k)))
  (vla-Close doc :vlax-false)
  T)

(defun rv-run (folder n / docs src work i r err f msg)
  (setq folder (vl-string-right-trim "/\\" folder))
  (setq src  (strcat folder "/repro_vla_template.dwg")
        work (strcat folder "/repro_vla_work.dwg"))
  (if (not (findfile src))
    (progn (princ (strcat "\nTemplate not found: " src)) (exit)))
  (setq docs (vla-get-Documents (vlax-get-acad-object)))
  (setq i 0 err nil *rv-count* 0)
  (while (and (< i n) (not err))
    (setq i (1+ i))
    (vl-file-delete work)
    (vl-file-copy src work)
    (setq r (vl-catch-all-apply 'rv-one (list docs work)))
    (if (vl-catch-all-error-p r)
      (setq err (vl-catch-all-error-message r))))
  (setq msg (if err
              (strcat "FAILED at document " (itoa i) " of " (itoa n) ": " err)
              (strcat "no failure in " (itoa n) " documents")))
  (setq msg (strcat msg " (attributes read so far: " (itoa *rv-count*) ")"))
  (setq f (open (strcat (getvar "TEMPPREFIX") "repro_vla_result.txt") "a"))
  (write-line msg f)
  (close f)
  (princ (strcat "\n" msg))
  (princ))

(defun c:REPRO_VLA (/ folder)
  (setq folder (getstring T "\nFolder with repro_vla_template.dwg: "))
  (rv-run folder 300))

(princ "\nrepro_vla.lsp loaded. Command: REPRO_VLA")
(princ)
