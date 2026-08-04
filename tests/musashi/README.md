# 68000 opcode oracle

Musashi in this tree emulates one CPU, the 68000 the Neo Geo carries -
a Toshiba TMP68HC000 in the CD hardware. Most work on it is meant to
change nothing a game can observe: removing an instruction the chip
never had, collapsing a table, folding a test that was already
constant. This checks that claim rather than asserting it.

Every one of the 65536 opcode words is executed from the same starting
state against a bus that answers the same way forever, and everything
the instruction did - each read, each write, each interrupt
acknowledgement, the register file afterwards, the cycles it billed -
is folded into a digest. `golden` holds the digest of the whole run.

    make check      build and compare against golden
    make sanitize   the same run under ASan and UBSan
    make dump       write per-opcode lines to oracle.txt

The bus is stateless: reads outside the vector table and the code
window are a fixed function of the address, and writes are recorded
rather than stored. No opcode can reach the next one, so there is no
order dependence and no accumulated state to argue about.

## When it fails

A mismatch is one digest against another and says nothing about where.
Run `make dump` here and on a build without your change, then diff the
two `oracle.txt`: each line is an opcode, its cycle count and its own
digest, so the differing encodings fall out directly.

## Re-recording

`make golden` overwrites the digest. That is only correct when the
change is *meant* to alter behaviour, and the commit message has to say
which encodings moved and why. MOVE16 was the last one: F620-F627 were
running a 68040 block move on a 68000 instead of trapping.

## What it does not cover

One instruction from reset, with fixed registers and fixed extension
words. It will not catch anything that needs a particular register
value, a sequence of instructions, or interrupt timing - it is a net
under mechanical changes to the core, not a conformance suite.
