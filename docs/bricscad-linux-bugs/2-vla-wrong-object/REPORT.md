# LISP: vla functions return an object of the wrong type after documents were closed

## Summary

After some documents have been opened, processed with vla functions and
closed, a vla function returns an object that BricsCAD takes for an object of
an earlier, already closed document. A document returned by `(vla-Open)` is
then treated as an attribute reference, and the next call fails with

    Automation Error DISP_E_UNKNOWNNAME; [IAcadAttributeReference] identifier
    name not recognised in [MODELSPACE] property.

The object cannot be used or closed any more. The point of failure differs
from run to run.

## Environment

| | |
|---|---|
| Product | BricsCAD Ultimate V26.2.07 (x64), de_DE |
| Platform | Linux Mint 22.3 (Ubuntu 24.04 base), kernel 7.0.0-34, X11 |
| FIBERWORLD | 1 |
| Profile | reproduced in a new, empty user profile, no add-ons loaded |
| Windows | the same code runs on V26.2.07 for Windows over 323 documents without failure |

## Steps to reproduce

1. Put `repro_vla.lsp` and `repro_vla_template.dwg` into one folder.
2. Start BricsCAD with a new, empty profile and load `repro_vla.lsp`.
3. Run the command `REPRO_VLA` and enter the folder.

The command opens a copy of the template 300 times with `(vla-Open)`, reads
the tag of every attribute through the model space collection and closes the
document without saving. It stops at the first automation error.

The template contains one block with 20 attributes, inserted 20 times. It was
created with `make_template.lsp`, which is attached for reference.

The result is printed and written to `repro_vla_result.txt` in the
`TEMPPREFIX` folder.

## Actual result

Four runs, each in a new BricsCAD session:

    FAILED at document 47 of 300: Automation Error DISP_E_UNKNOWNNAME; [IAcadAttributeReference] identifier name not recognised in [MODELSPACE] property.
    FAILED at document 33 of 300: (same message)
    FAILED at document 24 of 300: (same message)
    FAILED at document 19 of 300: (same message)

## Expected result

    no failure in 300 documents

## Observations

- The wrong type is always the type of an object that was used in an earlier
  document. With a drawing that contains polylines the message names
  `[IAcadLWPolyline]` instead of `[IAcadAttributeReference]`.
- The same happens with vla functions that run inside each document. A script
  opened 323 drawings one after the other and ran this in each of them:
  `(vlax-ename->vla-object)` for every block reference, `GetAttributes`,
  `(vla-get-TagString)`, `(vla-put-TextString)`, then
  `(vla-get-Layers (vla-get-ActiveDocument (vlax-get-acad-object)))`.
  16 of the 323 drawings failed, with two kinds of message:

      [IAcadAttributeReference] identifier name not recognised in [LAYERS] property.
      [IAcadDocument] identifier name not recognised in [TAGSTRING] property.

  So the mix-up goes both ways: a document is taken for an attribute, and an
  attribute is taken for a document.
- Once `(vla-Open)` has returned a wrong object, the document cannot be
  obtained in another way either. `(vla-Item)` on the documents collection
  returns the same wrong object, also after `(vlax-release-object)`.

## Tried without effect

- `(vlax-release-object)` on every attribute, entity, model space and
  document object after use
- `(gc)` after closing each document
- both together
- `(vle-fastcom nil)`

With each of them the failure still occurs, only at a different document.

## Impact

LISP code that processes several drawings with vla functions is not reliable
on Linux. Where the error is not caught, the function is aborted and the
drawing stays unprocessed without any notice to the user.

## Attachments

- `repro_vla.lsp`
- `repro_vla_template.dwg`
- `make_template.lsp`
