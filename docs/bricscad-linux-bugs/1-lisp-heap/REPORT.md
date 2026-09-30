# LISP: "out of LISP 'Heap' memory at [gc]" from the third document on

## Summary

In one BricsCAD session only the first two documents whose LISP code needs a
heap garbage collection work. In every further document the first heap
collection fails with

    out of LISP 'Heap' memory at [gc]

although `(mem)` reports 121 MB of heap available and less than 1 MB in use.
The LISP function that is running is aborted. This makes batch processing of
drawings with LISP impossible on Linux.

## Environment

| | |
|---|---|
| Product | BricsCAD Ultimate V26.2.07 (x64), de_DE |
| Platform | Linux Mint 22.3 (Ubuntu 24.04 base), kernel 7.0.0-34, X11 |
| FIBERWORLD | 1 |
| Profile | reproduced in a new, empty user profile, no add-ons loaded |
| Windows | the same scripts run on V26.2.07 for Windows over about 3,200 document openings per session |

## Steps to reproduce

1. Start BricsCAD with a new, empty profile.
2. Run the attached script: command `SCRIPT`, file `repro_heap.scr`.

The script creates six new drawings one after the other with `QNEW`. In each
of them it builds a string of 3,000 characters, character by character, five
times, and closes the drawing without saving. The drawing itself is not
touched.

The result is printed and written to `repro_heap_result.txt` in the
`TEMPPREFIX` folder.

## Actual result

    document 1: ok, string length 3000
    document 2: ok, string length 3000
    document 3: out of LISP 'Heap' memory at [gc]
    document 4: out of LISP 'Heap' memory at [gc]
    document 5: out of LISP 'Heap' memory at [gc]
    document 6: out of LISP 'Heap' memory at [gc]

The result is the same every time, both when the script is started with
`bricscad -b repro_heap.scr` and when it is started with the `SCRIPT` command
in a running session.

## Expected result

`ok` in all six documents, as on Windows.

## Observations

- What counts is the number of documents that needed a heap collection, not
  the number of documents opened. Documents with little LISP work in between
  do not change the result: in a run with 16 documents, 13 of them without any
  garbage collection, the failure came in the third document that collected.
- It makes no difference whether the earlier documents are closed or stay
  open.
- Documents that already exist keep working. Only documents created after
  the second collecting document fail.
- `(mem)` after the work in the first three documents:

      document 1   VM Page 182.402 MB available   VM Heap 121.598 MB available, 3.719 MB used
      document 2   VM Page 179.152 MB available   VM Heap 124.844 MB available, 2.146 MB used
      document 3   VM Page 182.402 MB available   VM Heap 121.598 MB available, 0.494 MB used

  In the second document the border between page and heap memory moves by
  3.25 MB. From the third document on the heap cannot grow any more.
- A function that fails can be called again in the same document, it fails
  again.
- An explicit `(gc)` in an affected document can crash BricsCAD
  ("The program has crashed. Reason: Illegal access").

## Tried without effect

- `liblispex.so.cfg`: `VM_MAXIMUM_MEM` 1024, `VM_PAGE_OVER_HEAP` 30,
  `MDI_LISP_COMPRESS` 64. The values were applied, `(mem)` showed them.
- `LISPINIT` 0, `SDI` 1
- `(gc)` or `(gc-free-unused-memory)` before closing a document
- `(gc-growing-factor)`, `(gc-low-threshold)`, `(gc-compact-threshold)`,
  `(gc-min-objects)`, `(expand)`, `(alloc)` at the start of each document
- a delay of one second between the documents
- closing all documents, including the first one, before opening the next
- starting without address space randomisation (`setarch -R`)

`NEXTFIBERWORLD` 0 could not be tested: with it BricsCAD does not execute a
script given with `-b` on this system.

## Impact

Any LISP batch run over more than two drawings fails on Linux. A workaround
inside one session is not known. Distributing the work over several BricsCAD
processes with two drawings each works, but takes about six times as long.

## Attachment

- `repro_heap.scr`
