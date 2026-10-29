# The project's SCSP

An implementation of the Yamaha SCSP (YMF292) written for this repository's Model 3 port, which does not
derive from MAME's: it is written from `src/sound/scsp_contract.h` -- the hardware contract, 48 fields with
their provenance -- and from the seven block tests, which are here.

    make test      the seven block tests
    make measure   what the core costs, item by item

## What is here, and what is not

The core (`scsp_own`), its effects DSP (`dsp_own`), the interface (`scsp_core.h`), the contract, the
selector, the trace format, the bench and the seven block tests. Plus the documents, which are the bulk of
the work: the knowledge weighs more than the code.

**MAME's core is not here** -- `scsp.cpp`, `scspdsp.cpp`, `scsp_mame.cpp` stay in the port, under
BSD-3-Clause, and that is where the licence boundary runs. Three tests stay with them, because their
subject IS that core: `test_scsp_core`, `test_scspdsp`, and the second half of `test_scsp_contract`, which
reads the driver's ROM.

## What this repository cannot redo, and you need to know it

**The proof lives in the game.** The numbers in the documents -- byte-identical output over 3,219,865
samples per chip, the share of a frame the sound takes, the DSP programs read off three games -- all come
out of the port driving the real ROMs. This repository carries the code and the documents; it can neither
redo nor recheck those numbers. For that, it is `fv2-recomp`.

What it can do alone, and that is already a lot: build without a line of MAME, and pass its seven block
tests.

## Where it comes from, and how it gets back

This repository is a `git subtree` of `fv2-recomp`, under the prefix `scsp/`. The paths are the same on
both sides: `src/sound/scsp_own.cpp` here is `scsp/src/sound/scsp_own.cpp` there, so a `diff`
between the two reads without translation. From the port, in both directions:

    git subtree push --prefix=scsp scsp main      what changed here goes there
    git subtree pull --prefix=scsp scsp main      and what changed there comes back

Nothing copies, nothing is exported: every commit on `main` is the port's own, with its message and its
author. That is not the same thing as a dumped archive, and it is the whole point.

**The history starts the day the subsystem took its prefix**, not at its first line. The forty commits
that made it -- the contract, the seven block tests, the DSP, every measurement -- are in `fv2-recomp`,
where `git log --follow scsp/src/sound/scsp_own.cpp` walks back through all of them. A `subtree split`
cannot carry them across: before the move, the prefix `scsp/` did not exist.
