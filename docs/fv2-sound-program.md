# The sound program, read in its own code

> This document is one of the nine on the sound. The front door, which says where everything is:
> [scsp/docs/sound.md](../scsp/docs/sound.md).

Begun on 2026-10-24. **It is a beginning, not a complete record**: the program is 512 KB and what follows
covers a handful of its functions. Every line says where it was read.

Why this document: the repository's overriding rule says to open the decompilation **before** measuring, and
for the PowerPC it exists (`docs/fv2-match-flow.md`). For the sound there was nothing, and it was costing
— `docs/fv2-console-sound.md` is 641 lines of questions, and `soundboard.cpp` carries a **guessed** structure
behind a guard (`driver_known`) because nobody had read it. Two of its three hypotheses are now **confirmed
by the program itself**.


## 1. The image, and the byte order settled by measurement

The program is **a single EPROM**, 524 288 bytes, mapped at `0x600000` by the board
(`src/sound/soundboard.h`). `epr-20600a.21` for Fighting Vipers 2, `epr-19231.21` for Virtua Fighter 3.

The byte order is the only trap, and it was **settled, not assumed**: `romset.cpp` lays the SOUND region
down with `stride 1, chunk 1, swap false`, so the file's bytes go in as they are; then
`SoundBoard::Bus::word` reads `v[a] | v[a+1] << 8`, that is **the low byte first**, when the 68000 is
big-endian. The reset vectors say so beyond appeal:

| | SSP | reset PC |
|---|---|---|
| swapped (low byte first) | `00002000` | **`00600120`** — stack at the top of the 8 KB, code in the ROM |
| plain big-endian | `00000020` | `60002001` — odd, off the board |

And after the swap, vectors 2 and 3 land at `006005AC` and `006005B0`, in the ROM as well. Four sound
vectors against one false one.

`tools/sound/extract-sound.py` does the extraction and the swap, and **refuses to write** if the reset PC
does not land in the program ROM — so that a wrong image does not get disassembled for an hour.

**Both games give the same SSP and the same reset PC** (`00002000` / `00600120`): it is the same driver,
which supports the idea that `driver_known` is talking about one driver family and not two.


## 2. The entry points, which the 68000 carries itself

No need for a recompiler map as for the PowerPC: a 68000 carries its entries in its exception table. It has
**72 longs** here — not 256: the reset PC is `0x600120`, so the table stops where the code begins, and
reading beyond it reads code as vectors (`FV2SoundSetup.java`'s first pass was caught by that and placed
three functions in the middle of another function).

| vector | target | |
|---|---|---|
| reset | `00600120` | |
| bus error | `006005AC` | |
| address error | `006005B0` | |
| **IRQ 1** | **`00600780`** | the driver's round: both chips' 32 slots |
| **IRQ 2** | **`006032D2`** | |
| **IRQ 3** | `006005BE` | |
| IRQ 4-7, and some fifty others | `006005B4` | a common catch-all handler |

Three interrupt levels served, and **only the first SCSP's interrupts reach the 68000**
(`soundboard.cpp`: the second has an empty `h.irq`).


## 3. The voice records at `0x400` — what `group_of()` was guessing, and what is confirmed

`soundboard.h` says: "VF3's sound driver keeps a record a voice, 32 bytes at 0x400 + slot * 0x20 in each
SCSP's RAM: the slot's register address, then the channel the voice plays for." That was a deduction. Here
is the program, in `snd_irq1`:

```
00600780  movem.l   d2/d6-d7/a1/a4,-(a7)
00600784  move.l    $c10.w,$c14.w
0060078A  move.w    #$3c6,$10041a.l      ; SCSP 0 +41A : the interrupt acknowledged/rearmed
00600792  move.w    #$80,$100422.l       ; SCSP 0 +422
0060079A  move.b    $c01.w,d7            ; a global flag in sound RAM
0060079E  andi.b    #$12,d7
006007A2  bne.b     $6007d2              ; already running -> exit
006007A4  ori.b     #$2,$c01.w
006007AA  clr.b     $c30.w
006007AE  lea.l     $400.w,a4            ; <<< THE RECORDS, chip 0
006007B2  moveq     #$1f,d7              ; 32 slots
006007B4  clr.l     $c10.w
006007B8  bsr.b     $6007de
006007BA  lea.l     $200400.l,a4         ; <<< THE RECORDS, chip 1 (0x400 of ITS RAM)
006007C0  moveq     #$1f,d7
006007C2  move.l    #$200000,$c10.w      ; c10 = the current chip's offset
006007CA  bsr.b     $6007de
...
0060084C  dbra      d7,$6007de           ; and the routine ends with
00600848  lea.l     $20(a4),a4           ; <<< A STEP OF 0x20
```

**Established:** the records are at `0x400` in each chip's RAM, **32 records of `0x20` bytes**, walked
through in the interrupt. The step of `0x20` and the count of 32 are in the code, not deduced.

**And a mistake of mine, corrected on the evening of 2026-10-24.** I had written that "the driver's globals
begin just above, at `0xC01` — that is `0x400 + 32 × 0x20 = 0xC00`, the geometry closes". **That is wrong:**
`32 × 0x20 = 0x400`, so the records occupy `0x400-0x7FF` and `0x400 + 0x400 = 0x800`, not `0xC00`. What is at
`0x800` is the MIDI command queue (§3 bis). My "the geometry closes" was an arithmetic coincidence on which
I built a conclusion — exactly the fault the repository's memory calls a cause with no measurement. The real
map, this one measured:

| | |
|---|---|
| `0x300-0x30F` | a master block; `tst.b $300` governs the sequencer's entry |
| `0x310-0x37F` | **7 sequencer tracks** of `0x10`, field `+0xF` = a group nibble (`0x6026AE`) |
| `0x380-0x3FF` | **8 entries** of `0x10`, initialised from a ROM table at `0x608024`, and their `+5` set to `0xFF` (`0x600422`) |
| **`0x400-0x7FF`** | **the 32 voice records of `0x20`** — `group_of()`'s table |
| `0x800-0xBFF` | **the MIDI command queue**, 4 bytes per entry (§3 bis) |
| `0xC00` and beyond | the globals: `0xC01` busy, `0xC10` the chip's offset, `0xC1B`-`0xC1D`, `0xC30`, `0xC3E`/`0xC3F`, `0xEC0`-`0xECD` the MIDI parser's state |
| `0x1000-0x13FF`, `0x201000` | **32 sample-copy descriptors** of `0x20`, walked through by A6 |
| `0x200` + chip offset | the sample loader's **undo log**: the old values, one long per entry (§5 bis sexies) |
| `0xF80-0xF9F` + chip offset | **one word per MIDI channel** — 16 channels, 32 bytes — indexed `channel*2` by `0x603172`; cleared when a voice stops (§5 bis septies). Contents not established. **Careful: the index is not that of the neighbouring table at `0xF00`**, which is per slot |
| `0x3000` + chip offset | the **sample records**, `0x0C` bytes per entry: `+0` address and bank, `+4` length, `+8` relocation accumulator (§5 bis sexies) |
| `0x0000-0x00FF` | the **live vector table**, copied from the ROM by `0x6003D0` — and the **stack** starts at `0x2000` and grows down below it (`lea $2000.w, a7` at `0x6006C4`) |
| `0x2000-0x2FFF` + chip offset | the **bank of sixteen single-cycle waveforms**, thirteen distinct, at a reserved address nothing overwrites; slots point into it through their `SA` for an **FM voice** (§4 septies) |
| `0x4EE8-0x706B` | the **instrument table in RAM** (`[0x608038]`, length `[0x608030]`), and `0x706C` is the samples' allocation base: the two touch **exactly** |

**The record's first long is the slot's register address**, confirmed by `0x6006DC`:

```
006006DC  movea.l   $0(a4),a1            ; a1 <- the long at +0 of the record
006006E0  move.b    #$1f,$b(a1)
006006E6  move.b    #$10,$0(a1)          ; writes INTO the slot's registers
```

So `group_of()`'s `r[0] != (n ? 0x30 : 0x10)` and `r[1] != slot * 0x20` test **that long's high word and low
word**: `0x00100000 + slot*0x20` for chip 0, `0x00300000 + slot*0x20` for chip 1. Both tests are **right**,
and they are what the program writes.

## 3 bis. The `A0 <bank> <index>` protocol, followed from end to end

Read on the evening of 2026-10-24, and it is entirely standard MIDI:

1. **`snd_irq3` (`0x6005BE`) is the MIDI reception.** It reads the byte at register `+0x405`, **re-emits** it
   to `+0x407` if `0xC3F` is set (a MIDI thru), then parses it: bit 7 set = status byte, otherwise data. The
   count of data bytes expected is set to **2, or 1 if the status is in `0xC0..0xDF`** — the exact MIDI rule
   (Program Change and Channel Pressure take one datum, the others two).
2. **The complete message is queued at `0x800`**, 4 bytes per entry, write index `0xEC4`, mask `0x3FF`. And
   `cmpi.w #$A000` (`0x60063C`) — **the comparison is on a WORD**, status and first datum together, which
   explains why a search for `cmpi.b #$A0` finds nothing: `A0 00 01/02/03` are control commands that clear
   the tracks `0x300`-`0x370`.
3. **The dispatcher is `0x600852`**, called by the main loop (`0x601CB8`) when `0xECC` is not zero. It takes
   `D5 = status & 0x0F` — **the MIDI channel** — and jumps through a table at `0x60088E`, eight `bra.w`
   indexed by the high nibble:

   | status | | | status | |
   |---|---|---|---|---|
   | `8n` note off | `0x60120A` | | `Cn` program change | `0x601C1A` |
   | `9n` note on | `0x6008AC` | | `Dn` | `rts`, ignored |
   | **`An` the game's protocol** | **`0x6025F2`** | | `En` pitch bend | `0x6012D6` |
   | `Bn` control change | `0x601402` | | `Fn` | `rts`, ignored |

4. **`0x6025F2` does the bank/index lookup**: the bank is **offset by `0x10`** (hence `0x10` for the music),
   rejected if `>= 0x60` or `< 0x10`; a two-level table from the pointer `0x60801C`, bounded by a count at
   each level, `0xFFFF` marking an empty bank. Then **`tst.b d5`** (`0x60264A`): channel 0 → the music and
   the sequencer at `0x300`; non-zero channel → `0x602694`.

**And the third test is confirmed too, on the evening of 2026-10-24.** It had to be chased from the other
end: not "who sets a channel" but "who writes the byte at `+5`". A single instruction does it on the
records' register (`0x6010F2`):

```
006010EA  move.w  a0, d0
006010EC  subi.w  #$c40, d0        ; A0 - 0xC40
006010F0  lsr.w   #$5, d0          ; / 0x20
006010F2  move.b  d0, $5(a4)       ; -> the record's byte at +5
```

**The byte at `+5` is not a MIDI channel: it is the index of an entry in a table of `0x20`-byte structures
based at `0xC40`.** That is why it can go above 15, which a MIDI status nibble cannot.

And the threshold's `0x0F` is not a magic number, it is a **consequence of the layout**. `0x600430` builds
that table from a ROM table (the pointer `0x608014` → `0x608338`, then `+0x24` → `0x60835C`), and it places
it **right-aligned so as to end at `0xEC0`**:

```
0060044E  move.w  #$280, d6
00600452  sub.w   d7, d6           ; 0x280 - count * 0x20
00600454  adda.l  d6, a2          ; a2 = 0xC40 + that remainder
```

`0x280 / 0x20` = room for **20 entries**, and `0xEC0` is precisely where the MIDI parser's state begins. The
ROM table declares **5** of them. So they occupy `0xE20`-`0xEBF`, and their indices are **`0x0F` to
`0x13`** — the fifteen lower places, `0x00` to `0x0E`, being left to the music.

| | |
|---|---|
| `group_of()`: `r[0] == 0x10/0x30` | ✔ confirmed by the write (`0x6004E0`) |
| `group_of()`: `r[1] == slot * 0x20` | ✔ confirmed by the same |
| `group_of()`: `(byte +5) >= 0x0F` | ✔ **confirmed**: the effects' five channel objects are `0x0F`-`0x13` |

**A correction in passing.** `soundboard.h` writes "channels 0x0F to 0x12": the measurement gives **five**,
`0x0F` to `0x13`. The port's test being a `>=`, it catches them all and nothing was broken — but the comment
said four where there are five, and it is corrected.

The five entries, as the ROM gives them (16 bytes each, the rest zeroed):

```
indice 0x0F : 83 0b 00 df 00 00 60 20 4e 10 df 00 08 00 00 00
indice 0x10 : a3 0c 00 eb 00 00 60 20 4e 10 eb 00 04 00 00 00
indice 0x11 : a3 0d 00 eb 00 00 60 20 4e 10 eb 00 04 00 00 00
indice 0x12 : a3 0e 00 ed 00 00 60 20 4e 10 ed 00 08 00 00 00
indice 0x13 : a3 0f 00 db 00 00 60 20 4e 10 db 00 04 00 00 00
```

The first byte is a MIDI status (`0x83`, then `0xA3` four times) and the second runs from `0x0B` to `0x0F`:
not read any further, and noted as it is rather than guessed at.

**What remains open on this point:** `driver_known` keeps all its point. Nothing above says that ANOTHER
game arranges its records this way — these are that driver's tables, and the guard remains the right answer
for a third game.


## 3 ter. The `FF` does not stop the sound: it restarts the board

The repository measured this trap in August and worked around it without explaining it.
`docs/fv2-console-sound.md` puts it this way: "the reset's trap: `FF` **restarts** a sound still in progress
before cutting it", and "the stop delay: 80 frames, so that the reset survives the opening beep". Here is
what `FF` really does, read in `snd_irq3_reception_midi`:

```
006005F2  cmpi.b  #$ff, d0
006005F6  beq.w   $6006c4
...
006006C4  lea.l   $2000.w, a7           ; the stack put back to its reset value
006006C8  move.l  #$4572726f, $c08.w    ; "Erro"
006006D0  move.l  #$72536574, $c0c.w    ; "rSet"
006006D8  bra.w   $600120               ; a jump into snd_reset
```

**`FF` is a software restart of the sound board.** It puts the stack back to `0x2000` (the reset vector's
value), writes the signature `"ErrorSet"` at `0xC08`, and branches into the reset code. It is not a stop
command: it is the MIDI system reset taken at its word.

And `snd_reset` reads that signature back, which closes the loop:

```
00600196  cmpi.l  #$4572726f, $c08.w
006001A0  cmpi.l  #$72536574, $c0c.w
006001AA  move.b  $c04.w, d4            ; the byte kept across the clearing
006001AE  lea.l   $0.w, a0              ; then sound RAM is cleared
```

So the marker tells a **warm restart** from a cold start, and a single byte (`0xC04`) survives the clearing
of sound RAM — the rest is made new: registers, MVOL, RBL/RBP, the 32 records, the track tables, everything.

**What that explains.** A sound still in progress is heard again because the board **reinitialises its chips
while voices are still running**: the reset puts MVOL back to `0xF`, rewrites the common registers and
sweeps the slots, and what was decaying goes through there again. It is neither a bug in the port nor a whim
of the chip: it is a reboot.

**And what to deduce from it cautiously.** The 80 frames of delay the repository found by trial are very
probably explained by the **copying of the samples**: the restart clears sound RAM, and the copy engine
(§ the main loop) brings the samples back up from the ROM **512 bytes per pass**. Nothing more can play
before that. **It is a deduction, not a measurement** — nobody has timed the copying.

**The practical consequence for this port**: `FF` is not a cheap stop. Everything that sends it restarts the
sound board, and the port does send it (it is its stop lever, `docs/fv2-console-sound.md`). The question
"should the game be frozen under the mode list" asked in that document reads differently once one knows
that.


## 3 quater. The bridge: how a MIDI channel becomes a voice, and on which chip

That was the hole named above. It reads in the **note on** (`9n` → `0x6008AC`):

```
006008BC  lea.l   $c40.w, a0        ; the table of channel objects
006008C0  moveq   #$13, d7          ; TWENTY entries (dbra -> 20 rounds)
006008C2  tst.b   $0(a0)            ; is the object in service ?
006008C8  cmp.b   $1(a0), d5        ; its byte +1 == the message's MIDI channel ?
006008DC  btst.b  #$5, $0(a0)       ; bit 5 of +0 :
006008E4  move.l  #$200000, $c10.w  ; ... then it is the SECOND chip
006008EC  bsr.b   $600912           ; play the note on that chip
006008F8  lea.l   $20(a0), a0       ; a step of 0x20
```

Three things at once:

1. **The table at `0xC40` does have 20 entries of `0x20`** — `moveq #$13,d7` says so, and it is the same 20
   as section 3's `0x280 / 0x20`. Two paths, the same count.
2. **An object's byte `+1` is the MIDI channel it listens to**, and a note on plays on **every** object whose
   `+1` matches — so one MIDI message can start voices on both chips.
3. **Bit 5 of byte `+0` picks the chip**: set, `[0xC10]` goes to `0x200000`, and everything that follows
   works on the second SCSP.

Reread with that semantics, the five effects objects the ROM declares:

| index (a record's byte `+5`) | `+0` | bit 5 | chip | `+1` = MIDI channel |
|---|---|---|---|---|
| `0x0F` | `0x83` | 0 | **0** | `0x0B` |
| `0x10` | `0xA3` | 1 | **1** | `0x0C` |
| `0x11` | `0xA3` | 1 | **1** | `0x0D` |
| `0x12` | `0xA3` | 1 | **1** | `0x0E` |
| `0x13` | `0xA3` | 1 | **1** | `0x0F` |

**And that confirms `soundboard.h`'s last assertion** — "the game's effects, voices and announcer ask for
[...] **on the second SCSP**": four of the five are there, through bit 5 of their byte `+0`. So the complete
chain reads from end to end:

```
the game sends  A<channel> <bank> <index>      channel 0 = music, 0x0B-0x0F = effects and voices
  -> snd_irq3_reception_midi, queued at 0x800
  -> snd_repartir_commande_midi, D5 = the channel
  -> the channel object at 0xC40 whose +1 == D5   indices 0x0F-0x13 for the effects
  -> its bit 5 picks the chip                     four of the five on the second
  -> the voice record at 0x400 + slot*0x20
       its +5 = that object's index               (A0 - 0xC40) / 0x20
  -> SoundBoard::group_of() reads that +5 and thresholds at 0x0F
```

The port's three tests are right, and the threshold `0x0F` is the boundary between the music's fifteen
objects and the effects' five, laid down by the table's layout and not chosen.


## 3 quinquies. The long sounds are STREAMED, and the port depends on it

A new question, taken up because all the previous ones were closed — and this one touches the port.

The main loop is the sample copy engine (A6 walks through 32 descriptors of `0x20` at `0x1000`, `0x201000`
for the second chip). Two things leap out there:

```
00601CA4  move.b    (0x3,A6), (0x00100408).l   ; the descriptor says WHICH SLOT to watch
...
00601CF8  tst.b     $100409.l
00601CFE  bpl.b     $601ca4                    ; bit clear -> round again
00601D00  movea.l   $10(a6), a2                ; destination A
...
00601D22  tst.b     $100409.l
00601D28  bmi.w     $601ca4                    ; bit SET -> round again
00601D34  movea.l   $8(a6), a2                 ; destination B
```

**Two destinations in the descriptor (`+0x08` and `+0x10`), chosen by the opposite polarity of the same
bit.** That is a double buffer, with no ambiguity.

And the bit is **`CA`**. The byte `+0x409` is the low half of the word `0x408`, which the port fills with
`(CA << 7) | (SGC << 5) | EG` — so its bit 7 is **bit 0 of `CA`**. And `CA = (cur_addr >> (SHIFT + 12))
& 0xF` with `SHIFT = 12`, that is the sample index shifted by 12: **`CA` counts in 4096 samples**, and its
bit 0 flips every **93 ms** at 44.1 kHz.

`0x6004F0` completes the picture: it sets each descriptor's byte `+3` to `8, 16, 24…`, that is
`(slot + 1) << 3` — and writes into the high byte of the word `0x408`, which is to say **`MSLC`**. So the
descriptor says which slot the chip should report on, and the loop reads where it has got to.

**What that establishes:** the driver **streams** the long sounds into the SCSP's RAM, in halves of 4096
samples, asking the read position which one is free. It does not load them whole.

**The consequence for this port, and it is direct:** `CA` is **load-bearing**. If the port made it wrong or
frozen, the copy engine would spin on `0x601CA4` or write into the half being listened to — a stutter or a
crackle on the long sounds, that is to say the music. The port computes it on the fly from `cur_addr`, and
`VF3_SCSP_MSLC_LATCH=1` exists to compare with the version latched once per sample (`scsp.h`) — we now know
**what** that variable acts on, and why it is not harmless.

**And it refines section 3 ter's deduction**: the 80 frames of delay after an `FF` are not "copy all the
samples back", they are **priming the streaming buffers** — nothing can play before one half is filled.
Still a deduction, but better framed.


## 4. The `+0x600`: a question I put badly, and that the measurement closes

**This section said something false, and here it is corrected.** I had written, on the afternoon of
2026-10-24: "the driver **accumulates**, per slot and for both chips, into the register window at `+0x600`
[...] 32 times per chip, on every interrupt". That was read in the code and **never measured**.

The instruction does exist, in `snd_tour_de_fente`:

```
006007F0  tst.b   $4(a4)              ; bit 7 of the record's +4
006007F4  bpl.b   $600848             ; if clear, on to the next record
...
0060083A  lea.l   $100600.l, a1
00600840  adda.l  $c10.w, a1          ; 0x100600 or 0x300600
00600844  add.w   d2, (a1, d6.w)      ; accumulates at +0x600 + slot*2
```

But it is **guarded by bit 7 of the record's byte `+4`**, and that guard never opened. Measured by widening
the `VF3_SCSP_DSPLOG` probe to the whole register window:

| | writes into `0x600-0x6FF` |
|---|---|
| Fighting Vipers 2, `fv2-fight.txt`, 4200 frames, 35 096 writes logged | **0** |
| Virtua Fighter 3, `vf3-match.txt`, 2400 frames, 15 417 writes | **0** |

Not one, on either game. The probe does see both chips and the small offsets — it logs besides 1726 and 715
writes of the slots' word `0x6`, at offsets `0x00C`, `0x02C`, `0x04C`…

**And on 2026-10-25, a positive control on the branch itself**, because a mute probe and a probe that says
"nothing" are indistinguishable in the output. The `RINGBUF` branch was temporarily pointed at
`0x400-0x4FF`, a range written for certain (the common registers): **786 179 lines**. So the condition, the
label and the `fprintf` work, and the branch is reachable. The zero is a zero.

This document's other zeros carry their own control **in the same line**: `dspprog` announces
`frcl 0 adrl 0 yrl 0 negb 0` next to `iwt 23 twt 39 mrd 23 mwt 10 ewt 5`, counted by the same code at the
same moment — counters that speak for four neighbours that keep quiet.

### And the zero was not general: it depends on the arena

The same 2026-10-25, forcing the arena (`VF3_STAGE`, see §4 sexies) instead of letting the game choose its
own, the same probe over the same 4 200-frame scene gives:

| arena | writes into `0x600-0x6FF` |
|---|---|
| the one the game chooses by itself | **0** — the measurement above |
| **arena 2** | **993**, from frame 3783 on |
| the eleven others (1, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12) | **0** |

So the zero was true, and it did not mean what I drew from it. **It is not a guard that never opens: it is a
guard that one arena in twelve opens.** The offsets touched confirm it — `0x600` to `0x63E` and `0x640` to
`0x67E`, that is to say exactly `0x600 + slot × 2` and `0x640 + slot × 2`, the two ranges `0x60153A` writes
(§5 bis quaterdecies). The code I read is indeed the code that executes.

**And `STWINH`'s question is settled at the same stroke.** That was the port's worry: it routes `+0x600` to
MAME's FM buffer, which the audio path rewrites on every sample **unless `STWINH` protects the entry**. By
following `STWINH`'s state slot by slot in the ordered log, and reading it at the moment of each write:

```
993 writes into the FM buffer :
  STWINH set   (the entry is protected) : 993  100.0 %
  STWINH clear (the sound rewrites it)  :   0    0.0 %
  state unknown (never saw a SLOT6)     :   0
```

**The driver protects the entry every time, without one exception in 993.** So the software modulation
reaches the hardware intact, and the port's routing is correct on that point. That was this section's other
open question, and it is closed.

The lesson, which has been this document's from the start: a zero measured with a positive control is a real
zero, but it only bears on **what was run**. It had taken me an hour, the same day, to write "the feature
exists, these scenes do not use it" — that was right, and changing arena was all it took to find a scene
that does.

**Consequence: the worry I had raised is beside the point.** The port does route `0x600-0x6FF` to MAME's FM
ring buffer, and the audio path does overwrite it on every sample — but **there is nothing there to
overwrite**, on those scenes. The code path exists and is not taken.

**And what the driver really does with `STWINH`**, measured at the same stroke: it **sets** it, in 890 of
the 1726 slot programmings on Fighting Vipers 2 and 423 of the 715 on Virtua Fighter 3, on every slot. So it
uses it for what it is for — deciding which slots feed the ring buffer — and not to protect something it has
just written there, which is what I had put forward as a hypothesis.

**The lesson, and it is the second time in the day.** I drew a frequency ("32 times per chip, on every
interrupt") from a reading of the code, without measuring it, and I wrote it as a fact. The measurement says
zero. The repository's rule — read the code to know **what** to look for, measure to know **what happens** —
holds too, and above all, for what one thinks one has understood by reading.

**What remains open**, reduced to its real size: another scene could open that guard. The question is no
longer "is the port losing writes" but "is that path ever used", and it closes with a single run with the
widened probe as soon as a scene is suspected of taking it.


## 4 bis. The three other things, and what the `0xF00` table really is

### The `0xF00` table: a software modulation, written into the FM buffer

It is section 4's **explanation**, and not merely its correction. The per-slot tick reads a table of **4
bytes per slot** at `0xF00` (indexed by `slot*4`, obtained from the record's `+2` shifted by 3):

```
00600806  subq.b  #$1, (a1, d6.w)       ; +0 : a countdown
0060080A  bne.b   $600832               ; not expired -> straight to +1
0060080C  move.b  $2(a1, d6.w), d2      ; +2 : the reload
00600810  bne.b   $60081a
00600812  bclr.b  #$7, $4(a4)           ; +2 zero -> END : bit 7 of the record's +4 falls
0060081A  move.b  d2, (a1, d6.w)        ; reload +0
0060081E  move.b  $3(a1, d6.w), d2      ; +3 : a second value
00600822  beq.b   $60082e
00600824  move.b  d2, $1(a1, d6.w)      ; +3 non-zero -> +1 = +3, a one-shot
00600828  clr.b   $2(a1, d6.w)
0060082E  neg.b   $1(a1, d6.w)          ; +3 zero -> THE SIGN OF +1 IS FLIPPED
00600832  move.b  $1(a1, d6.w), d2
00600838  lsr.b   #$1, d6               ; slot*2
0060083A  lea.l   $100600.l, a1
00600844  add.w   d2, (a1, d6.w)        ; accumulates into the FM buffer
```

| byte | |
|---|---|
| `+0` | a countdown, decremented on every IRQ1 tick |
| `+1` | a **signed** value, accumulated into `0x600 + slot*2` |
| `+2` | the reload for `+0`; **zero, it ends the effect** |
| `+3` | a second value, which replaces `+1` once and then clears itself |

So: a value whose **sign flips every N ticks** is accumulated into the FM ring buffer, at the slot's place.
And a slot whose `MDXSL`/`MDYSL` points there reads it **as a modulation source**.

**That is a vibrato — or a pitch sweep — driven by software, using the chip's FM buffer as a modulation
channel.** That is what section 4's `+0x600` is, and why the port, which puts MAME's ring buffer there and
overwrites it on every sample, would lose it **if the feature were used**. It is not used in the two scenes
measured: bit 7 of a record's `+4`, which commands it, never gets set.

### IRQ2's two other things: a fade down and a fade up

`snd_irq2` calls three helpers. The third, `0x603460`, is the sequence stream interpreter (§3 bis: the
variable-length quantities and the *End of Track*). The first two are **fades**:

| | armed by | step | |
|---|---|---|---|
| `0x6033C8` | `[0xEEB]` | `[0xEED] -= 4` | **fade down**; once below zero it sends `A0 00 02`, which stops the master track |
| `0x603408` | `[0xEF2]` | `[0xEED] += 4` | **fade up**, up to `[0xEEE]` |

Both build `d1 = 0xA0010000 | (level << 8)` — that is the command **`A0 01 <level>`** — and branch to
`0x6036FC`, which **posts it into the same queue at `0x800`** that the MIDI receiver fills, advancing both
indices:

```
006036FC  lea.l   $800.w, a1
00603702  move.w  $ec4.w, d0          ; the write index
0060370C  move.l  d1, (a1)            ; the command, 4 bytes
0060370E  addq.w  #$4, $ec4.w
00603712  addq.b  #$1, $ecc.w         ; the count of pending commands
```

**So the fades talk to themselves through the public command path**, instead of touching the volume directly.
And since bank `0x01` is below `0x10`, `0x6025F2` sends it to its control-command jump table (`0x602722`,
`jmp $602730(pc,d2.w)`): banks `0x00`-`0x0F` are **orders**, and banks `0x10` and above are **sounds**.
Section 3 bis said "bank offset by `0x10`"; here is why.

### Channel 0's music path

```
0060264E  tst.b   $c3e.w
00602654  move.b  #$f1, $100407.l     ; if armed, a 0xF1 is emitted over MIDI (a sync mark)
00602662  lea.l   $300.w, a4          ; THE MASTER TRACK
00602666  bsr.w   $602712             ; its 0x10 bytes cleared
0060266C  move.b  (a2)+, d0
0060266E  andi.b  #$60, d0
00602672  or.b    d0, $0(a4)          ; two flag bits taken from the sound's header
00602676  clr.b   $eeb.w              ; A FADE DOWN IN PROGRESS IS CANCELLED
0060267A  move.l  (a2)+, $4(a4)
0060267E  move.l  a2, $8(a4)          ; +8 = THE STREAM POINTER of the sequence
00602682  move.w  #$1, $2(a4)         ; +2 = 1, the first delay
```

So a track of `0x10` bytes carries: `+0` flags (bit 7 = active), `+2` the current delay, `+4` a long taken
from the sound's header, **`+8` the pointer into the sequence stream** — the one `0x603460` reads with A3 —
and `+0xF` the group nibble the allocator compares (§3).

And `0x602676`'s `clr.b $eeb.w` **cross-checks** the reading of the fades: `0xEEB` is indeed the fade-down's
arming flag, since starting a piece of music cancels it. Two independent paths, the same conclusion.


## 4 ter. The instrument engine, and the `+6` that could have fooled the port

`0x600912`, called by the note on once the channel object is found, is the engine's core. It first exits on
the **note off** path if the velocity `[0xEF8]` is zero (`0x601256`) — the MIDI convention.

### Two more ROM tables, and two more bytes of the channel object

```
00600932  movea.l $608028.l, a1       ; the VELOCITY CURVES
00600938  move.b  $b(a0), d1          ; the channel object's byte +0xB picks the curve
0060093E  move.w  $2(a1, d1.w), d1
00600944  move.b  (a1, d0.w), $ecd.w  ; the velocity put through the curve -> [0xECD]
0060094A  move.b  $2(a0), d0          ; the object's byte +2 picks the INSTRUMENT
0060094E  movea.l $608038.l, a2       ; the instrument table
00600956  move.w  $2(a2, d0.w), d0
0060095A  bmi.w   $601102             ; negative = no instrument, give up
```

| the channel object at `0xC40` | |
|---|---|
| `+0` | flags; **bit 5 = the chip** (§3 quater) |
| `+1` | the MIDI channel listened to |
| `+2` | **the instrument**, index into the table `0x608038` |
| `+0xB` | **the velocity curve**, index into the table `0x608028` |

The mapped velocity lands in `[0xECD]`, which `0x600C6A` copies into the voice record's `+0xA`.

The instrument is a list of **key regions**: two forms depending on its byte `+1`, one with a 15-bit pitch
comparison (`note*128 + fine`, entries of 12 bytes), the other on a plain byte interval. A multisample, then,
with key splits.

### The record's `+6` and `+9`, and note exclusivity

```
006009C0  lea.l   $400.w, a4          ; the voice records
006009C8  moveq   #$1f, d7            ; all 32
006009CA  cmp.b   $6(a4), d5          ; the record's +6 == THE MIDI CHANNEL
006009D0  cmp.b   $9(a4), d0          ; the +9 == an EXCLUSIVITY GROUP
```

The **instrument**'s byte `+0xB` carries that group, and its sign picks the behaviour:

| the instrument's `+0xB` | if a record has the same channel and the same group |
|---|---|
| zero | the search does not happen — **polyphonic** |
| negative (bit 7) | `bra 0x601102`: **the new note is dropped** (no re-attack) |
| positive | `bsr snd_fente_terminee`: **the old voice is cut and replaced** |
| `0x7F` | a case apart, `0x600A12` |

### And here is what could have fooled the port

**The voice record has TWO channel fields, side by side:**

| | |
|---|---|
| `+5` | **the channel object's index** — `0x00`-`0x0E` music, `0x0F`-`0x13` effects (§3) |
| `+6` | **the MIDI channel** — `0x00`-`0x0F` |

`SoundBoard::group_of()` reads the `+5`, and that is the right one. **Had it read the neighbouring `+6`**, its
test `>= 0x0F` would have caught only MIDI channel `0x0F` — and the four other effects objects, which listen
to channels `0x0B` through `0x0E`, would have been classed **with the music**. Four of five misfiled, by one
byte of offset, and the symptom would have been "the effects volume barely acts" — hard to attribute.

Nothing to fix: the port had it right. But it is noted, because a neighbouring byte that looks like the one
you are after is exactly the kind of trap where a deduction verifies badly.


## 4 quater. The voice allocator, the polyphony ceilings, and what separates the two games

### A priority voice steal, bounded by three constants in ROM

The rest of the note on's path, when exclusivity found nothing (`0x600A42`):

```
00600A42  btst.b #$0, $0(a0)         ; bit 0 of the channel object : is allocation permitted
00600A52  move.b $ee3(a3), d0        ; a counter of active voices, PER CHIP (a3 = the offset)
00600A56  cmp.b  $60807e.l, d0       ; against a ceiling in ROM
00600A5E  add.b  $ee2(a3), d0        ; plus a second counter
00600A62  cmp.b  $60807d.l, d0       ; against a second ceiling
00600A7E  cmp.b  $ee1(a3), d0        ; a third, against 0x60807C
00600A86  lea.l  $400.w, a4          ; otherwise : THE SEARCH FOR THE VICTIM, over the 32 records
00600AA8  tst.b  $b(a4) ; bpl        ; the record's +0xB, bit 7 = in service
00600ABA  cmp.b  $d(a4), d0          ; the +0xD = a PRIORITY ; the lowest is kept
00600ACA  movea.l a4, a1             ; the candidate
```

Three **counters of active voices per chip** at `0xEE1`, `0xEE2`, `0xEE3`, against three **ceilings in ROM**
at `0x60807C`, `0x60807D`, `0x60807E`. And `snd_fente_terminee` decrements them (`subq.b #$1, $ee0(a1)`,
`$ee1`, `$ee2`): the loop is closed. When everything is full, the record of **lowest priority** (`+0xD`) is
stolen.

### And the ceilings are not the same from one game to the other

| | Fighting Vipers 2 | Virtua Fighter 3 |
|---|---|---|
| `0x60807C` / `0x60807D` / `0x60807E` | **`0x20` = 32** | **`0x10` = 16** |
| the byte OR-ed into the bank register (`0x608068`) | `0xE0` | `0x20` |

**Fighting Vipers 2 allows itself a chip's 32 slots, Virtua Fighter 3 limits itself to 16.** The mechanism is
the same, the setting is not. Nothing in the port depends on it — it is the driver imposing the limit on
itself — but it is a real difference in behaviour between the two games it serves.

And the bank byte: the driver ORs `0xE0` (FV2) or `0x20` (VF3) into what it writes at `0x400001`. The port
reads only **bit 4** of that register (`soundboard.h`: "bit 4: the samples' upper 8 MB"); bits 5 to 7 are
therefore filed into `b.ctrl` and ignored. They are probably other control lines of the board. **Nothing says
the port is missing them** — nothing needs them — but it is noted.

### The nuance that brings to `driver_known`

The two sound ROMs are only **28.9 %** identical (151 272 bytes out of 524 288; 33 pages of 4 KB out of 128).
And the identical pages are **not** the code's: the code lives in pages 0 to 3, and all four differ.

Yet everything this document establishes holds for both: the same SSP and the same reset PC, the same vector
table, the same tables at `0x300`/`0x380`/`0x400`/`0x800`/`0xC40`, the same twenty channel objects, the same
threshold, and **three DSP programs identical to the byte**.

So: **the same driver, built and tuned differently for each game** — not the same binary. That is exactly
what justifies `soundboard.h`'s `driver_known` guard rail: not that another game would have an unrelated
driver, but that *this one* recompiles per game, and that a third could move what this document has nailed
down.


## 4 quinquies. The slot's programming, and three loops that close

`0x600B96` is the point where all the note on's paths converge, and the first instruction says it all:

```
00600B96  movea.l $c10.w, a3
00600B9C  movea.l $0(a4), a5       ; <<<< A5 = THE SLOT'S REGISTERS, taken from the record's +0
```

**That is the very pointer `group_of()` validates.** The port checks that a record's first long equals
`0x00100000 + slot*0x20`; the driver, for its part, **dereferences** it to write into the slot. So the port's
test is not a heuristic on a value that looks right: it is exactly the use the driver makes of it.

### A voice's two levels, and where they go

```
00600BD4  move.b  $5(a2), d0       ; the +5 of the instrument REGION
00600BDA  andi.b  #$7f, d3
00600BDE  bne.b   $600be4
00600BE0  move.b  $c(a0), d0       ; zero -> fall back on the CHANNEL OBJECT's +0xC
00600BE8  move.b  d0, $15(a5)      ; -> the slot's byte 0x15
```

A slot's byte `0x15` is the low half of the word `0xA`, that is **`ISEL` and `IMXL`** — *which DSP input and
at what level*. So **the send to the reverberation is a property of the instrument**, with a per-channel
default. It is the tap whose fate the whole MADRS question of section 5 decides.

And `0x601108`, called just before with the region's `+4` (or the object's `+6`), computes the other level:

```
0060110A  andi.b  #$e0, d0 ; lsr.b #$5, d0   ; les 3 bits de poids fort -> 0..7
00601110  add.b   $c04.w, d0                 ; + A GLOBAL TRIM
00601116  clr.b   d0                         ; clamped below
00601120  moveq   #$7, d0                    ; and above
00601122  lsl.b   #$5, d0
00601124  andi.b  #$1f, d3 ; or.b d3, d0     ; recombined : level(3) : pan(5)
```

`level(3 bits) : pan(5 bits)` is **exactly** the layout of the high byte of a slot's word `0xB` — `DISDL` at
bits 13-15 and `DIPAN` at bits 8-12. So it is the **direct output** that is computed there.

### Three loops that close, and that is what gives confidence

1. **`[0xC04]`, the only byte the `FF` preserves** (§3 ter: `snd_reset` saves it in D4 across the wiping of
   the RAM), is **a global level trim clamped to 0-7, added to every voice's `DISDL`**. The one thing worth
   carrying across a restart is the overall volume. Two separate readings, two hours apart, that meet.
2. **`[0xEFB]`** is consulted here (`0x60112A`) **and** by the DSP preset loader (§3 bis: a forced `0xE0`,
   that is `EFSDL = 7`). It is **the global reverberation switch**, read from both sides — a voice's send and
   the DSP's return. Two independent paths, the same flag.
3. **The voice counters**: `0x600BFE` puts `[0xEE1]` into the record's `+0xB`, sets its bit 7, **and
   increments `[0xEE1]`**; `0x600C1A` increments `[0xEE3]`. These are the counters the allocator (§4 quater)
   compares with the ROM ceilings and `snd_fente_terminee` decrements. The loop is closed in all three
   directions.

And the record's fields cross-check too: the `+0xD` (priority, which the allocator reads to choose its
victim) comes from the region's `+0xA`, and the `+9` (exclusivity group, which §4 ter's search compares) from
the region's `+0xB`. What the allocator reads is what the programming writes.


## 4 septies. The 4 KB at `0x2000`: sixteen waveforms, and the RAM map that closes on it

A point from §5 ter, taken up on 2026-10-25. `0x600524` copies 4 096 bytes of ROM (`0x603FE2`) to `0x2000` in
**each of the two sound RAMs**, in 512 rounds of two `move.l`. The content was noted as "unidentified".

### It is a bank of sixteen waveforms

The 2 048 words read as signed 16-bit PCM, and sort themselves into windows of 128 words:

| address | what it is |
|---|---|
| `0x2000` | **a sine** — one exact period of 128 words, amplitude 32 767, **maximum deviation 5** against `−32767·sin(2πk/128)` |
| `0x2200`, `0x2B00` | two full-scale **squares** |
| `0x2C00` | a **constant**, 32 704 over the 128 words |
| `0x2300`, `0x2400`, `0x2500`, `0x2600`, `0x2800`, `0x2A00`, `0x2D00`, `0x2E00`, `0x2F00` | **smooth** single-cycle shapes (mean step of 500 to 1 800 over a range of 65 000, so continuous) |
| `0x2100`, `0x2700`, `0x2900` | **irregular, with large steps** (18 000 to 21 000 mean step): noise |

**Sixteen slots of 256 bytes, thirteen distinct shapes**: `591bf29e` occupies `0x2300`, `0x2A00` and
`0x2F00`, and `8dcc2bdc` occupies `0x2600` and `0x2D00`. The smooth shapes join up well end to end —
`|w[0] − w[127]|` is 0, 1 or ~1 600, that is one step of a sine — and the squares jump by 65 281 at the join,
which is what a square must do.

So it is **a bank of single-cycle oscillators**, which is what a driver needs for synthetic voices, since the
SCSP can only play PCM taken from its RAM.

### And the RAM map closes on it

The longs of the constants area, read at the same place:

| | |
|---|---|
| `[0x608038]` | `0x4EE8` — the instrument table **in RAM** |
| `[0x608030]` | `0x2184` — its length |
| `[0x608034]` | `0x706C` — the **allocation base** for loaded samples |

And `0x4EE8 + 0x2184 = 0x706C`, **exactly**. The instrument table ends where sample allocation begins,
without a byte of slack. So the sound RAM reads as follows, from the bottom up:

```
0x0000-0x00FF  the live vector table, copied from ROM by 0x6003D0
0x0100-0x1FFF  ... and the STACK, which starts at 0x2000 and descends (lea $2000.w, a7 at 0x6006C4)
0x2000-0x2FFF  THE BANK OF SIXTEEN WAVEFORMS
0x3000-...     the sample records, copied from [0x608000] by 0x600556
0x4EE8-0x706B  the instrument table, copied from [0x60802C] by 0x600592
0x706C-...     the loaded samples, stacked upwards by 0x60288A
```

So the bank is at a **reserved** address: the sample allocator starts at `0x706C` and climbs, it never
touches it. It is a permanent region, and that is consistent with data the slots play.

### What makes it play: measured, and it is an FM voice

Two static searches had turned up nothing — no instruction names an address from `0x2001` to `0x2FFF`, and
none of the 659 sample records points there. One testable hypothesis was left: **it is the slots that play
it**, through their `SA` register. That cannot be read, it has to be measured.

So the `VF3_SCSP_DSPLOG` probe was extended to a slot's words `0x0` and `0x2` — `SA` holds its bits 19-16 in
the low nibble of the first and its bits 15-0 in the second. Two runs of 4 200 frames over `fv2-fight.txt`,
arenas 2 and 7:

| | arena 2 | arena 7 |
|---|---|---|
| `SLOTSA` lines | 6 641 | 6 151 |
| complete `SA` addresses | 1 013, **21 distinct** | 940, 25 distinct |
| **falling inside `0x2000-0x2FFF`** | **37** | **0** |

All 37 point at `SA = 0x02800`, that is **slot 8 of the bank** — one of the sixteen shapes only, all on
**chip 0**, and from frame **3783** to 4191.

**And 3783 is exactly the frame where the 993 writes to the FM buffer begin** (§4, end). Same arena, same
chip, same frame. The two things I took for two questions are only one.

### The complete chain, and what each link owes to what

```
0x60153A  move.b $a(a1), $e(a5)     ; the instrument's +0x0A -> a slot's register 0x0E
          move.w $c(a1), (0x600 + slot x 2)
          move.w $e(a1), (0x640 + slot x 2)
```

Register `0x0E` is, according to the port's implementation (`scsp.cpp`: `data[0x7]`), **`MDL` bits 15-12,
`MDXSL` bits 11-6, `MDYSL` bits 5-0** — the FM modulation level and the two ring-buffer inputs that feed it.
So `0x60153A` **arms FM on the slot and supplies the modulation itself**, into the two inputs the slot will
read.

What holds the whole together:

| link | where it comes from |
|---|---|
| the carrier waveform | the bank at `0x2000`, slot 8, through `SA` — **measured**, 37 slots |
| FM armed on the slot | the instrument's `+0x0A` into register `0x0E` — **read** at `0x60153A` |
| the modulation values | `0x600 + slot × 2` and `0x640 + slot × 2` — **measured**, 993 writes |
| they survive the audio path | `STWINH` set **993 times out of 993** — measured (§4) |
| and all of it in one arena out of twelve | arena 2 — measured on both sides |

**So it is an FM voice**, whose modulation the driver writes rather than letting the slot modulate itself on
the audio going by. And the port already knew it for the other game: `scsp.cpp`'s comment around line 1095
notes that "Pai's stage theme has an FM voice (the second SCSP, `MDL 0xF`, `MDXSL 0x20`: its own last sample)
from 9.4 s on", a depth tuned **by ear** against Supermodel. The two cases differ on one point: at Pai's the
slot modulates on itself, here the driver supplies the values.

### What it leaves open

- **Fifteen of the sixteen shapes are unused** in the two arenas measured. They are used elsewhere, or not
  at all.
- **Why slot 8** and not another: what chooses the `SA` is not read. It is not a sample record, since none
  points there; so it is a table or a computation I have not found.
- And the FM's depth in the port remains **tuned by ear**, not measured on hardware: `scsp.cpp`'s comment
  says so, and this work does not change it.

## 4 sexies. The DSP's program changes by arena — for three arenas out of twelve

Point 5 of the reading list, **left unanswered for want of a lever** from the start. The lever has existed
since the other session wrote the stage selection: `Model3::fv2_hold_stage` commands a versus match's arena
and `fv2_hold_training_stage` the route byte, which commands the arena of **a game's first match**.

So `tools/vf3run.cpp` takes `VF3_STAGE=n`, which **holds both** on every frame — held and not written once,
because the draw marks its own choice as used in taking it. The lever is in my tool and not in
`src/host/console_play.h`, which is another session's file.

> **This section first concluded the opposite, and it is the day's most instructive mistake.** With **two**
> arenas — 2 and 7 — everything was identical write for write, and I wrote "the DSP's program does not change
> by arena" while noting the limit: "two arenas out of twelve… redoing the other ten costs ten minutes and
> would be worth doing". The ten minutes turned the title over: **2 and 7 are both in the group of nine that
> load nothing more.** The caveat was right, the assertion was not. What follows is the measurement over all
> twelve.

### The control, arena by arena

A write does not prove the game read it. Twelve runs of 4 200 frames over `fv2-fight.txt`, one per arena,
and for each the footprint of the final RAM and of the WAV:

**twelve distinct RAM footprints, twelve distinct WAVs.** So the lever took for all twelve, and each run is
indeed a different game — the condition for the comparison to mean anything.

### Every arena shares the same loading, and three add one

The MPRO bursts fall on the same frames for all twelve up to frame 2438. Then the **last** burst splits the
arenas into two families:

| arenas | last MPRO burst | program |
|---|---|---|
| 1, 2, 3, 4, 5, 7, 8, 10, 12 | chip **1**, frames 2396-2438, 1 225 writes | `149e507a` |
| **6**, **9** | chip **0**, frames **2722-2738**, 1 193 writes | **`efff4af1`** |
| **11** | chip **0**, frames **2736-2738**, 1 169 writes | **`f977884a`** |

**Three arenas out of twelve load one more program, on chip 0, after the fight starts.** And `efff4af1`
**appears nowhere else**: nine arenas never see it. So it is a variation of the microcode by arena, and not
a mere shift in loading.

The detail of frame 2722-2724 for arena 9 is worth noting: it is **exactly** the frame where an earlier
measurement in this document had seen "a program written at frame 2724". So that measurement bore on an arena
of that group — the one the game chooses by itself.

### The images had to be compared, not the counts

The trap was one step away. Arena 11 makes **25 155 MPRO writes** against 23 986 for the nine — more, hence
"a different program", one would have concluded from the count alone. But by fingerprinting **the complete
image of `0x800-0xBFF`** at the end of each burst, arena 11 holds only the same two programs as the nine: its
extra burst **reloads `f977884a`**, it brings nothing new. Arenas 6 and 9, for their part, do hold a third
program.

A longer sequence of writes does not prove a different program. The two methods do not answer the same
question, and it is the second that answers this one.

### The coefficients, for their part, vary in five sets

Same method, on the sequences of COEF and MADRS writes, frame number removed:

| arenas | COEF | MADRS |
|---|---|---|
| 1, 2, 5, 7, 10 | `5b9d79a9e0` | `1b9661bb72` |
| 3, 4, 8, 12 | `308b7ad0ad` | `cd9a948eb7` |
| 6 | `980fc20056` | `a29a1505aa` |
| 9 | `099510d537` | `89bcda78a6` |
| 11 | `32b130ec67` | `7444a5af8d` |

**Five distinct sets of coefficients and delay-line addresses**, and the two areas give **exactly the same
partition** — which is expected, since `0x602B32`'s interpreter writes them from the same descriptors
(§5 bis bis). So the nine arenas that share a microcode still share **two** different settings between them.

And the disputed range `0x7C0-0x7FF` follows the microcode: 1 280 writes for the nine, 1 344 for {6, 9, 11}.
The 64 extra are the longer program, not a new incursion.

### The FM buffer: one arena out of twelve

| arenas | writes into `0x600-0x6FF` |
|---|---|
| 1, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12 | **0** |
| **2** | **993**, from frame 3783 on |

So `0x60153A`'s software modulation is used by **one arena out of twelve**. See the end of §4: that is also
where `STWINH` is settled, and it protects all 993 without exception.

### What that establishes, and what it does not

**The answer to the question asked is yes**: the driver can load a DSP program particular to the arena, and
it does so for three of the twelve. The parameters, for their part, vary for all of them — in five sets.

What it does not do:

- **Nothing about *Virtua Fighter 3***, whose driver is the same and whose data is not.
- **One scene only**, `fv2-fight.txt`, and only its first match. A longer game crosses several arenas and
  could show others.
- **Why** those three arenas, and why two of them share a program: not read. The `0xEF1` that picks the
  preset is written by a control command (§5 bis bis); who sends it from the PowerPC side is not looked
  into.
- And the discrepancy in counts with this document's earlier measurement ("43 bursts, 9 programs")
  **remains unexplained**. This count sees 13 to 15 of them depending on the arena, and 2 or 3 programs. I do
  not give it as resolved.

## 5. The range `0x7C0-0x7FF`: the mechanism is settled, the magnitude is not

**The program does wipe `0x7C0-0x7FF`, and it does so AFTER loading MADRS.** Measured on 2026-10-24 with a
probe written for it (`VF3_SCSP_DSPLOG`, `soundboard.cpp`), on both games.

What the probe says, and that is all it says:

| | Fighting Vipers 2 | Virtua Fighter 3 |
|---|---|---|
| writes into the DSP registers | 33 370 | 14 702 |
| of which MPRO / COEF / MADRS / `7C0-7FF` | 25 179 / 5 007 / 1 840 / **1 344** | 11 568 / 1 745 / 749 / **640** |
| values written into `7C0-7FF` | **`0000`, all 1344** | **`0000`, all 640** |
| sequences (frame × chip) with a wipe | 41 | 20 |
| **non-zero** MADRS writes before the wipe | **878** | **176** |
| non-zero MADRS writes after | 53 | 19 |

The range's 32 words are written in one block, and the log shows the loop pass without a break from `0x7B8`,
`0x7BC`, `0x7BE` to `0x7C0`, `0x7C2`, `0x7C4`: **a single wipe of 64 words, from `0x780` to `0x7FF`**. Then,
in the same frame, two to four real addresses are rewritten.

### What it really is: a single sweep, and the mirror destroys itself

Corrected on the evening of 2026-10-24, after reading who writes MPRO. My first description — "the driver
loads MADRS, **then** wipes `0x7C0-0x7FF`" — described the order correctly but not the mechanism, and the
mechanism is sharper.

`0x602AD6`, called by `snd_reset` for both chips, wipes the DSP area in one stroke:

```
00602ADA  lea.l   $100700.l, a3        ; the start of the DSP area
00602AE0  adda.l  $c10.w, a3           ; + the chip's offset
00602AE4  move.w  #$4f, d7             ; 80 rounds
00602AE8  clr.l   (a3)+   x4           ; 16 bytes per round
```

80 × 16 = `0x500` bytes, that is **`0x700` to `0xBFF` in one go**: COEF, MADRS, `0x7C0-0x7FF` and MPRO.
Measured, frame 39: `COEF zero ×64, MADRS zero ×32, 7C0 zero ×32, MPRO zero ×512` — 640 words, exactly.

And **loading a preset follows the same path**. Frame 45, chip 0: **981 writes, from `0x700` to `0xBFE`,
monotonic, dominant step `+2`** (with 237 words written in two bytes, `mask ff00` then `00ff`). It is not
"load MADRS then wipe": it is **a single linear sweep of the whole DSP area from a block in ROM**, and that
block's 32 words for `0x7C0-0x7FF` are zero.

**Hence the argument, and it is the day's strongest:** if the hardware mirrored `0x7C0-0x7FF` onto MADRS,
**no preset would survive its own loading** — the sweep would write the 32 delay-line addresses, then
overwrite them 32 words later, every time, for every preset and every game. The DSP would always run with
zero addresses. And yet the cabinet had a reverberation.

That is an **inference**, not a measurement: it assumes the period reverberation worked as intended. But it
comes from the game's own code, and not from an agreement between emulators — which is exactly the kind of
argument `scsp/docs/scsp_references.md` asks to prefer.

### What it does, and why it is a deduction and not a measurement

`scsp.cpp` (`w16`) with the mirror: a write at `0x7C0 + k` goes into `MADRS[k]`. The 32 words being zero,
**MADRS is zeroed entirely**, after the loading. So, with the mirror, Fighting Vipers 2's 878 addresses and
Virtua Fighter 3's **176** are overwritten, and only the 53 and 19 written afterwards remain. That is a
deduction from two facts (the measured order, the routing read in the code), not a third measurement.

### And what it makes audible: far less than the mechanism suggested

Measured just afterwards (`tools/sound/compare-madrs.cmd vf3`, 2400 frames of `vf3-match.txt`, both runs
produced without a window and compared by `tools/sound/measure-madrs.py`):

| | mirror (the current one) | separate | difference |
|---|---|---|---|
| mean peak, coin and selection | 519 | 666 | **+2.2 dB** |
| mean peak, fight | 2891 | 2898 | +0.0 dB |
| L/R correlation, fight | **0.861** | **0.865** | none |
| energy per band (4 bands) | — | — | **±0.1 dB** |
| coin echo, −20 dB reached | 2.05 s | 2.25 s | +0.20 s |
| start echo, −20 dB reached | 0.95 s | 1.15 s | +0.20 s |
| maximum difference between the two files | — | — | 1897, that is −24.7 dBFS |

**The two files differ, and very little.** And above all: **the left/right correlation does not move**
(0.861 against 0.865), when that is THE signature of the collapse — on VF2 the same measurement gave 0.99
with the mirror against 0.73-0.85 without. Here, nothing.

So **the mechanism is established and its audible consequence, on this scene, is small**: two tenths of a
second of decay and 2.2 dB on the coin. That is not the order of magnitude of what `scsp.h` describes for
VF3 ("kept apart, they rang on for several [seconds]"), and it is not that of Fighting Vipers 2's defect
(mean peak 22831 → 6532, that is 11 dB, and the correlation collapsing).

Three possible readings, **none settled**:

1. VF3's DSP program uses few delay lines, or its send to the DSP (`EFSDL`) is low on this scene, and
   overwriting MADRS therefore changes little there;
2. the 19 addresses written **after** the wipe are the ones that carry the sound, and the 176 overwritten
   were not in use;
3. **the scene does not go where it shows.** `vf3-match.txt` does the boot, the attract, a coin, the
   selection and one round of random play — no K.O., no victory screen, no variety of arenas. Fighting
   Vipers 2's defect lived precisely on the victory screen (`VIC_DSP`).

The third is the most likely and the cheapest to check, and it is the one a listening test should aim at.

### Where the difference really is, and how big

Frédéric heard no difference between the two files. The signal analysis says why: **the difference exists,
it is real, and it fits into a single moment.**

The **mirror file drops to exact digital zero at 12.54 s**, where the **separate one keeps a reverberation
tail** that runs on for another 1.9 s, decaying from 311 to 10 (that is −40.5 to −70 dBFS). Over the whole
scene:

| | mirror | separate |
|---|---|---|
| strictly zero samples | **19.90 s** out of 38.8 | **10.68 s** out of 38.8 |

The 9.2 s of difference are reverberation tails that the mirror cuts short. But they are spread over **369
stretches**, of which only one really counts:

| start | length | the separate one's peak | |
|---|---|---|---|
| **12.54 s** | 0.59 s | 311 | **−40.5 dBFS** — the only clearly audible one |
| 13.12 s | 0.15 s | 115 | −49.1 dBFS |
| 13.28 and 13.41 s | 0.14 s | 72 | −53.2 dBFS |
| 15.08 s | 1.42 s | 4 | −78.3 dBFS — the level of the least significant bit |
| 16.58 s | 5.26 s | **1** | −90.3 dBFS — a single LSB, inaudible by any account |

`tools/sound/audibility.py` (noise-to-mask ratio, with spreading of the masking, the absolute threshold of
hearing and room noise) places that moment at **+32 dB above the threshold** — because it falls into a
silence, where nothing masks it. `build/extrait-vf3-AB.wav` makes it obvious: 11.8 to 14.6 s, the two
versions one after the other, +18 dB.

### The control, and what it invalidated

The same measurement on a case whose answer is known — Fighting Vipers 2's victory screen, where the mirror
gives rise to a sound that climbs 24 dB up to the limiter:

| | median | 99th percentile | share of windows > 0 |
|---|---|---|---|
| FV2 victory screen (gross defect, **known**) | −15.3 dB | +20.0 dB | **10.0 %** |
| Virtua Fighter 3 (slight difference, inaudible by ear) | −2.2 dB | +25.0 dB | **47.9 %** |

**The control returns smaller figures than the subtle case.** That is not a failure of the tool: the NMR
measures whether a difference is **unmasked**, not whether it is **serious**. FV2's defect is a loud,
sustained sound, so its own mask is enormous and the NMR stays low; VF3's difference falls into a silence,
where nothing masks it.

Two lessons, and the second is the real one:

1. for "is it serious", you need the level, the spectrum and the correlation (`measure-madrs.py`), not the
   NMR;
2. **without the control, I would have announced that VF3's difference was larger than FV2's defect.** The
   first two versions of `audibility.py` were wrong — the first compared each band with itself, ignoring the
   masking by its neighbours; the second counted residues at −90 dBFS as audible for want of an ambient
   noise floor. Each gave plausible figures. It is the control that killed them.

### What that overturns, and to what extent

`src/sound/scsp.h` keeps the mirror for Virtua Fighter 3 on the grounds that it was "MEASURED RIGHT: its
coin and start echoes die in about a second, as the reference's do; kept apart, they rang on for several".

What is settled: **the mechanism**. The mirror does overwrite 176 delay-line addresses on VF3, and the
reverberation's length depends on it — that was not a neutral setting.

What is not, and what the previous paragraph's measurement calls into doubt: **the magnitude**. The coin and
start echoes, measured here at the same place, go from 0.95 s to 1.15 s and from 2.05 s to 2.25 s. "Several
seconds" is not reproduced. Either the original measurement bore on something else (another scene, a
listening test at the window), or it aimed at a state `vf3-match.txt` does not pass through.

And it is **the same mechanism** that was judged wrong on Fighting Vipers 2 on 2026-10-24 (commit `335c050`,
the victory screen's sound feeding itself, mean peak 22831 → 6532 when separated), and the same one MAME
corrected on its side for `doa` (#16527). Three games, one mechanism.

**What is NOT settled, and must not be concluded:** what the hardware does. If a real SCSP mirrors that
range, then the cabinet ran with its delay lines collapsed too, and the short reverberation **is** the period
sound. Nothing here says so, and there is no Model 3 on this desk.

**So what needs doing, and what I have not done:** take up the VF3 measurement again with
`VF3_SCSP_MADRS_SEPARATE=1`, and listen. Changing that setting changes the port's sound: it is a decision,
not a correction, and it is not mine to make.


## 5 bis. How the question stood before the probe

A reminder of the question (`scsp/docs/scsp_references.md`): this port keeps the MADRS mirror for VF3 and
separates it for Fighting Vipers 2; MAME `master` discards the writes and returns MADRS on reads. Nobody had
looked at what the program does.

What was found: in the whole image, **no absolute reference to `+7C0`-`+7FF`** — but **COEF (`+700`) seven
times and MADRS (`+780`) three times**, all grouped between `0x602A82` and `0x602C52`. In other words the
DSP's loading is there, and if it wipes `0x7C0-0x7FF` it does so **through an address register in a loop**,
from the MADRS base, not through an address written in the clear — which is exactly the pattern `scsp.h`'s
comment describes ("writes COEF and MADRS, then zeros 0x7c0-0x7ff").

**The function is `snd_charger_preset_dsp`, `0x602B32`**, read in Ghidra: a command interpreter over a table
of presets. Each command carries an opcode (`+8 & 0x1F`: 1, 2, 4, 0xD, 0xE, 0x10, 0x16), a destination
offset for COEF (`+0xA`) and one for MADRS (`+0xB`), and a bit `+8 & 0x20` that makes a base offset be added
to the MADRS values. Opcode 2 writes 10 words of COEF then 8 of MADRS; opcode 0x16, 38 words of COEF; opcode
4, three COEF and two MADRS.

In other words **the code alone could not answer**: the destinations come from the data table. §5's probe was
needed. That is the day's lesson in method — reading the code says *what to look for*, and sometimes only the
measurement says *what happens*.

The function's last pass writes the byte `0x100017 + slot*0x20`, that is each slot's **EFSDL/EFPAN** (the
DSP's return level and its pan), with the table's value — or **a forced `0xE0`** (EFSDL = 7, the maximum) if
the sound RAM's global flag `0xEFB` is set. That is the reverberation's switch.


## 5 ter. A5 and A6: an assertion of mine, false, and undone the next day

On 2026-10-24 I wrote — in a Ghidra comment, in a script and in `fd74c38`'s message — that the driver sets
A5 to `0x00100000` and A6 to `0x1000` **once at reset and never touches them again**, and that telling
Ghidra so would raise its function count. **Both halves were false.**

### What the measurement says

| | writes | |
|---|---|---|
| **A5** | **33** | `0x00100000` by `lea` in seven places; **`[0xC10]`, the chip's offset (0 or `0x200000`), in three**; zero in two; an indirect pointer in some twenty others |
| **A6** | **8** | `0x1000` twice, `0x00201000` once, and indirect pointers |

These are **ordinary address registers**, not global bases. And at `0x600B9C` — in section 4 quinquies of
this document, written the same night — A5 takes **the address of a slot's registers**. The document was
already contradicting itself two sections apart.

### What the intervention gave

Set over the whole range and the analysis rerun, headless:

```
functions before: 41
functions after:  41   (+0)
```

Nothing. And a **false constant remained in the project's database**, which the decompiler would have applied
to the twenty-six places where A5 is something else. Removed
(`tools/ghidra/FV2SoundRegistersUndo.java`, which reads back `null` to check it), `0x600142`'s comment
replaced by what was measured, and `FV2SonRegistres.java` deleted from the repository.

### The three ways it held, and the fourth during the correction

1. **The assertion comes from a single instruction, generalised** — the reset's `lea` — when my notes from
   the same session **already quoted** `movea.l $c10.w, a5` at `0x602B32`. The counter-example was in hand
   and was not read.
2. **My first opcode sweep returned "zero writes"**, which looked like a confirmation. The mask `0xFFC0` had
   been written `0xF1C0`, which **erased the register's bits**: the comparison could never be true. I only
   caught it because I had read the instructions by hand the day before and the zero contradicted what I
   knew. Without that truth in front of me, I would have published a second falsehood.
3. **And during the correction itself**: my `Select-String` filter looked for `Defaire>` where the prefix is
   `FV2SoundRegistersUndo.java>`. The script had run and succeeded; its output was hidden. A successful run
   and a mute run, indistinguishable — exactly the repository's memory about probes, in miniature.

It is the same fault as the two of the day before (§4, the records' arithmetic): **a frequency or a scope
asserted from a reading, without a measurement.** Three times in two days, and the third looked verified.

### And why Ghidra only finds 41: measured, and it was indeed the jump tables

The question stayed "unexplained" for an hour, the time it took to measure it instead of supposing it. And
first by checking that "41 against 95" was comparing the same thing — my walker calls *function* any target
of a `bsr`, which Ghidra does not. Out of this pass's 82 entry points:

| | |
|---|---|
| 34 | are the **entry** of a Ghidra function |
| **2** | fall **inside** a body without being its entry — labels, not functions |
| **46** | fall **outside every body** — code Ghidra ignores |

So the denominator was honest to within two, and the gap is real. What Ghidra covers, in addresses:

**6 806 of the 15 136 bytes of code, that is 45 %.** And the holes are not scattered: **the largest is 7 278
bytes**, from `0x600BC6` to `0x602833` — nearly half the code in one block. It starts **exactly** where
`snd_reset`'s body stops, which Ghidra fused over 2 726 bytes in one piece.

And the fact that settles it:

| the dispatcher's target | | its state in Ghidra |
|---|---|---|
| `0x60120A` | `8n` note off | **ignored** |
| `0x6008AC` | `9n` note on | **ignored** |
| `0x6025F2` | `An` the game's protocol | **ignored** |
| `0x601402` | `Bn` control change | **ignored** |
| `0x601C1A` | `Cn` program change | **ignored** |
| `0x6012D6` | `En` pitch bend | **ignored** |

**All six, without exception.** Ghidra follows calls and fall-throughs, but it does not recover the two
PC-relative jump tables — `jmp (A1)` at `0x60088C` after `lea (0x60088E,PC,D0.w)`, and
`jmp $602730(pc,d2.w)` at `0x60272C`. Everything reachable only through them — the note on, the note off,
control, program, pitch bend, the copy engine, the slot's programming — is invisible to it. That is the hole
of 7 278 bytes.

### And opening the two tables: measured, +49 functions and coverage from 45 to 77 %

Done on 2026-10-25 (`tools/ghidra/FV2SoundJumpTables.java`). The two tables were first **reread and
verified** before a line was written, because their forms are not the same:

| | form | targets |
|---|---|---|
| `0x60088E`, the MIDI status dispatcher | **eight slots of 4 bytes**, each a `bra.w` (the `Fn` slot carries the `rts` they share) | 7 distinct, all in the code |
| `0x602730`, the control commands (banks `0x00`-`0x0F`) | **sixteen words**, each an offset **relative to the table's base** — `move.w $602730(pc,d2.w), d2` then `jmp $602730(pc,d2.w)` | 12 distinct (`0x602750` four times: the unused bank), all in the code |

The script disassembles each slot, adds a `COMPUTED_JUMP` reference from each `jmp` to each of its
destinations, creates a function at each distinct target, then reruns the analysis. And it prints **the
function count and the bytes covered**, before and after — the count alone can rise without the coverage
following.

```
before: 41 functions,  6806 bytes covered (45 % of the 15136 of code)
   20 distinct targets: 18 functions created, 2 already there
after:  90 functions, 11668 bytes covered (77 % of the 15136 of code)
```

**+49 functions, and the coverage from 45 to 77 %.** The 90 meet the 82 to 95 the hand-seeded walker was
reaching: **the gap is closed, and the cause was indeed the two tables.** That is, for once in this day, an
assertion I announced before measuring it and that the measurement confirmed.

### The remaining 23 %: two more tables, and a denominator of mine that counted data

Taken up the same day. And first a fault of method corrected: my hole computation assumed that a Ghidra
function body is **contiguous**. It is not — 90 functions carried 101 ranges, so eleven fragmented bodies.
With the real ranges asked for, here is what the holes were.

**The program's computed jumps, enumerated instead of guessed** — eleven, where I had dealt with two:

| | |
|---|---|
| `0x60088C` | `jmp (A1)` — table 1, the MIDI status dispatcher |
| `0x60272C` | `jmp (d8,PC,D2.w)` — table 2, the control commands |
| **`0x601418`** | `jmp (d8,PC,D2.w)` — **table 3** at `0x60141C`, the `Bn`'s sub-dispatcher |
| **`0x602EDC`** | `jmp (d8,PC,D0.w)` — **table 4** at `0x602EE0`, under bank `0x00` |
| seven others | `jmp (An)` — nothing static resolves them, and they are not touched |

And the two large holes were **exactly** tables 3's and 4's targets: 1 496 bytes at `0x6014C0` for the first,
956 at `0x602F16` for the second — where one finds one-line routines terminated by `rts`, a table's
signature.

**How to size them, since their length is written nowhere:** each table **ends where its lowest target
begins**, so the count is `(lowest target − base) / 2`. The rule was **checked first on table 2**, whose 16
entries had been counted by hand: it gives 16 back, and the script stops if it ceases to do so rather than
sizing the others blind. Table 3: **81 entries, 36 targets**. Table 4: **26 entries, 23 targets**.

```
before:  90 functions, 11668 bytes covered (77 % of the 15136 of code)
   79 distinct targets in all : 59 functions created, 20 already there
after : 168 functions, 14484 bytes covered (96 % of the 15136 of code)
```

**41 → 90 → 168 functions. 45 % → 77 % → 96 %.**

And what remains, identified rather than left as "unexplained" — 654 bytes in 9 holes:

| | bytes | |
|---|---|---|
| `0x600000-0x60011F` | 288 | the vector table |
| `0x60141C-0x6014BD` | 162 | **jump table 3 itself** |
| `0x602EE0-0x602F13` | 52 | **jump table 4 itself** |
| `0x602730-0x60274F` | 32 | **jump table 2 itself** |
| `0x6002D4-0x6002E7` | 20 | the ten words `0x600292` copies into the interrupt registers |
| `0x600396`, `0x60103C`, and two words | 100 | **unidentified** |

**554 of the 654 are data** — and my denominator of "15 136 bytes of code" counted them as code. That leaves
**100 bytes, 0.7 %**, of which two words follow a `jmp (An)` immediately: the next instruction, which no flow
reaches since the jump's destination is unknown.

So the 23 % were **two more tables, plus a denominator of mine that counted data tables as code**. And this
method's real ceiling is the seven `jmp (An)`.

> Two assertions in this paragraph are false, and the following sections correct them with the
> measurements: the `96 %` still holds the same denominator (15 136 bytes "of code" of which 650 are data,
> and the end bound itself was two bytes too low), and **all seven `jmp (An)` resolve** — each has a
> `lea (d8,PC,Dn)` four bytes above it. There was no ceiling.


### The remaining 4 %: my denominator again, and the driver signing its name

Taken up on 2026-10-25. The "4 % not decompiled" did not exist. Here are the nine holes, one by one, and
what they really are.

**First the bound, because it was skewing the arithmetic.** My constant said the code ends at `0x603B20`.
That address's `rts` occupies `0x603B20-0x603B21`: so the code runs to **`0x603B22`**, and there are 15 138
bytes, not 15 136. What follows is a table of ascending bytes (`80 81 … 8b 90 … 9b a0 …`), clearly data. The
check is arithmetic: with `0x603B22`, covered + holes = 14 484 + 654 = 15 138 and it closes; with `0x603B20`
it gave 15 138 for 15 136, and **those two bytes of discrepancy were the sign that the bound was wrong** — I
had seen them and not read them.

**Then `0x600100`, and it is the finest of the nine.** Thirty-two bytes that are neither code nor a table:

```
Ver3.5.9|98  1/13|model3|by Hiro
```

The driver carries **its version, its date, its target and its author**, and those thirty-two bytes end
*exactly* where the reset vector sends the processor, `0x600120`. The name is cut off by the end of the
field, not by the reading. So: *Fighting Vipers 2*'s sound code is version 3.5.9 of the "model3" driver,
dated 13 January 1998.

**The six word tables, and what each is — or is not.**

| | bytes | what is established |
|---|---|---|
| `0x600000` | 256 | the 68000's vector table: 64 longs, data by construction |
| **`0x6002D4`** | 20 | **ten words, ten registers**: `lea 0x100418,A0` / `lea 0x6002D4,A1` / `moveq #9,D7` / `move.w (A1)+,(A0)+` / `dbra`, at `0x600288`. These are the SCSP's common registers `0x418` to `0x42A` |
| `0x600396` | 32 | sixteen words read by `move.w (A1,D0.w)` after `lea 0x600396,A1`, index doubled, written into a buffer `(A0)+`. **What they are worth is not established** |
| `0x60103C` | 64 | **thirty-two descending words** (`0x1A00` → `0x0001`), read at `0x600CF6` and `0x6038DA`, always by `move.w (table,Dn.w), 0x0E(A4)`, the index being a counter filed in `0x0A(A5)`. **A curve of 32 steps; its nature is not established** |
| `0x60141C` | 162 | jump table 3 itself: 81 words |
| `0x602730` | 32 | jump table 2 itself: 16 words |
| `0x602EE0` | 52 | jump table 4 itself: 26 words |

**`0x6002D4` reads entirely**, and it is the board's interrupt configuration:

| register | value | |
|---|---|---|
| `0x418` TIMA/TACTL | `0x00CE` | count `0xCE`, prescaler 0 |
| `0x41A` TIMB/TBCTL | `0x03C6` | count `0xC6`, prescaler 3 |
| `0x41C` TIMC/TCCTL | `0x0100` | count `0x00`, prescaler 1 |
| `0x41E` **SCIEB** | `0x00C8` | bits 3, 6, 7: **MIDI input, timer A, timer B**. Timer C is **not** armed |
| `0x420` SCIPD | `0x0000` | |
| `0x422` SCIRE | `0x00C8` | the same three, cleared |
| `0x424` SCILV0 | `0x0088` | |
| `0x426` SCILV1 | `0x0048` | |
| `0x428` SCILV2 | `0x0000` | |
| `0x42A` MCIEB | `0x0020` | bit 5: timer A, armed towards the other side |

The three `SCILV` give the level per source, one bit each: **MIDI (bit 3) takes `0b011` = 3, timer A (bit 6)
takes `0b010` = 2, timer B (bit 7) takes `0b001` = 1**. Three sources, three levels — which cross-checks the
three levels the vector table serves. This is a **reading**, not a measurement on a machine: it comes from
those ten words and the layout of the SCSP's registers. What it predicts and what remains to be checked on
the port: three interrupt sources and only three, at those three levels.

> **The timers' names in that table were corrected the same day.** The first version said "timer B and timer
> C armed, timer A not", and the opposite is true. What revealed it: `snd_irq2` reloads `0x418` — TIMA — and
> acknowledges bit 6. The bits now come from the port's implementation (`timerA_cb()` sets `0x40`,
> `timerB_cb()` `0x80`, `timerC_cb()` `0x100`) and not from a memory of a table. The **levels** 3, 2, 1 were
> right from the start; only the names were permuted. See 5 bis ter.

**And the last four bytes, the only ones Ghidra really was missing.** Two words, `24 d9` each, at `0x601F3C`
and `0x6023E2`. I had counted them among the **seven `jmp (An)` that "nothing static resolves"**. That was
false: a `lea` names them four bytes above.

```
00601F2E  move.w  #$200, d0
00601F32  sub.w   d5, d0
00601F34  lsr.w   #$1, d0
00601F36  lea.l   $601f3c(pc, d0.w), a0
00601F3A  jmp     (a0)
00601F3C  move.l  (a1)+, (a2)+        <- the two bytes
00601F3E  move.l  (a1)+, (a2)+        ... and 126 times more
```

It is a **Duff's device**: 128 `move.l (A1)+,(A2)+` in a row, and the entry is computed —
`D0 = (0x200 − D5) >> 1` — so that the copy starts the further into the block the less there is left to
copy. 512 bytes at most, without a loop test. Both blocks belong to `snd_reset`, which goes from 2 728 to
2 730 bytes of body. **So five `jmp (An)` remain unsolvable, not seven**: `0x600308`, `0x602042`,
`0x6024E8`, `0x603494`, `0x60376A`.

**The final count, and its control.** `tools/ghidra/FV2SoundData.java` declares the seven data blocks,
attaches the four bytes of code, then **recounts for itself** what remains outside every function body *and*
outside every declared datum:

```
168 functions, 14488 bytes in a body, 650 bytes of declared data, out of 15138 from 600000-603B22
real code = 15138 - 650 = 14488 bytes, of which 14488 in a body = 100.00 %
LEFT OUTSIDE EVERYTHING : 0 bytes
```

A script that counts its own work is not a control, so the count was redone by **the other path**:
`FV2SoundList.java`, which this work has not touched, gives the ranges back, and the external hole computation
draws from them **14 488 bytes covered and 7 holes for 650 bytes** — exactly the seven data blocks, the two
two-byte holes having disappeared. 14 488 + 650 = 15 138. The two paths share nothing and give the same
number.

**And what "100 %" means, so that it is not read as more than it is:** every byte of code is in a function's
body, and every byte of data is declared and commented. That is **not** to say the program is understood.
Fifty-nine of the 168 functions still carry the name Ghidra gave them, nobody has read them, and two of the
data tables above have no established meaning. Coverage measures the surface reached, not the reading done.

**41 → 90 → 168 functions. And the code's coverage, with an honest denominator: 100 %.** The 4 % were not
undecompiled code; they were a vector table, an author's signature, three jump tables, three value tables —
and an end bound wrong by two bytes. What remains open is not an unread surface, it is **the meaning** of two
tables (`0x600396`, `0x60103C`) and five indirect jumps whose destination cannot be computed without running
the program.

### And the five remaining `jmp (An)`: zero. The sentence was wrong about all seven

The lesson of the first two applies at once to the other five, and looking at **the preceding instruction**
was all it took. The five, measured the same day:

| the `jmp` | what is four bytes above | what it is |
|---|---|---|
| `0x600308` | `lea 0x60030A(pc,D1.w),A1`, `D1 = <v> & 0x0E` | branch table, **8 slots of 2** (`bra.b`) |
| `0x603494` | `lea 0x603496(pc,D0.w),A0`, `D0 = (D1>>2) & 0x1C` | branch table, **8 slots of 4** (`bra.w`) |
| `0x60376A` | `lea 0x60376C(pc,D0.w),A4`, `D0 = (D0 & 7) << 2` | branch table, **8 slots of 4** (`bra.w`) |
| `0x602042` | `lea 0x602044(pc,D5.w),A0` | unrolled block, **128 × `clr.l (A2)+`** |
| `0x6024E8` | `lea 0x6024EA(pc,D5.w),A0` | unrolled block, **128 × `clr.l (A2)+`** |

**The index's mask gives the count**, and that is better than the self-sizing rule because it is written in
the code: `& 0x0E` lets through only eight even values, `& 0x1C` and `(&7)<<2` eight multiples of four.

Two surprises in the slots, and they would have made a rule that only counted branches tell a lie:

- at `0x60030A`, **seven `bra.b` only**. The eighth slot, `0x600318`, is already ordinary code
  (`movea.l (0xC10).w,A2`): the last case's handler is placed there, instead of behind a branch.
- at `0x60376C`, **seven `bra.w` only**. The eighth, `0x603788`, is `move.w #$2000,SR ; rts` inline — and
  slot 6 branches to it. **Table 1 does exactly the same thing** with the `rts` its slots share.

Eight destinations each, then, seven branches and one handler inline. The script checks each slot's opcode
and **names** those that are not `bra` instead of supposing; it gave back by itself the two addresses the
reading had found.

**And the two unrolled blocks clear, they do not copy** — `clr.l (A2)+`, not `move.l (A1)+,(A2)+`. They go in
pairs with the two copies at `0x601F3C` and `0x6023E2`, and **the pair's arithmetic closes**:

| | index | instructions executed | effect |
|---|---|---|---|
| the copy, `0x601F36` | `D0 = (0x200 − D5) >> 1` | `128 − (0x200−D5)/4 = D5/4` | **`D5` bytes copied** |
| the clear, `0x60203E` | `D5 >> 1` | `128 − D5/4` | **`512 − D5` bytes cleared** |

A destination of 512 bytes: `D5` bytes from `(A1)`, zeros for the rest. The four blocks are **128
instructions, `0x100` bytes, 512 bytes handled** — measured by counting the identical patterns, not deduced.
The `0x200` of `move.w #$200,D0` is that buffer, and it appears twice. **What those 512 bytes are is not
established.**

`tools/ghidra/FV2SoundIndirectJumps.java` lays down the 26 computed-jump references. It **creates no
function**: coverage is already 100 %, those targets are inside existing bodies, and cutting a function in
two at a branch target would make the listing worse. The check afterwards — `FV2SoundList` again — gives back
168 functions, 14 488 bytes, the seven data blocks: nothing has regressed.

**No unresolved jump remains in this program.** And what held the error up for so long is simple: I read
`jmp (An)` as the whole instruction, when the answer was four bytes above, in the instruction I had not
looked at.


## 5 bis quaterdecies. The rest of the program, read

The last sixty functions, read on 2026-10-25 to close the reading. They sort into six families, and several
of them close loops left open elsewhere in this document.

### What the reset installs in RAM

| | |
|---|---|
| `0x6003D0` | **copies the 64 longs at `0x600000` to `0x0`** — the vector table, from ROM to RAM. That is how the 68000 gets its vectors, since at address 0 the board puts RAM and the ROM is at `0x600000`. The table at `0x600000` is the model, the one at `0x000` is the live one |
| `0x600592` | copies `[0x608030]` bytes from `[0x60802C]` to `[0x608038]`: **the instrument table**, from ROM to RAM. Hence the pointer `0x6014C0` and CC 1 follow |
| `0x600556` | copies a block from `[0x608000]` to `0x3000`: **the table of sample records**, if `[0x608000]` is not negative. The origin of §5 bis sexies' `0x3000` |
| `0x600524` | `lea 0x603FE2, a0` then 512 rounds of two `move.l` to `0x2000`, and again to `0x202000`: **4 096 bytes of ROM into each sound RAM**, at `0x2000-0x2FFF`. Content unidentified |
| `0x6002A2` | the 32 voice records: `+0x15` over sixteen bytes in steps of 8, `+0x0B` and `+0x0D` to `0xFF`, `+0` to `0x10` |
| `0x6003B6` | clears byte `0x17` of the registers of **18** slots (`moveq #$11`). Why 18 and not 32 is not explained |

### Table 1's six slots

`0x600892`, `0x600896`, `0x60089A`, `0x60089E`, `0x6008A2`, `0x6008A6`: four bytes each, one `bra.w` each.
These are **table 1's slots** themselves, which Ghidra counts as functions because `0x60088C`'s `jmp (A1)`
jumps into them. The seventh slot is the shared `rts` at `0x6008AA` (§5 bis quinquies).

### Putting a note out

| | |
|---|---|
| `0x60128E` | **note off**. If bit 6 of the object — the sustain pedal — is set, it cuts nothing: `bset #$1, $4(a4)`, the voice is marked "to be released". Otherwise `move.b #$10, $0(a5)`, bits 0 and 5 of the `+4`, and the record's `+0x0E`/`+0x10` serve as a countdown |
| `0x601370`, `0x601388` | a sweep of the 32 records on channel `D1`, and the body called for each one, which reads `[0xED4]` and the record's `+7` |
| `0x601BFA` | sweeps the 32 records and stops **all those on channel `D5`**, then `andi.b #$ef, $c01.w`. That is the "all notes off" CC `0x50` calls |
| `0x602F68`, `0x602F84` | a **fourth granularity of stopping** (§5 bis septies gave three): the voices whose `+4` has **bit 2** set, on both chips — the same bit as the voice stealer's predicate (§5 bis decies) |

### The eleven editors of the instrument record

`0x6014DE` finds the current channel's channel object and leaves it in `A0`, **popping the return address**
if there is none — an emergency exit that cancels the caller. `0x6014C0` follows that object's `+2` index
into the instrument table at `[0x608038]`, checks the magic word `0x8000`, and leaves the record in `A1`.
Eleven handlers use it, and the code-sharing pattern is remarkable:

| | writes |
|---|---|
| `0x601582` | `$a(a1)` bits 7-4 |
| `0x601598` | `$a(a1)` bit 3, set or cleared according to the value |
| `0x6015B8` / `0x6015D6` | `$b(a1)` bit 7 / its bits 6-0 — a flag and a value |
| `0x6015EC` / `0x6015F6` | the same, with `D2 = 5`: so `$10(a1)` |
| `0x601600` / `0x60160A` | the same, with `D2 = 6`: so `$11(a1)` |
| `0x601572`, `0x60157A` | `D2 = 2`, and they join the bodies of CC `0x41` and `0x42` |

Three pairs (flag, value) on `+0x0B`, `+0x10`, `+0x11`, each written by two handlers that share the same body
down to one index register. Then `0x601526` reapplies, sweeping both chips' 32 records through `0x60153A`.

### `0x60153A`, and the FM buffer my probe had never seen written

```
0060154A  move.b $a(a1), $e(a5)        ; the instrument's +0x0A -> the slot's register 0x0E
00601550  move.l a5, d4 ; lsr.w #$4, d4 ; addi.l #$600, d4
0060155C  move.w $c(a1), $0(a5)        ; -> 0x600 + slot x 2
00601562  move.w $e(a1), $40(a5)       ; -> 0x640 + slot x 2
```

The computation is worth a close look: `lsr.w #$4` shifts only **the low word**. With
`A5 = 0x100000 + slot × 0x20`, the low word is `slot × 0x20`, shifted by 4 it gives `slot × 2`, and the high
word stays `0x0010`. Plus `0x600`, the address is therefore `0x100600 + slot × 2` — **the FM buffer**.

**And that is the reconciliation of a measurement from §4.** My probe had looked for writes at `+0x600` and
found **zero** over 4 200 frames of *Fighting Vipers 2* and 2 400 of *Virtua Fighter 3*, with a positive
control. Here is the code that would make them. It is reached only by `0x601526`, so by those eleven
controllers. The conclusion is not that the probe was wrong: it is that **the feature exists and these scenes
do not use it**. Both facts hold together.

### The voice groups at `0x380`

The jump table at `0x60376C` — the one for `0x60376A`'s `jmp (A4)`, opened in the section "And the five
remaining `jmp (An)`" — dispatches six handlers that all work on an `A0` structure of `0x10` bytes: `+0` an
eight-bit mask saying which members are active, `+8` to `+0xF` **the slot numbers of the eight members**
(`move.b $8(a0,d6.w), d5 / andi.w #$1f / lsl.w #$5` gives `slot × 0x20`). These are the **eight entries at
`0x380-0x3FF`** §2 had seen without knowing what they were for: a **voice group**, an instrument spread over
several slots.

| | |
|---|---|
| `0x6039A6` | puts **one** member out: `0x10` into its register `0x00`, `+4` bit 0 of its record, the member's bit cleared, `+0x0E` copied into `+0x10` |
| `0x6039F4` | reads a pan from the stream, converts it through `0x601A98` — §5 quater's `DIPAN` converter — files it in `+2` and applies it to the eight members |
| `0x603A38` | a level clamped to 7, filed into `+3` under mask `0x78`, applied to the eight |
| `0x603A82` | **the same body**, joined at `0x603A46` with `D7 = 7` instead of `0x78`: the value `(v & 0xF) << 3` therefore goes into bits 6-3 of the same byte. Two fields, one body, a mask in a register |
| `0x603A90` | **loads a group definition**: index bounded by the word `+0` of `[0x60803C]`, long pointer at `+2 + index × 4`, and the bytes copied into `+1`, `+3`, `+4` |

So `[0x60803C]` is the root of the group definitions — and it is the same long `0x603198` tests to decide
whether the feature exists (§5 bis septies).

### The remaining control commands

| | |
|---|---|
| `0x602A60`, `0x602B20` | **pick the chip**: `D5` zero → `[0xC10] = 0`, otherwise `0x200000`. That is the origin of the selector the whole driver consults |
| `0x602E4C`, `0x602E9E` | call `0x602E58` and `0x602EB0` once per chip |
| `0x602E58` | a second reader of the DSP preset block (`0x602CE4`), which requires `$10(a1) == 1` and compares `$11(a1)` with the channel: a per-channel variant |
| `0x602EB0` | guarded by `0xEFB` like CC 10, it writes byte `0x17` of the register of the slot at index `channel` (`(D5 & 0xF) << 5` added to the registers' base), fed by the converter `0x601A98`. **Which field bytes `0x16` and `0x17` carry respectively is not settled**: CC 10 writes `0x16`, this writes `0x17`, and the register layout I have does not explain both |
| `0x60279A`, `0x602834` | a pair that sets `[0xC19]`/`[0xC1A]` and scales one against the other |
| `0x602AD0` | `[0xEF1 + chip] = 0xFF`: the DSP preset put back to "none" |
| `0x602F38` / `0x602F40` | `[0xEDC]` to `0xFF` / to zero. And `0xEDC` is what `0x60344A` tests in order to **run the next event without waiting for the countdown**: so a fast-forward of the tracks |
| `0x602F46` / `0x602F62` | transpose by `+1` / `−1` the `+4` of the objects whose bit 0 is clear — the same body, `D1` being `1` or `0xFF` |
| `0x602F9A` | `[0xEFE] = 0x8000` |
| `0x603274` / `0x60327C` | `[0xC3F]` to 1 / to zero |
| `0x602F14`, `0x602F34`, `0x602F36`, `0x602F66` | bare `rts`: entries of table 4 that do **nothing** |
| `0x601954` | the second half of `0x60193A`, which Ghidra counts apart because the first falls through into it |

### Two routines in duplicate

`0x603718` and `0x60361E` are **the same code**: `btst #$5, $0(a2)` on the track's flags, then the wait for
`MOFULL` (bit 4 of byte `0x404`) before writing the byte into `MOBUF` at `0x407`. Two copies of the MIDI
pass-through, in two different reading paths (§5 bis quater).

And `0x603638` is a **second reader of variable-length quantities**: `move.b (a3)+, d1 / bpl -> done /
andi.b #$7f / or / lsl.l #$7 / again`. That one is **not bounded to two bytes** like the delta-times' reader
(`0x603664`), it accumulates as many bytes as the stream gives. Two VLQ readers, two limits.


## 5 bis duodecies. `0x60048C`: the initialisation that lays down the three sentinels

82 bytes, called just once, from `0x60020C` in `snd_reset`. It is short and every line closes a loop left
open elsewhere.

```
0060048C  lea.l $400.w, a4 ; move.l #$100000, d0 ; bsr $6004de     ; chip 0
00600498  lea.l $200400.l, a4 ; move.l #$300000, d0 ; bsr $6004de  ; chip 1
006004DE  moveq #$1f, d7 ; move.l d0, (a4) ; lea.l $20(a4), a4 ; addi.w #$20, d0 ; dbra
```

**Here is the loop `group_of()` assumes.** Each record's first long receives the address of its slot's
registers: `0x100000 + slot × 0x20` for the first chip, `0x300000 + slot × 0x20` for the second. That is
exactly what the port tests (`r[0] != (n ? 0x30 : 0x10)`), and §2 gave it as confirmed "by a loop at reset" —
here it is, named.

Then the sentinels, and all three correspond to a test read in another function:

| | set to | and it is the sentinel tested by |
|---|---|---|
| `[0xEF9]` | `0x77` | the bits `0x60288A` composes into the bank register (§5 bis sexies) |
| `[0xC18]`, `[0x200C18]` | `0x7F` | `0x60288A`'s `cmpi.b #$7f, d0 / beq -> exit`: "no sample set loaded" |
| `[0xEF1]`, `[0x200EF1]` | `0xFF` | `0x602CE4`'s `bmi -> give up`: negative, so "no DSP preset" |

And one line that stays obscure: `moveq #$ff, d1 / move.b $60807d.l, d0 / lsr.l d0, d1` then
`[0xED8] = [0x200ED8] = d1`. `[0x60807D]` is the voice quota, which is 32 (§5 bis decies), and shifting a
long by 32 places gives **zero**. So the mask is zero on this ROM, and it would only be non-zero with a quota
below 8. What `0xED8` is for is **not** read.

## 5 bis terdecies. `0x603282`: a two-level lookup, by bank and index

80 bytes, reached only through a jump table. It is guarded twice, then makes two indirections:

```
00603282  tst.b $c1c.w ; bmi -> exit            ; a mode, negative = inactive
00603288  cmp.b $c1b.w, d5 ; bne -> exit        ; D5, the MIDI channel, must equal [0xC1B]
0060328E  movea.l $60801c.l, a4 ; movea.l a4, a2
00603298  move.b $c1c.w, d0 ; subi.w #$10, d0 ; lsl.w #$1, d0
006032A2  move.w $2(a4, d0.w), d0
006032A6  cmpi.w #$ffff, d0 ; beq -> 602644      ; absent
006032AE  adda.l d0, a4
006032B2  move.b $c1d.w, d0
006032B6  cmp.w (a4), d0 ; bgt -> 602644         ; bound : the sub-table's word +0
006032BC  lsl.w #$1, d0 ; move.w $2(a4, d0.w), d1 ; adda.l d1, a2
```

So: **`[0xC1B]` a channel, `[0xC1C]` a first index offset by `0x10`, `[0xC1D]` a second**, and two cascading
word tables from `[0x60801C]`, each bounded by its own word `+0` — the same convention as this driver's four
other tables — with `0xFFFF` for "no entry".

A two-level lookup by bank and index, guarded on a channel: **that looks a great deal like the
`A0 <bank> <index>` protocol** the reading list was after. It is not established — what is, is the lookup's
shape and the three globals that index it.


## 5 bis undecies. The 35 control changes, and the two structures they edit

§5 quater had read CC 1, 7 and 10. Here is the whole family, read on 2026-10-25, and it sorts into **two
groups according to the structure aimed at** — which is this reading's fact of architecture.

### Group 1: the LFO register, in the channel object

A channel object's word `+8` is the image of a slot's register `0x12`, and five controllers each edit a field
of it, then call `0x60193A` to apply it:

| CC | code | mask | field of register `0x12` |
|---|---|---|---|
| `0x01` | `0x601882` | `+9 & 0xE0` | **`PLFOS`** (bits 7-5), the pitch LFO's depth |
| `0x02` | `0x601904` | `+9 & 0xF8`, value `>>4` | **`ALFOS`** (bits 2-0), the amplitude LFO's depth |
| `0x15` | `0x60169C` | `+9 & 0xE7`, value `<<3`, rejected if ≥ 4 | **`ALFOWS`** (bits 4-3), the amplitude waveform |
| `0x18` | `0x6016CE` | `+8 & 0x83`, value `<<2`, clamped to `0x1F` | **`LFOF`** (bits 14-10), the LFO's frequency |
| `0x14` | `0x60166C` → `0x601684` | `+8 & 0xFC`, rejected if ≥ 3 | **`PLFOWS`** (bits 9-8), the pitch waveform |

> **Corrected the same day.** I had first written that `CC 0x12`, `0x13` and `0x14` all three went to
> `0x601684` and set `PLFOWS`. That was false, and the fault is instructive: my automatic extraction read
> twenty instructions from each handler and **crossed the `bra`** that ends the first two, attributing the
> third's body to their entry. Only `CC 0x14` writes `PLFOWS`; `0x12` and `0x13` jump to `0x601632`, a
> generic body.

And `CC 0x37` (`0x60186E`) **falls into CC 1's code** after a prologue: an entry variant, not a separate
handler.

Three controllers go through `0x601632`, a body of eight instructions that simply writes the byte `D0` at a
fixed offset of the object, the offset being passed in `D3`:

| CC | code | offset | |
|---|---|---|---|
| `0x11` | `0x60162A` | `+4`, value `−0x40` | a second transposition, signed by the bias |
| `0x12` | `0x601652` | `+5`, value `−0x40` | **the transposition** — exactly the `+5` `0x601390` adds to the semitone count (§5 bis nonies) |
| `0x13` | `0x60165C` | `+7`, value clamped to `0x0C` then `<<4` | unidentified |

The `+5` closes a loop: §5 bis nonies had seen it **read** as a transposition by the pitch computation, and
here is the controller that **writes** it.

Two others edit the same object's mixing bytes:

| CC | code | | |
|---|---|---|---|
| `0x0A` | `0x601A08` | `+6 & 0xE0`, low 5 bits | **`DIPAN`** (§5 quater) |
| `0x19` | `0x601702` | `+6 & 0x1F`, high 3 bits | **`DISDL`**, the direct send level |
| `0x2A` | `0x601AB0` | `+0x0C & 0x78`, value clamped to 7 | **`IMXL`** (bits 2-0), the send level to the DSP |
| `0x2B` | `0x601ADE` | `+0x0C & 0x07`, value `<<3` then `& 0x78` | **`ISEL`** (bits 6-3), **which DSP input** the voice feeds |

So `+6` carries `DISDL` and `DIPAN` in one byte, like register `0x16`.

And `+0x0C` is the image of a slot's byte `0x15`, that is the low byte of register `0x14` — `0x600C36` copies
it over as it stands when a note is turned on: `move.b $c(a0), $15(a5)`.

> **I had first written that the two masks "overlap on bit 7" and that the sharing was not established.**
> That was a misreading: I had looked at the write masks without looking at **what each handler clamps `D0`
> to before writing**. CC `0x2A` does `cmpi.b #$7, d0 / ble / moveq #$7, d0`, so `D0 ≤ 7` and the `or`
> cannot reach bit 7; CC `0x2B` does `lsl.b #$3, d0 / andi.b #$78, d0`, so it only contributes bits 6-3.
> **There is no overlap at all**, and bit 7 is simply cleared by CC `0x2A`'s `andi #$78` and never set
> again.

So the split is **3 bits + 4 bits + one unused bit**, and six facts line up on it: the two widths, the two
positions, the unused most significant bit, and the copying of the whole byte into register `0x15`. The
layout comes from the port's implementation rather than from a memory of a table (`src/sound/scsp.cpp`:
`ISEL` is `(data[0xA] >> 3) & 0xF`, `IMXL` is `data[0xA] & 7`).

**What that means, and it is not a detail:** `CC 0x2B` chooses **which input of the effects DSP** a voice
feeds, and `CC 0x2A` **at what level**. That is the per-voice routing to the reverberation, commanded by a
MIDI controller — the other end of everything §4 sexies and 5 bis bis say about the DSP.

### Group 2: the ADSR envelope, in a template pointed to by `+0x1E`

`0x601762` is this group's key, and it fits in five instructions:

```
00601764  move.w $1e(a0), d1 ; beq -> 601104    ; the object's +0x1E : a 16-bit POINTER
0060176C  movea.l d1, a1 ; rts
```

Sixteen bits, so an address **in the sound RAM**, not in the register space: it is a **template in RAM**. It
is set at `0x6009A2` and `0x600B2A`, and the computation that precedes it tells its size:
`sub.b d0, d3 / mulu.w #$c, d3 / adda.l d3, a2` — a record of **`0x0C` bytes**, chosen by a bounded index. So
it is **not** a mirror of a slot's `0x20` bytes of registers, but a compressed form, and only a few fields
fit in it.

The controllers `0x30` to `0x34` edit it, and the five fields fall exactly on the layout of a slot's
registers `0x08` and `0x0A`, **with an offset of −2** (`a1+6` ↔ register `0x08`):

| CC | code | writes | SCSP field |
|---|---|---|---|
| `0x30` | `0x601732` | `$7(a1) & 0xE0`, 5 bits | **`AR`** — register `0x08`, bits 4-0 |
| `0x32` | `0x6017A2` | `$6(a1) & 0xF83F`, value `<<6` | **`D1R`** — register `0x08`, bits 10-6 |
| `0x33` | `0x6017D4` | `$6(a1) & 0x07`, value `<<3` | **`D2R`** — register `0x08`, bits 15-11 |
| `0x31` | `0x601770` | `$8(a1) & 0x7C1F`, value `<<5` | **`DL`** — register `0x0A`, bits 9-5 |
| `0x34` | `0x601808` | `$9(a1) & 0xE0`, 5 bits | **`RR`** — register `0x0A`, bits 4-0 |

**Five fields, five exact matches, a single offset.** That is what gives confidence: an offset hypothesis
that happened to land right by chance on five five-bit masks at four different positions would be an
expensive coincidence. And none of those five calls `0x60193A` — they write the template, which will be
applied at the next note on.

`CC 0x36` (`0x60183A`) writes `$2(a1)` as a byte, masked to `0x3F`, that is — at the same offset — the high
byte of register `0x04`, `LSA`. **Not confirmed**: one field is not worth the previous five.

The `+0x1E` is set at `0x6009A2` and `0x600B2A`, both in the selection of an instrument. So the template is
the current instrument's.

### The three that do something else

**`CC 0x40`, the sustain pedal** (`0x601B0A`): `bset #$6, $0(a0)` when pressed, `bclr #$6` when released,
then `bsr 0x601B50`. So **bit 6 of a channel object's flags is the sustain pedal** — which explains at last
the `andi.b #$bf` that `0x602FA0` and `0x603080` apply to the twenty objects on a general stop
(§5 bis septies): they **release the pedal everywhere**. The question put there is answered here.

**`CC 0x50`** (`0x601BA6`): `andi.b #$bf, $0(a0)` and `andi.w #$7f18, $8(a0)` — the pedal released and the
same mask `0x7F18` as `0x603172`, which clears `PLFOS`, `ALFOS` and `LFORE`. That is a **reset of the
controllers**.

**`CC 0x41`, `0x42`, `0x4D`** (`0x6014FE`, `0x601510`, `0x601614`) all go through `0x6014DE` then `0x6014C0`
— two argument-reading routines — before acting. `0x601614` writes `$5(a1)`; which structure `A1` designates
there is **not** established, and it is **not** a voice record's `+5`, whose write is at `0x6010F2`
(§5 quater).

Of table 3's 81 entries, **46 point at the "nothing" handler at `0x6014BE`**, and `CC 0x35` is one of them —
a hole in the middle of the block `0x30-0x34`, `0x36`, `0x37`.


## 5 bis decies. `0x601144` and `0x60119E`: allocating a voice, and stealing the oldest

Two functions to read together, 90 and 108 bytes, because the second is the first's fallback branch.

### The allocator: a round-robin, and two gates before it

```
00601144  tst.b $ed6.w ; bmi -> 60115c              ; the sample's flags : bit 7 skips the quota
0060114A  move.b $ee3(a3), d0 ; add.b $ee2(a3), d0  ; two counters
00601152  cmp.b $60807d.l, d0
00601158  bge.w $60119e                             ; AT THE QUOTA -> steal
0060115C  move.b $ee0(a3), d0 ; add.b $ee1(a3), d0  ; the two category counters
00601164  cmpi.b #$20, d0 ; beq -> 6011a0           ; 32 taken -> steal, BUT NOT AT THE SAME PLACE
0060116A  movea.l $ee4(a3), a4                      ; the round-robin cursor
00601170  cmpi.w #$800, d0 ; bne -> 601180 ; lea.l $400.w, a4   ; wrap at the end of the table
00601180  tst.b $4(a4) ; beq -> 601192               ; a free record
00601186  lea.l $20(a4), a4 ; dbra d7, $601170
0060118E  bra.w $601106                             ; none free although the counters say otherwise
00601192  move.l a4, d0 ; addi.w #$20, d0 ; move.l d0, $ee4(a3)  ; the cursor advances
```

So: a **round-robin search** for a record whose `+4` is zero, the cursor kept at `0xEE4`, wrapping from
`0x800` back to `0x400`. And `[0x60807D]` is a **quota in ROM** on the number of simultaneous voices. **It is
`0x20` in this ROM, that is 32** — as many as there are slots: so the quota is set wide open, and its gate
opens only at the moment everything is taken anyway. It is a setting the driver provides for, not a
constraint this game is under.

### The thief: the oldest, from two pools

`0x60119E` only looks at the **taken** records (`tst.b $4(a4) ; beq -> next`): it is not an allocation, it is
a steal. It keeps **two** candidates in parallel, each the minimum `+0xB` within its pool:

| candidate | pool |
|---|---|
| `A1` | the records passing the test: `D0 ≠ 0`, or `0xED6` bit 7, or **`+4` bit 2** |
| `A3` | the records whose **`+4` bit 5** is set |

and the exit prefers `A3`, otherwise `A1`, otherwise returns `A4 = 0` without stopping anything — the `rts`
at `0x601200`. In the first two cases it **stops the chosen voice** (`bsr 0x6006DC`) before returning the
record.

`+0xB` is the serial number set at note on from `0xEE0` or `0xEE1` (§5 bis septies), so looking for its
minimum is looking for **the oldest voice**.

### The entry point encodes the urgency

The detail that stopped me: the quota's gate branches to `0x60119E`, but the 32-taken one branches to
`0x6011A0` — **one instruction further on**, skipping the `clr.l d0`. And `D0` is exactly what the thief
tests to decide whether to apply its predicate:

| entry | `D0` | effect |
|---|---|---|
| `0x60119E`, from the quota | set to zero | the predicate applies: only the records marked `+4` bit 2 are stealable |
| `0x6011A0`, from the 32 taken | is `0x20` | **everything is stealable** |

So "at the quota" steals cautiously, "not one slot left" steals anything. Two policies in one function,
chosen by the entry address.

### And a consequence of the category marking

A **category A** voice's `+0xB` has its bit 7 **forced** by an `ori.b #$80` (§5 bis septies); a category B
one's does not. So every B value is `< 0x80` and every A value is `≥ 0x80`, and a search for the minimum
**will always take a B voice before an A voice** within one pool. The marking is not just a label: it
installs a **priority**, category A being protected.

That is a deduction from two measured facts — the `ori` and the search for the minimum — and it holds as long
as the two counters do not overflow into one another. Which of the two categories is the music remains, for
its part, not established.


## 5 bis nonies. `0x601390`: the note number becomes `OCT`/`FNS`, and the table proves it

114 bytes, one caller only — `0x600CA4`, in the note on. It converts a pitch into a slot register, and its
unit reads out of the code.

```
00601390  d4 = d5 >> 8 ; d5 &= 0xFF            ; d5 : semitones high, fine offset low
0060139C  d3 = $c(a4) ; add.b $14(a4), d3      ; plus TWO transpositions from the record
006013A4  add.b d5, d3 ; ext.w ; ext.l ; lsl.l #$1, d3
006013AC  d0 = d2 & 0x0F ; add.b d4, d0 ; ext ; lsl.l #$5, d0   ; semitones x 32
006013BC  add.l d3, d0
006013BE  bpl -> 6013CA
006013C0  addi.w #$180, d0 ; addi.w #$f800, d4 ; bra     ; negative : one octave below
006013CA  cmpi.l #$180, d0 ; bcs -> 6013da
006013D2  subi.w #$180, d0 ; move.w #$800, d4            ; beyond : one octave above
006013DE  lea.l $603d62.l, a1 ; move.w (a1, d0.w), d3    ; THE PITCH TABLE
006013E8  d0 = d2 & 0xF0 ; lsl.w #$7, d0                 ; the high nibble -> the OCT field
006013F0  add.w d4, d0 ; add.w d3, d0
006013F4  andi.w #$7bff, d0                              ; bits 15 and 10, unused
006013F8  movea.l $0(a4), a5 ; move.w d0, $10(a5)
```

**The unit is 1/16 of a semitone.** `0x180` = 384, and `384 = 12 × 32`: twelve semitones of 32 index units —
but the index is a **byte offset** into a table of words, so 32 bytes = 16 words per semitone. And `d0` is
always even (`d3` is shifted by 1, the semitone term by 5), which is necessary: a `move.w` at an odd address
would raise an address error on a 68000.

**The table at `0x603D62` confirms it by itself.** Its words:

```
0000 0003 0007 000b 000e 0012 0016 001a 001e 0021 ...
```

ascending up to `03f8`, then **entry 192 comes back to `0000`**. So 192 words for an octave, 16 per semitone
— the same number as the code's modulo `0x180`, obtained by an independent path. And `0x3F8` fits just under
`FNS`'s 10 bits.

The final assembly is exactly the register's layout: `d2`'s high nibble shifted by 7 lands on bits 11-14
(`OCT`), the octave correction is `±0x800`, that is one step of that field, `FNS` occupies bits 9-0, and the
mask `0x7BFF` clears bits 15 and 10, the two unused ones.

### Two formulas for the pitch, as for `TL`

`0x603928` also writes `$10(a5)`, by an altogether different computation — velocity × signed sensitivity plus
a base pitch, then `(x<<1 + 0x4000) & 0x7800` for the octave (§5 bis octies). **Two writers of the pitch
register, two formulas**, called from two different places: `0x600CA4` for this one, `0x6037F2` and
`0x60388E` for the other. The same doubling exists on `TL`. Why two paths, and which serves when, is **not
established**.

### And one more field named

The voice record's `+0x14` is set at note on from the channel object's `+5`
(`0x600C60 : move.b $5(a0), $14(a4)`), and it is **added to the semitone count** here. So a channel object's
`+5` is a **transposition**. The record's `+0x0C` is a second one, additive as well.


## 5 bis octies. `0x603928`: the velocity, and the two things it commands

126 bytes, called from `0x6037F2` and `0x60388E`, both in the note on path. `A2` is the instrument record,
`A4` the voice record, `A5` the slot's registers. The input `D0` is the **velocity**, kept in `D3` to serve
twice.

### First use: the level, through a breakpoint curve

```
0060392A  movea.l a2, a3 ; lea.l $6(a3), a3 ; moveq #$3, d1
00603932  cmp.b  (a3), d0 ; bge -> 603946        ; past the point : next segment
00603936  sub.b  -$4(a3), d0                     ; minus the current point's abscissa
0060393A  mulu.w $2(a3), d0 ; lsr.w #$8, d0      ; times the slope, / 256
00603940  add.b  -$3(a3), d0                     ; plus its ordinate
00603946  lea.l  $4(a3), a3 ; dbra d1, $603932
0060394E  move.b -$3(a3), d0                     ; past the last point : the final ordinate
00603952  move.b d0, $a(a4)                      ; THE VOICE RECORD'S +0x0A
```

**Five points of four bytes at `+2` of the instrument record** — abscissa, ordinate, and a word — so `+2` to
`+0x15`, four linearly interpolated segments. The segment from `k` to `k+1` takes point `k`'s abscissa and
ordinate and **point `k+1`'s word** as the slope; past the last point, the value is flat.

And the result goes into the **voice record's `+0x0A`** — precisely the byte that applying CC 7 multiplies
by the channel's volume (`move.b $a(a4), d6 / mulu.w d0, d6`, §5 quater). So the volume chain is complete:
velocity → the instrument's curve → `+0x0A` → × the channel's volume → `TL`.

But the two paths to `TL` are **not the same formula**, and that is worth noting for fidelity:

```
00603956  lsr.b #$1, d0                          ; 0..127
00603958  movea.l $608028.l, a3 ; adda.w $2(a3), a3
00603962  move.b (a3, d0.w), d0                  ; A TABLE IN ROM : level -> value
00603968  move.b $5(a0), d1 ; mulu.w d0, d1
0060396E  lsr.l #$8, d1 ; addq.w #$1, d1
00603972  not.b d1 ; lsr.b #$1, d1
00603976  move.b d1, $d(a5)                      ; TL
```

At note on, the level goes through **a table in ROM** (`[0x608028]` plus its own word `+2`) before the
multiplication, and there is an `addq.w #$1` that CC 7 does not have. CC 7 multiplies directly. Two formulas
for the same register, depending on whether a note is being turned on or the volume moved. **The current
port has nothing to do with it** — it runs this code on an emulated 68000; it is a standalone core that
would owe both (§5 ter).

### Second use: the pitch

```
0060397A  move.l d3, d0                          ; the velocity, again
0060397C  move.w $1c(a2), d1 ; muls.w d1, d0     ; times the sensitivity -- SIGNED MULTIPLY
00603982  lsr.l #$8, d0
00603984  add.w $16(a2), d0                      ; plus the instrument's base pitch
00603988  move.w d0, $ede.w                      ; filed at 0xEDE
0060398C  move.w d0, d1 ; lsl.w #$1, d1
00603990  addi.w #$4000, d1 ; andi.w #$7800, d1  ; the OCT field
00603998  andi.w #$3ff, d0 ; or.w d1, d0         ; the FNS field
0060399E  move.w d0, $10(a5)
```

A slot's `+0x10` is **`OCT`/`FNS`**, the pitch: `OCT` on bits 14-11, signed, and `FNS` on bits 9-0. So the
internal value is a number whose high bits give the octave and whose low ten give the fraction, and the
`+0x4000` before the mask is the **bias** that converts the signed octave into the register's encoding.

**So the velocity also modulates the pitch**, through the instrument's `+0x1C` — a signed sensitivity, hence
the `muls`. And `0xEDE` keeps the computed pitch: it is what `0x600C50` copies into the voice record's
`+0x12` (`move.w $ede.w, $12(a4)`), seen in §5 quater without knowing where it came from.

### What that names in the instrument record

| | |
|---|---|
| `+2` to `+0x15` | **five points of the velocity curve**, 4 bytes each: abscissa, ordinate, slope (word) |
| `+0x16` | the **base pitch** (word) |
| `+0x1C` | the **sensitivity of the velocity on the pitch** (signed word) |

`+1` was already known (the modulation byte, §5 quater) and `+0` carries a flag CC 1 tests.


## 5 bis septies. `0x602FA0`: "reset the sound", and two parallel sets of globals

176 bytes, **entry 1 of table 4** — the sub-commands of the control bank `0x00`. No instruction names its
address: it is reachable only through that table, which is exactly why the walker did not see it before it
was taught the jump tables.

What it does, in order, at `sr = 0x2200`:

```
00602FA4  cmpi.b #$80, (a3) ; beq -> 602fba       ; variant : if 0x80, the tracks are SPARED
00602FAA  bsr.w  $603198
00602FAE  lea.l $300.w, a4 ; 32 longs cleared     ; otherwise, the eight tracks are razed
00602FBA  lea.l $400.w, a4 ; [0xC10] = 0 ; bsr $603050     ; chip 0's 32 voices
00602FC8  lea.l $200400.l, a4 ; [0xC10] = 0x200000 ; bsr $603050   ; chip 1's
00602FDA  lea.l $c40.w, a4 ; 20 x andi.b #$bf, $0(a4)      ; bit 6 cleared on every channel object
```

and `0x603050`, per chip: for each of the 32 records whose `+4` is non-zero, `bsr 0x6006DC` — the voice stop
— then `clr.l $ee0(a4)` and **eight longs cleared at `0xF80`**.

### The globals exist in two copies, one per RAM

The tail reloads the DSP on both chips, and that is where it shows:

```
00602FF4  move.b $ef1.w, d0                 ; chip 0's preset
00602FF8  move.b $ef0.w, $ef4.w
00603002  lea.l $100700.l, a3 ; adda.l $c10.w, a3 ; bsr $602a9e    ; the DSP area loader
00603010  move.b $ef4.w, d0 ; bsr $602b32                           ; the parameter interpreter
0060301E  move.b $200ef1.l, d0              ; CHIP 1's PRESET, in ITS RAM
00603024  move.b $200ef0.l, $ef4.w
0060302C  move.l #$200000, $c10.w
00603034  lea.l $100700.l, a3 ; adda.l $c10.w, a3 ; bsr $602a9e
00603046  move.b $ef4.w, d0 ; bsr $602b32
```

`0xEF1` for chip 0, **`0x200EF1` for chip 1**: the driver keeps **two sets of globals at the same offset,
one in each SCSP RAM**, and `[0xC10]` — 0 or `0x200000` — picks which. That is the reason for all the
`$ef0(a5)`, `$eea(a5)` and `lea $3000.w / adda.l a5` met elsewhere: they are not arbitrary bases, they are
**the same structure in one or the other RAM**. And `0xEF4` serves as *unqualified* scratch, so always in
chip 0's RAM, to pass the value to the callee.

That cross-checks §5 ter's correction of `A5` directly: A5 is not a global base, it carries `[0xC10]`, and
that is why.

### The three stops, and two categories of voice

`0x602FA0` is not alone: **entries 1, 2 and 3** of table 4 are three stops of different granularity, all
guarded by the same `cmpi.b #$80, (a3)`.

| entry | tracks cleared | voice stop predicate | and besides |
|---|---|---|---|
| **1** `0x602FA0` | all 8 (`0x300`, 32 longs) | `0x603050`: `+4 ≠ 0` | **DSP reloaded** on both chips |
| **2** `0x603080` | the **master only** (`0x300`, 4 longs) | `0x603102`: `+4 ≠ 0` **and** `+0xB` bit 7 **clear** | `0xF80` cleared on both chips, master volume reapplied (`0x6027B2`) |
| **3** `0x60311E` | the **seven others** (`0x310`, 28 longs) | `0x60315A`: `+4` **bit 3 set** | — |

And the predicates are not arbitrary: **a note on files the voice into one of two categories**, and it is
written in black and white by two parallel paths that differ only in that.

```
00600BFE  move.b $ee1(a3), $b(a4)     ; category A : the counter 0xEE1
00600C04  ori.b  #$80, $b(a4)         ;   and bit 7 of the +0xB FORCED
00600C0A  addq.b #$1, $ee1(a3)
00600C0E  bset.b #$3, $4(a4)          ;   and bit 3 of the +4

00600C3C  move.b $ee0(a3), $b(a4)     ; category B : the counter 0xEE0
00600C42  addq.b #$1, $ee0(a3)        ;   bit 7 not set, bit 3 not set
```

| | `+0xB` bit 7 | `+4` bit 3 | counter | stopped by |
|---|---|---|---|---|
| category **A** | **set** | **set** | `0xEE1` | entries 1 and **3** |
| category **B** | clear | clear | `0xEE0` | entries 1 and **2** |

So bit 7 of the `+0xB` is a **deliberate marking** (`ori.b #$80`), not an overflow of the counter — which
had to be checked before relying on it.

**And the two categories pair up with the splitting of the tracks**: entry 2 clears the master track and
stops category B; entry 3 clears the seven others and stops category A. So track 0 ↔ B, and tracks 1-7 ↔ A.
That is **the music/effects split seen from a third angle**, after `group_of()` and the channels `0x0F-0x13`.
Which one is the music is **not established by this alone**; that `snd_irq2` treats track 0 apart, with its
three timing hooks, points towards "track 0 = the music" — but that is an indication, not a measurement.

`0x603172`, called by entries 2 and 3 before each stop, cleans two things:

```
00603174  move.b $6(a4), d5 ; bmi -> rts    ; the +6 : the MIDI channel, 0xFF = unused
0060317E  lsl.w #$1, d5 ; clr.w (a2, d5.w)  ; a2 = 0xF80 : ONE WORD PER CHANNEL
00603186  move.b $5(a4), d5 ; lsl.w #$5, d5 ; the +5 : the object index, x 0x20
00603190  andi.w #$7f18, $8(a0, d5.w)       ; and the object's word +8 is masked
```

Two things follow from it. **`0xF80` is indexed by MIDI channel × 2, in words** — 16 channels, 32 bytes — and
not "one byte per slot" as I had first supposed on seeing that it follows the `0xF00` table. The two tables
are neighbours and **do not have the same index**: `0xF00` is per slot (§4 bis). And the mask `0x7F18` on the
LFO register's image keeps `LFOF` and the two waveforms, but clears `LFORE`, `ALFOS` and above all
**`PLFOS`**: stopping a voice **puts the modulation depth back to zero** for its channel object, which
cross-checks §5 quater from the other end.

Finally `0x603198`, called by entries 1 and 3: if the configuration long `[0x60803C]` is not negative, it
clears the `+0` of the eight entries at `0x380` (step `0x10`) and `0xEFC`.

### And it links the two halves of the DSP already read

`0x602A9E` then `0x602B32`, in that order, twice. The second is §5 bis bis' parameter interpreter; the first
is the DSP area's loader, next door to the `0x602AD6` §5 had read as the one-stroke wipe from `0x700` to
`0xBFF`. So the complete sequence of an effect change is: **load the area, then apply the descriptors** — and
the 59 addresses enumerated in §5 bis bis are the second step, not the whole of it.


## 5 bis sexies. `0x60288A`: the sample loader, and its undo log

470 bytes, **the program's largest function after `snd_reset`**, and never cited by this document before
2026-10-25 — it is the progress measurement that pointed at it. Called from `0x602DBA` and `0x602DD6`, two
control commands, plus a `bra` from `0x602880`. It runs at `sr = 0x2200`.

**What it does: "load sample set `D0`".** And since it *replaces* the previous set, it starts by undoing it.

### The two tables

| | |
|---|---|
| `0x3000` + `[0xC10]` | a **header of two words** — `+0` the base index, `+2` the maximum index — then the **sample records** from `0x3004` on, `0x0C` bytes: `+0` the address (24 bits) with the bank bits and a bit-7 marker in the high byte, `+4` the length, `+8` a running total |
| `0x200` + `[0xC10]` | the **undo log**, one long per replaced record |
| `[0xEEA + offset]` | the **number of records to undo**, and it caps the `dbra` |
| `[0xC18 + offset]` | the set currently loaded — if `D0` already equals it, the function exits at once |

### Undo, then do

The first sweep (`0x6028B6`) looks for the marked records — `tst.b (a2) / bmi`, step `0x0C` — and for each
one puts the log's long back in place of the current one, correcting the running total in both directions:

```
006028C2  move.l (a1, d2.w), d1          ; the old value, from the log
006028C6  move.l $0(a2), d3              ; the value in place
006028CA  move.l d1, $0(a2)              ; restore
006028CE  bclr.b #$1c, d1 ; add.l d1, $8(a2)
006028D6  bclr.b #$1f, d3 ; sub.l d3, $8(a2)
```

Then the loading. `[0x608020]` is the root of a table in ROM, bounded by its own word `+0`
(`cmp.w (a1), d0 / bgt -> exit`, the same convention as this driver's three other tables), and the set's
record gives a count masked to `0x3F` — **64 samples per set at most**. The destination starts at
`[0x608034] + [0xC10]` and advances by the length rounded up to even: **an allocator with a climbing
pointer** in the sound RAM.

For each sample in the set:

```
0060292E  move.l (a3, d1.w), d2 ; move.l d2, (a2)+   ; the ROM source, and it is LOGGED
00602934  andi.l #$ffffff, d2                        ; 24 bits : the address in the window
0060293A  move.b $ef9.w, d6 ; andi.b #$f, d6
00602942  or.b $608068.l, d6
00602948  or.b (a3, d1.w), d6                        ; the record's bank bits
0060294C  move.b d6, $400001.l                       ; THE BANK REGISTER
00602952  move.l d0, (a3, d1.w) ; bset.b #$7, ...    ; the record now points at RAM, and is marked
0060295C  addq.b #$1, $eea(a5)                       ; one more to undo
00602960  sub.l d2, $8(...) ; add.l d0, $8(...)      ; the relocation running total
```

**The bank byte is composed from three sources**: `[0xEF9] & 0x0F`, a constant in ROM at `[0x608068]`, and
the record's own high bits. It is set again **for every sample**, not once per set — because two samples of
one set can be in two different halves of the 16 MB.

### The copy, and its arithmetic

```
00602980  move.l d6, d5 ; lsr.l #$8, d5               ; d6 = the length ; d5 = length >> 8
00602984  [16 x : movem.l (a0)+,d1-d4 / movem.l d1-d4,(a4) / lea $10(a4),a4]
00602A44  dbra d5, $602984                            ; one round = 256 bytes
00602A48  andi.l #$ff, d6 ; lsr.l #$1, d6
00602A50  move.w (a0)+, (a4)+ ; dbra d6               ; the tail, word by word
```

**Sixteen steps of `movem.l` over four registers, so 16 bytes per step and 256 bytes per round of the
loop**, with no test between the steps — measured by counting the identical patterns: exactly 16, 192 bytes
of block. This is **not** a Duff's device like `snd_reset`'s four: here the remainder is handled separately,
word by word, `(length & 0xFF) / 2` times. The block's 192 bytes are 41 % of the function.

### What it establishes, and what it does not say

The records at `0x3000` are **rewritten in place**: before the loading `+0` is the address in the sample ROM,
afterwards it is the address in the sound RAM. The log at `0x200` exists precisely so that the next set can
return the records to their ROM state. It is a reversible substitution, and `[0xEEA]` counts what there is to
return.

Not to be confused with the **32 descriptors at `0x1000`** that `A6` walks in steps of `0x20` (§2): these are
two distinct tables, of different step and size. The link between the two is not established.

**The header of two words was found through the consumers, not here.** `0x60288A` does `tst.l (a2)+` and
`tst.l (a3)+` before its loops — four bytes skipped, without saying why. It is `0x600B4C` and `0x6037FA` that
name it, doing the same thing in the other direction:

```
00600B54  cmp.w $2(a1), d1 ; ble -> 600b6a     ; d1 beyond the maximum : refused
00600B6A  sub.w (a1), d1                       ; minus the base index
00600B6C  tst.l (a1)+                          ; the header is skipped
00600B6E  mulu.w #$c, d1 ; adda.l d1, a1       ; the record
00600B74  move.l a1, $ed0.w                    ; 0xED0 : THE CURRENT RECORD
00600B78  move.b $0(a1), $ed6.w ; bmi -> rts   ; its flag byte, bit 7 = already in RAM
```

So a **global sample number** becomes a local index by subtracting the base, and the bit 7 `0x60288A` sets is
what the consumer tests to know whether the sample is already in RAM. `0xED0` is the selected record and
`0xED6` its flags — and `0xED0` is read back at note on (`0x6038E6`:
`movea.l $ed0.w, a2 / tst.b $4(a2) / bmi`). So the chain runs from the loader to the note.

And the `+8`, the running total: it receives `−old +new` at every substitution, so it follows the
**displacement** of the addresses. What it is used for afterwards is **not** read: none of `0x3000`'s five
uses consumes it.


## 5 bis quinquies. `0x600852`: the consumer, and table 1 named at last

Read on 2026-10-25. It is the other end of the ring at `0x800`, and it closes the whole chain.

```
00600852  move.b $ecc.w, d0
00600856  cmp.b  $efa.w, d0 ; bcs -> 600860
0060085C  move.b d0, $efa.w          ; 0xEFA : A WATERMARK, the maximum ever reached
00600860  subq.b #$1, $ecc.w         ; one message fewer pending
00600864  lea.l $800.w, a3
00600868  move.w $ec6.w, d0 ; andi.l #$3ff, d0   ; 0xEC6 : the READ cursor
00600874  addq.w #$4, $ec6.w
0060087A  move.b (a3)+, d0           ; the status byte
0060087C  move.l d0, d5 ; andi.b #$f, d5         ; D5 = THE MIDI CHANNEL, the low nibble
00600882  lsr.b #$2, d0 ; andi.w #$1c, d0
00600888  lea.l $60088e(pc, d0.w), a1 ; jmp (a1)
```

**Three things at once.**

**Table 1 at `0x60088E` is the consumer's dispatcher**, not the incoming MIDI's. I had opened it to Ghidra
calling it "the MIDI status dispatcher" without knowing who reached it. It dispatches the messages **drawn
from the ring**. Its eight slots:

| slot | status | target |
|---|---|---|
| 0 | `0x8n` note off | `0x60120A` |
| 1 | `0x9n` note on | `0x6008AC` |
| 2 | `0xAn` the driver's commands | `0x6025F2` |
| 3 | `0xBn` control change | `0x601402` |
| 4 | `0xCn` program change | `0x601C1A` |
| 5 | `0xDn` | `0x6008AA`, that is **the `rts`: ignored** |
| 6 | `0xEn` pitch wheel | `0x6012D6` |
| 7 | `0xFn` | the `rts` itself, inline |

Seven `bra.w` and one shared `rts` — exactly the form `FV2SoundJumpTables.java` had described for this
table, and that the two tables at `0x60030A` and `0x60376C` repeat.

**`D5` is born here.** The status's low nibble, that is the MIDI channel, is put into `D5` before the jump —
and it is the `D5` that CC 1, CC 7 and CC 10 compare with each channel object's `+1`. So that convention's
provenance is established, and no longer supposed.

**And table 3's size is confirmed a fourth time.** `0x601402`, the `0xBn` branch:

```
00601404  move.b (a3)+, d0
00601406  cmpi.b #$50, d0 ; bgt -> 6014be    ; beyond 0x50 : the << nothing >> handler
00601414  move.w $60141c(pc, d2.w), d2 ; jmp $60141c(pc, d2.w)
```

The bound `0x50` is **written in the code**, and `0x50 + 1 = 81` — the count the self-sizing rule had given
and that the hole's size (162 bytes) had given again. Three independent measurements, plus this one.

### Who calls, and at what rate

Two places only, `0x601CB8` and `0x602160`, with the same shape:

```
00601CAC  move.b #$1, $c30.w
00601CB2  tst.b  $ecc.w ; beq -> 601cc0     ; nothing pending : skip
00601CB8  bsr.w  $600852                    ; ONE message only
00601CBC  clr.b  $c30.w
00601CC0  move.l a6, d0 ; addi.w #$20, d0 ; cmpi.w #$1400, d0
00602174  lea.l  $1000.w, a6                ; and it wraps back to 0x1000
```

**One message per pass**, interleaved with a walk of `a6` over `0x1000` to `0x13FF` in steps of `0x20` —
that is **32 structures of `0x20` bytes at `0x1000`**, the bounds being written in the `cmpi.w #$1400` and
the `lea $1000.w`. That is the main loop, round-robin.

**So the complete chain, from the data to the hardware:**

```
tracks at 0x300 -> the timer A tick decodes -> ring at 0x800 (256 longs, 0xEC4 writes, 0xECC counts)
   -> 0x600852 (0xEC6 reads) -> table 1 by status -> 0x601402 -> table 3 by controller
   -> CC 1 / 7 / 10 -> channel objects at 0xC40 -> voice records at 0x400 -> slot registers
```

And the back-pressure makes sense: the consumer draws **one** message only per round of the main loop, while
the timer A tick can pile up several per interrupt. Hence the `cmpi.b #$fc, $ecc.w` that stops production at
252 out of 256.

### Two details

`0xEFA` is a **watermark**: the maximum `0xECC` has reached. It is written and read **only in `0x600852`** —
no other use in all the code reached. It is a trace left by the author, dead as far as the program is
concerned.

`0xC30` is set around the draw and **cleared by `snd_irq2`** (`0x6033B0`). So the two talk to each other
through that byte; what exactly it protects is not established.

### What it leaves open

The six handlers of table 1 I have not read: note off, note on, the `0xAn` commands, the program change, the
pitch wheel.

The 32 structures at `0x1000` that `A6` walks are **not** open: §2 has already named them — the **sample copy
descriptors**, `0x20` bytes each, `0x1000-0x13FF` and `0x201000`. The bounds I have just picked up in the
loop (`cmpi.w #$1400`, `lea $1000.w`) give them again from another place in the code, which is one more
confirmation.

### And why table 1 ignores `0xFn`

Because system messages **never** pass through the ring: they are intercepted on reception. §3 ter had read
it in `snd_irq3_reception_midi` — `cmpi.b #$ff, d0 / beq -> 0x6006C4`, and `0x6006C4` puts the stack back to
`0x2000` and writes `"Erro"` at `0xC08`, which is to say **restarts the board**. So the `0xFn` the consumer
throws away is not an oversight: nothing puts one there. The two readings, made two days apart and by two
paths, cross-check.


## 5 bis quater. The eight tracks at `0x300`: a standard MIDI file reader

Read on 2026-10-25, following on from `snd_irq2`. The eight structures the tick walks are **eight sequencer
tracks**, and the driver is a standard MIDI format reader — variable-length delta-times, running status, and
a chaining list per track.

### The size, confirmed three times

`0x10` bytes per track, `0x300` to `0x37F`, and the three measurements share nothing:

| | |
|---|---|
| `snd_irq2` | seven `lea $10(a2), a2` between the calls, so a step of `0x10` |
| `0x602712` | `moveq #$3, d7 / clr.l (a4)+ / dbra` — **four longs**, the clearing of one track |
| `0x6036EE` | `lea $300.w, a0 / moveq #$1f, d0 / clr.l (a0)+` — **32 longs = `0x80` bytes**, that is all eight |

### The layout

| | | |
|---|---|---|
| `+0` | flags | bit **7** activates (`bset` at `0x602706`, `bclr` at `0x603202` for track 0); bits **6 and 5** supplied by the start command (`andi.b #$60, d0 / or.b d0, $0(a4)`); bit **3** "the track has a chaining list"; bit **0** "another event follows immediately" |
| `+1` | **running status** | the remembered MIDI status, for running status |
| `+2` | word | the **countdown** to the next event |
| `+4` | long | the **read pointer** into the data |
| `+8` | long | the **cursor in the chaining list** |
| `+0xC` | — | `+0xF` is **a group nibble**, set by `0x6026AE` which sweeps `0x310` over seven tracks — §2 had already noted it. `+0xC` to `+0xE` remain unidentified |

### Running status and the delta-time, as they stand

```
0060347A  move.b (a3)+, d1 ; bmi -> 603484     ; bit 7 set : a new status
0060347E  move.b $1(a2), d1 ; subq.l #$1, a3   ; otherwise : RUNNING STATUS, the byte is given back
00603484  move.b d1, $1(a2)                    ; and it is remembered
00603488  lsr.b #$2, d0 ; andi.w #$1c, d0
00603490  lea.l $603496(pc, d0.w), a0 ; jmp (a0)
```

That is **MIDI's running status** (a data byte reuses the previous status), and the `jmp` is `0x603494`'s —
the table of eight slots I had opened the day before without knowing what it dispatched.
`(status >> 2) & 0x1C` takes bits 6-4 of the status and multiplies them by four, so the eight families `0x8n`
to `0xFn`. Slots 1 and 3 share `0x60352C`: those are `0x9n` and `0xBn`, the two **three-byte** messages, and
a single routine assembles them. `0xAn` has its own handler (`0x6034B6`), which examines globals of its own —
**so it is not aftertouch but the driver's command channel**, what the reading list called "the
`A0 bank index` protocol".

And the common tail reads the delta-time:

```
00603664  btst.b #$0, $0(a2) ; bne -> 603464   ; flag set : next event immediately
00603670  move.b (a3)+, d1 ; beq -> 603464     ; delta zero : likewise
00603676  bclr.b #$7, d1 ; beq -> 603680       ; bit 7 set : a second byte follows
0060367C  lsl.w #$7, d1 ; or.b (a3)+, d1       ; seven more bits
00603680  move.w d1, $2(a2) ; move.l a3, $4(a2)
```

**The standard MIDI format's variable-length quantity**, bounded to two bytes — so 14 bits, 16 383 ticks at
most. And the two return paths to `0x603464` are what allows chords: several events in the same tick.

### The chaining list: a little program per track

When a sequence ends, if bit 3 is set, `+8` is a cursor into **a list of longs**, and each long is an
instruction:

| value | what it does |
|---|---|
| `0xFFFFFFFF` | **end**: `clr.b $0(a2)`, the track stops |
| `0xFFFFFFF1` | **indirection**: `movea.l (a0), a0` — a jump elsewhere in the list, and that is how a piece **loops** |
| `0xFFFFFFF2` | **stop everything**: the 32 longs at `0x300` cleared, all eight tracks with them |
| `0x80xxxxxx` | an **inline command**: `d1 = long << 8 \| 0x80`, pushed into the queue |
| anything else | the **next sequence** to play |

And without bit 3, the end of a sequence simply does `clr.b $0(a2)`.

### And the loop closes on `snd_irq2`

`0x6036FC`, called by the event handlers, **does not act**: it pushes.

```
006036FC  lea.l $800.w, a1
00603702  move.w $ec4.w, d0 ; andi.w #$3ff, d0  ; a ring of 0x400 bytes
0060370A  adda.l d0, a1 ; move.l d1, (a1)       ; the four-byte message
0060370E  addq.w #$4, $ec4.w                    ; the write cursor
00603712  addq.b #$1, $ecc.w                    ; THE NUMBER PENDING
```

That is **the command queue at `0x800`** — a ring of `0x400` bytes, that is **256 longs**, with `0xEC4` as
the write cursor and **`0xECC` as the pending count**. The consumer is elsewhere: `0x600852` reads `0xECC`,
`0x600860` decrements it.

**And that explains `snd_irq2`'s `0xFC`.** The timer A handler starts with
`cmpi.b #$fc, $ecc.w / bcc -> rte`: it **stops producing when the queue is almost full**, 252 out of 256. The
three numbers fit together — a ring of `0x400` bytes, mask `0x3FF`, threshold `0xFC` — and none of them was
supposed.

So the driver's architecture fits in one sentence: **the timer A tick decodes the sequences and piles
messages into the ring at `0x800`; a consumer outside the interrupt empties it.**

### What it leaves open

A track's four bytes `+0xC`, which no code reached touches. The role of flag bit 6, supplied by the start
command next to bit 5, which I know copies to the MIDI output. The queue's consumer, for its part, was read
right afterwards: see 5 bis quinquies.


## 5 bis ter. `snd_irq2` (`0x6032D2`): the music's tick, and a queue of deferred ticks

Point 3 of the reading list. Read on 2026-10-25.

### First a correction: I had the timers the wrong way round

The previous section, written the same day, said of `SCIEB = 0x00C8`: "MIDI input, **timer B and timer C**
armed; timer A is **not**". **That is false, and it is the other way round.** What revealed it: `snd_irq2`
reloads register `0x418` — TIMA, timer A — and acknowledges `SCIRE = 0x40`, bit 6. A handler that reloaded
timer A without timer A being armed would make no sense.

The bits, taken from the port's implementation rather than from my memory of a table
(`src/sound/scsp.cpp`): `timerA_cb()` sets `0x40` in SCIPD, `timerB_cb()` sets `0x80`, `timerC_cb()` sets
`0x100`, and the MIDI input path sets `8`. So:

| bit | source | armed by `0x00C8`? | level according to SCILV `0x88`/`0x48`/`0x00` |
|---|---|---|---|
| 3 (`0x08`) | MIDI input | **yes** | `0b011` = **3** |
| 6 (`0x40`) | **timer A** | **yes** | `0b010` = **2** |
| 7 (`0x80`) | **timer B** | **yes** | `0b001` = **1** |
| 8 (`0x100`) | timer C | **no** | — |

The levels I had computed — 3, 2, 1 — were right; only the sources' **names** were permuted. So **`snd_irq2`
is timer A's handler**, and it is the music's tick.

And the value confirms it from the outside: the handler reloads TIMA from `[0x60806A]`, which is **`0x00CE`**
— exactly the first of `0x6002D4`'s ten words, and exactly what the port's comment (`scsp.cpp`, line 372)
had already noted: "timer A at 0xCE: 50 samples, 882 a second". Three independent sources on the same value.

### A tick that can be stretched or compressed, once

```
006032D8  move.w $60806a.l, d0      ; 0x00CE, the nominal value
006032DE  tst.b  $c3c.w             ; zero : nothing
006032E6  subi.w #$30, d0           ; if positive -> 0x009E
006032F4  addi.w #$30, d0           ; if negative -> 0x00FE
006032F8  move.w d0, $100418.l      ; TIMA
006032FE  move.w #$40, $100422.l    ; SCIRE : acknowledges timer A
```

`0xC3C` is written in two places only, `0x602F24` (`#$1`) and `0x602F2C` (`#$ff`), both in the control
commands' area — so they are **two commands**, and the handler clears the byte, so the effect lasts **one
interval only**. The figures, since the count runs up to `0xFF`:

| `0xC3C` | TIMA | prescaler rounds | effect on the interval |
|---|---|---|---|
| 0 | `0xCE` | 50 | nominal |
| 1 | `0x9E` | **98** | almost twice as long |
| `0xFF` | `0xFE` | **2** | twenty-five times shorter |

An asymmetry of that size is not a tempo setting. It looks like a synchronisation with the main board — "skip
a beat" on one side, "call me back at once" on the other. **I do not name it**: what is measured is the two
values and the fact that the effect is one-shot.

### What really counts: the ticks are never lost

```
00603306  tst.b $c01.w ; bne -> 6033BE     ; already inside ?
0060330E  cmpi.b #$fc, $ecc.w ; bcc -> rte ; saturated : only the timer is reloaded
00603318  move.b #$80, $c01.w              ; the lock is taken
...
00603368  tst.b $c1e.w ; beq -> 603374
0060336E  subq.b #$1, $c1e.w ; bra 603322  ; a deferred tick : round again
...
006033BE  addq.b #$1, $c1e.w ; rte         ; THE REENTRANT PATH
```

`0xC01` is a **reentrancy lock**, and `0xC1E` a **counter of deferred ticks** — its only three uses in the
whole program are these three lines. When the interrupt comes down again while the handler is still running,
it **executes nothing: it increments `0xC1E` and exits**. Then the main loop goes round once more for each
pending tick before handing back.

**So the driver never loses a tick; it queues them and catches up within a single invocation.** That matters
for the port: if the host's scheduling makes the handler overrun, the real driver catches up instead of
dropping notes. The only case where a round really is skipped is `0xECC >= 0xFC` — which is, measured from
`0x6036FC`, **the number of commands pending in the ring at `0x800`**. That ring holds 256 longs (`0x400`
bytes, mask `0x3FF`), so `0xFC` = 252 is a **back-pressure** threshold, not a magic number. See 5 bis quater.

The round's body, between `sr = 0x2700` (everything masked) and `sr = 0x2000`: `a2 = 0x300`, then seven
`lea $10(a2), a2` each followed by a `bsr 0x60344A`. So **eight structures of `0x10` bytes at `0x300-0x37F`**,
the first handled apart (`0x6033C8`, `0x603408`, `0x603460` according to three flags) and the seven others by
the same routine. These are the **eight sequencer tracks**, read right afterwards: see 5 bis quater, which
also gives what `0xECC` counts and why the threshold is `0xFC`.

### And a return path the port throws away

```
00603322  tst.b $c3e.w ; beq -> 60333c
00603328  subq.b #$1, $c3d.w ; bne -> 60333c
0060332E  move.b #$a, $c3d.w
00603334  move.b #$f0, $100407.l            ; MOBUF
```

`0x407` is the low byte of register `0x406`, that is **MOBUF, the MIDI output buffer**. When `0xC3E` is set —
another command, `0x602F16` and `0x602F1E` — the driver **sends `0xF0` to the main board every ten ticks**,
that is about 88 times a second.

And in the port, `src/sound/soundboard.cpp` line 133: `h.midi_out = [](int) {};`. **The output is thrown
away.** If the main board is waiting for that byte, it would show; if it does not read it, it costs nothing.
**Which of the two is not measured** — it would mean seeing whether the PowerPC code reads the sound board's
return port.


## 5 bis bis. `0x602B32`: the DSP's parameter interpreter, and the 59 addresses it writes

Point 1 of the reading list — the function containing `0x602BA2`, the COEF/MADRS loading. Read on
2026-10-25, without Ghidra. It is not a block loader: it is a **descriptor-driven interpreter**.

### How the block is reached

`0x602CE4` supplies the block, and the indexing takes three hops:

```
00602CE4  movea.l $608018.l, a1        ; the root of the effects table
00602CEC  bmi -> give up               ; negative pointer : exit by popping
00602CF0  move.b  $ef1(a5), d1         ; 0xEF1 : THE CURRENT PRESET
00602CF4  bmi -> give up
00602CF6  lsl.w   #$1, d1
00602CF8  move.w  $2(a1, d1.w), d1 ; adda.l d1, a1    ; the preset's record
00602CFE  move.w  (a1), d1         ; adda.l d1, a1    ; the parameter block
```

`[0x608018]` is `0x0060E954`, and its word `+0` is **2**. That is not a count but **the maximum index** — the
sister routine `0x602D08` confirms it by doing `cmp.w (a0), d0 / bgt -> give up` on the same layout. So
**three presets, 0 to 2**. And it is verified from the other side: following the chain for indices 0 to 15,
**only 0, 1 and 2** give a block whose types are all known; from 3 on one lands on `entries=255`,
`step=255`, nonexistent types. The two measurements — the table's word and the blocks' validity — meet on
three.

### The block's layout, and the seven types

```
+0  number of entries       +4  maximum sub-index          +5  bytes per set of values
+8  the descriptors, FOUR bytes each : +0 type and flags, +2 COEF offset, +3 MADRS offset
```

The stream of values starts at `(number + 2) × 4` from the beginning — that is after the 8-byte header and
the descriptors — and the sub-index `d0` chooses a **set** within it (`a1 += d0 × step`). So the sub-index
changes the values, **not the addresses**.

The word counts per type, picked up one by one between `0x602B64` and `0x602C8C`:

| type (`& 0x1F`) | COEF words | MADRS words |
|---|---|---|
| `0x01` | 1 | — |
| `0x02` | 10 | 7 |
| `0x04` | 3 | 2 |
| `0x0D` | 2 | — |
| `0x0E` | 5 | — |
| `0x10` | 5 | — |
| `0x16` | **38** | 1 |

And **bit 5 of the type** is not a type: it adds `$2(a3)` to every MADRS value written
(`btst.b #$5, $8(a2)` / `move.w $2(a3), d4` / `add.w d4, d0`). That is a **relocation of the delay lines**: a
preset's delay addresses are offset by a base. The type `0x24` observed in the data is type `4` with that
flag. When a type writes no MADRS word, the descriptor's byte `+3` is `0xFF` — a marker, not an offset.

### The 59 addresses, enumerated from the data

Every write is **bounded and addressed by a descriptor's offset**: no linear sweep in this function. So the
set of addresses touched can be computed from the ROM, and it has been — the three blocks, 32 descriptors in
all:

```
59 register addresses written, from 714 to 794
inside 0x7C0-0x7FF : 0
```

**None.** The highest is `0x794`, and MADRS runs to `0x7BF`. The interpreter that sets the effect never names
`0x7C0-0x7FF`.

**That is a second line of evidence on the fidelity question, independent of the first.** Section 5 had
settled it by the sweep: a single linear pass from `0x700` to `0xBFF` from a block in ROM, so a hardware
mirror would destroy every preset during its own loading. This one says something else: the only other way
by which the driver touches COEF and MADRS **never addresses the disputed range**, and it does so in an
enumerable rather than a deduced way. The two together: `0x7C0-0x7FF` is written by this driver only by the
sweep that puts the whole area to zero.

What it still does not say: what the **hardware** does with those 64 bytes. That stays out of the reach of
reading the program.

### And as for the arenas' question

`0xEF1` chooses among **three** sets of COEF/MADRS parameters. Not to be confused with the **9 distinct MPRO
programs** measured on `fv2-fight.txt`: MPRO is the microcode, these three are the coefficients and the
delay addresses. Point 5 of the reading list concerns MPRO and stays open; what is settled here is that a
single byte of RAM selects the parameter set, and that there are only three.


## 5 quater. The MIDI control changes: modulation, volume, pan

Read on 2026-10-25, **without Ghidra** — with the walker and the bytes. The three handlers jump table 3
(`0x60141C`) dispatches by controller number: CC 1 (`0x601882`), CC 7 (`0x601974`), CC 10 (`0x601A08`).

### The two stages, and the two keys

All three have the same shape, and it states the driver's architecture:

1. **sweep the 20 channel objects** at `0xC40`, `0x20` bytes each; keep those whose `+0` is non-zero
   (active) and whose **`+1` equals the message's MIDI channel**;
2. **write the setting into the object**;
3. **sweep the 32 voice records** at `0x400` (then `0x200400` for chip 1), and for those that belong to that
   object, **push the value into the slot's registers**, the record's `+0` being a pointer to them.

And at step 3, **the three do not use the same key**, which is this reading's new fact:

| handler | the record is kept if | what that is worth |
|---|---|---|
| CC 1, `0x60193A` | `cmp.b $5(a4), d1` | **`+5` == the channel object's index** (`(A0 − 0xC40) >> 5`, so 0..19) |
| CC 10, `0x601A5A` | `cmp.b $5(a4), d3` | the same `+5`, the same index computation |
| CC 7, `0x6019D6` | `cmp.b $6(a4), d5` | **`+6` == the MIDI channel** (`D5`, the one the object's `+1` carries) |

So a record carries **two** identifiers: `+5` its channel object's index, `+6` its MIDI channel. Twenty
objects for sixteen MIDI channels: the two cannot be the same numbering. **That is `group_of()`'s missing
third** — and it remains to be finished, because *who sets* `+5` and `+6` is still not read. What is settled:
`+5` is an object index, not a channel.

Another measured asymmetry: **CC 1 applies inside the loop**, for every matching object; **CC 7 and CC 10
apply once after the loop**, on the **last** matching object (`moveq #$ff,d3` as a sentinel, `move.l a0,d3`
at each match, `tst.l d3 / bpl`). If two active objects shared a MIDI channel, the first one's volume and pan
would not be refreshed. Whether that can happen is not established.

### CC 1, modulation wheel: the hardware LFO, not the software table

```
00601882  lsl.b #1,d0 ; andi.b #$e0,d0        ; the 3 high bits of the doubled MIDI value
...
006018E2  move.b $9(a0),d3 ; andi.b #$e0,d3   ; the value in place
006018EC  beq -> rts                          ; unchanged, nothing is touched
006018EE  andi.b #$1f,$9(a0) ; or.b d0,$9(a0) ; otherwise the 3 bits are replaced
006018F8  bsr $60193A                         ; and it is applied
```

and the application writes **`$12(a5)`**, masked by `andi.w #$7fff`. A SCSP slot's `+0x12` is the **LFO**
register: bit 15 `LFORE`, bits 14-10 `LFOF`, 9-8 `PLFOWS`, **7-5 `PLFOS`**, 4-3 `ALFOWS`, 2-0 `ALFOS`. So
the three bits CC 1 sets are **`PLFOS`, the pitch LFO's depth**, and the mask `0x7FFF` leaves `LFORE` at
zero: the LFO is set **without being reset**. The channel object's word `+8` is a ready-made image of that
register, of which `+9` is the low byte.

**What that overturns:** the modulation wheel does **not** go through the software modulation table at
`0xF00`. It goes to the hardware LFO. So bit 7 of a record's `+4`, which I measured as **never set** and
suspected CC 1 of, is not armed by the wheel. Both measurements hold; what arms the software modulation
remains unknown.

The instrument's depth is **added** to it, saturating:

```
006018C4  move.b $1(a1),d3 ; move.b d3,d1     ; the instrument's modulation byte
006018CA  andi.b #$e0,d3
006018CE  cmp.b  d3,d0
006018D0  bcs.b  $6018d4                      <-- the target is the NEXT instruction
006018D4  andi.b #$7,d1 ; lsl.b #$5,d1        ; its 3 LOW bits, moved up
006018DA  add.b  d1,d0 ; bcc ; move.b #$e0,d0 ; summed, saturated at 0xE0
```

`0x6018D0`'s `bcs` has `0x6018D4` as its target, **the instruction that follows it** (`65 02`, displacement
2): both outcomes lead to the same place, so the comparison before it serves no purpose. Two possible
readings — a remnant of an earlier version, or a branch neutralised by changing its displacement, which is a
known way of patching a binary. **I do not know which**, and it does not change the behaviour: the
instrument's depth is always summed in.

The instrument is found by `movea.l $608038.l, a1` then `move.w $2(a1,d3.w), d3` (an offset; negative = no
instrument) then `adda.l d3, a1`.

### CC 7, main volume: two stages and a global

```
00601974  lsl.b #1,d0 ; beq ; addq.b #1,d0    ; 7 bits -> 8 (0x7F->0xFF, 0->0)
00601994  move.b d0,$3(a0)                    ; +3 = the volume as sent
0060199E  btst.b #0,$0(a0) ; bne              ; bit 0 of the flags : exempt from the global
006019A6  move.b $ee9.w,d2                    ; otherwise, the global scale 0xEE9
006019AA  mulu.w d2,d0 ; lsr.w #8,d0 ; addq   ; volume x scale / 256, rounded away from zero
006019B2  move.b d0,$a(a0)                    ; +0x0A = the effective volume
```

then the application, for each record whose `+6` equals the channel **and whose `+4` is non-zero**:

```
006019E8  move.b $a(a4),d6                    ; the RECORD's +0x0A : the voice's own level
006019EC  mulu.w d0,d6                        ; x the channel's effective volume
006019EE  beq -> moveq #$ff,d6                ; zero product -> 0xFF
006019F0  lsr.w #8,d6 ; not.b d6 ; lsr.b #1,d6
006019FA  move.b d6,$d(a5)                    ; TL, the slot's attenuation
```

A slot's `+0x0D` is the low byte of the word `0x0C`, that is **`TL`**. So
**`TL = ¬((channel_volume × voice_level) / 256) / 2`**, which bounds it to **0..127** — half the range —
with `0xFF` reserved for exact silence. It is an integer formula verifiable as it stands against the port.

### CC 10, pan: `DISDL` and `DIPAN` in one byte

The converter `0x601A98` turns the 7 MIDI bits into 5 SCSP bits, and it is exactly the `DIPAN` encoding:

```
00601A98  lsr.b #2,d0                   ; 0..127 -> 0..31
00601A9A  cmpi.b #$10,d0 ; bcc -> 601AAA
00601AA0  neg.b d0 ; subq.b #1,d0 ; andi.b #$1f,d0   ; left half : 0x1F..0x10 (bit 4 set)
00601AAA  andi.b #$f,d0                              ; right half : 0x00..0x0F
```

MIDI 64 (centre) gives 0 on both sides, MIDI 0 gives `0x1F`, MIDI 127 gives `0x0F`. The result goes into the
**5 low bits of the object's `+6`**, and the application composes the final byte:

```
00601A64  move.b $6(a0),d0 ; move.b d0,d1
00601A6A  andi.b #$e0,d0 ; lsr.b #5,d0  ; the 3 high bits : a level 0..7
00601A70  add.b $c04.w,d0               ; offset by the global 0xC04
00601A74  bpl / clr.b d0                ; negative -> 0
00601A7A  cmpi.b #7 / ble / moveq #7    ; clamped to 7
00601A82  lsl.b #5,d0 ; andi.b #$1f,d1 ; or.b d1,d0
00601A8A  move.b d0,$16(a5)
```

A slot's `+0x16` is the **high** byte of the word `0x16`, that is `DISDL` (bits 7-5) and `DIPAN` (bits 4-0).
The channel object's `+6` **is** that byte, up to the `0xC04` offset. And the whole handler is
short-circuited if `0xEFB` is non-zero (`tst.b $efb.w / bne -> rts`).

### The four globals, and where they are set

Found with `--search` **after seeding the jump tables into the walker** — see below, because the same search
previously returned a false count without saying so.

| | what it does | set by |
|---|---|---|
| `0xEE9` | the master volume scale, `0xFF` to start with (`0x600480`) | `0x6027B2`, `0x602D2A`, and copied from `0xEEE` at `0x6033F8` |
| `0xEEE` | a pending copy of the master, taken at **half** (`lsr.b #1`) at `0x60278A` | `0x60278C`, `0x60286A` |
| `0xC04` | the `DISDL` offset, set by a command to `d0 − 0x40` | `0x60276C`; saved/restored at `0x6001AA`/`0x6001E6` |
| `0xEFB` | non-zero ⇒ **CC 10 does nothing**; read in five places, all `tst.b` | **two neighbouring entries of table 4**: 6 (`0x6031B6`) clears it, 7 (`0x6031BC`) sets it to 1 |

Two sub-commands of the control bank `0x00` that only enable or disable the pan, and that then call
`0x602C94` on both chips: **it looks like a mono/stereo switch**. That is not established — what is, is the
measured effect on CC 10.

`0xEE9` and `0xC04` are the two globals the port must look at for `VF3_VOLUMES`: one multiplies every
non-exempt channel's volume, the other offsets every voice's send level.

### Who sets `+5` and `+6`: found, and through the write

Point 2 of the reading list, open from the start and **closed** on 2026-10-25. Both writes exist, and they
are no longer deductions drawn from the readers.

**`+6`, the MIDI channel**, set at note on, in the routine that fills a record (`A4`) from a channel object
(`A0`):

```
00600C50  move.w $ede.w, $12(a4)
00600C56  move.b d2, $7(a4)
00600C5A  move.b $1(a0), $6(a4)     ; <- the object's +1, that is to say its MIDI channel
00600C60  move.b $5(a0), $14(a4)    ; and the object's +5 goes to the record's +0x14
```

**`+5`, the object's index**, in the tail of the same sequence:

```
006010EA  move.w a0, d0
006010EC  subi.w #$c40, d0
006010F0  lsr.w  #$5, d0            ; (A0 - 0xC40) / 0x20
006010F2  move.b d0, $5(a4)         ; <- exactly the computation the handlers use as a key
006010F6  bset.b #$6, $4(a4)
006010FC  ori.b  #$8, $0(a5)        ; bit 3 of register 0x00's high byte : KYONEX
```

**Key and value are computed by the same code**, down to the byte. So what `group_of()` does in the port is
right, and the threshold `0x0F` does bear on object indices.

Three things read out in passing in that tail: `andi.b #$7f, $12(a5)` clears bit 7 of the word `0x12`'s high
byte, that is **`LFORE`** — the LFO is let go at the note's start, which cross-checks CC 1's `0x7FFF` mask;
`bset` sets bits 0 and 6 of the record's `+4`, the flag byte; and `ori.b #$8` on register `0x00`'s high byte
is **`KYONEX`**, so that is where the note started.

### And why the walker returned a false count

`--search ee9` gave **4 uses**. It gives **9**. The difference is not a bug in the search: the recursive
walk stops at a `jmp (An)`, so **everything reachable only through a jump table was invisible** — including
this section's three handlers. The search returned an apparently complete count over half the program.

`tools/sound/disasm_sound.py` now seeds the eleven computed jumps (the four word tables, the four slot tables,
the four unrolled-block entries), which brings the walk from 95 to **184 functions and 4 453 instructions**.
The control: `0x6019A6`, CC 7's read of `0xEE9`, which I had read by hand and that the old walk did not find
— it appears. And `--search` **prints its scope before its results**, because a count returned by an
incomplete walk looks like a complete count.


## 5 ter. The 168 functions are read — and here is what that does not mean

Measured on 2026-10-25, by crossing the list of bodies Ghidra holds with the addresses this document cites:
**168 functions out of 168, 14 488 bytes out of 14 488.** Every function of the sound program has been
opened and described somewhere above.

**The measurement and its method**, so that it can be redone and contested:

```
tools/ghidra/FV2SoundList.java   -> FUNCTION <address> <bytes> and RANGE <min> <max>
then : every 0x6xxxxx address cited in this document is attached to the function
       one of whose RANGEs contains it ; the bytes of the functions thus touched are summed
```

It counts **"cited"**, and that is an upper bound, for three reasons that must be stated:

1. **`snd_reset` weighs 2 730 bytes — 19 % of the program — and is read only in pieces.** The wiping of the
   DSP area, the four unrolled blocks, the initialisation loops, the tail of the note on: yes. The whole
   function, line by line: no. Taking it out of both sides of the ratio, the share read falls to **88 %**,
   and that is the more honest of the two figures.
2. **Citing a function is not explaining it.** Several of the last sixty are described by what they write
   and where, without the field's meaning being established.
3. The measurement says nothing about the **DSP's program** itself, nor about the data: sequences,
   instruments, samples, MPRO microcode.

### What remains explicitly unestablished

| | |
|---|---|
| `0x2000-0x2FFF` | **identified on 2026-10-25**: a **bank of sixteen** single-cycle **waveforms**, 128 words each, thirteen distinct — a sine to within 5, two squares, a constant, nine smooth shapes, three noises. Loaded from ROM into both RAMs, at an address the sample allocator never touches. **Its consumer is measured**: 37 slots point there through their `SA`, one shape out of sixteen, in one arena only — an FM voice whose modulation the driver writes itself. See 4 septies |
| ~~a slot's bytes `0x16` and `0x17`~~ | **established**: `0x16` carries `DISDL`/`DIPAN` (the direct output), `0x17` carries `EFSDL`/`EFPAN` (the send to the DSP) — all four in the same word. My layout was wrong. See 5 quinquies |
| ~~a channel object's `+0x0C`~~ | **established on 2026-10-25**: `IMXL` bits 2-0 (CC `0x2A`) and `ISEL` bits 6-3 (CC `0x2B`), bit 7 unused — the image of a slot's byte `0x15`. The "overlap on bit 7" I had noted came from having read the write masks without reading `D0`'s clamping. See 5 bis undecies |
| an instrument record's `+0x0B` | **`+0x10` and `+0x11` are established**: they feed the `0xF00` table of the software modulation (5 quinquies). `+0x0B` remains written by two controllers and **read back by nothing** |
| ~~`0x600396`, `0x60103C`~~ | **established**: `0x600396` is a decompressor's dictionary of sixteen words, `0x60103C` a release-duration curve the note off reads into a record's `+0x0E`. See 5 quinquies |
| `0xEFE` | **three of the four are established** (5 quinquies): `0xF80` is the per-channel pitch wheel, `0xC3F` a MIDI thru, `0xED8` a reserved-block mask that is zero on this ROM. `0xEFE` is **written once and read nowhere** |
| ~~the Duff's devices' 512 bytes~~ | **established**: a chunk of a sample being **streamed**, the source in the banked ROM, the destination in the `+8` of one of the 32 descriptors at `0x1000`, advanced on each pass. See 5 quinquies |
| `0x6003B6` | the **meaning is established** — it cuts `EFSDL`/`EFPAN`, called by the routine that wipes the DSP area — but **the count of 18 remains unexplained**. With no audible effect: the DSP has just been put to zero (5 quinquies) |
| ~~which of the two voice categories is the music~~ | **measured**: category **B is the music**, **A the effects** — 1 756 against 40, object indices `0x00`-`0x09` against `0x0F`-`0x13`, no counter-example. And so the music is **stolen before** the effects. See 5 quinquies |
| the hardware's behaviour on `0x7C0-0x7FF` | two lines of evidence on what the **driver** does; nothing on the chip. **Out of the reach of any reading**: it would take a Model 3 and a measurement |

### What the reading gave back to the port

Four results that touch the code, and not only the documentation:

1. **`group_of()` is right, and no longer rests on a deduction**: the loop that sets a record's first long is
   `0x60048C`, and the write of the `+5` is `0x6010F2`.
2. **A record's `+6` is the MIDI channel**, which the port did not know — noted in
   `src/sound/soundboard.h`.
3. **`TL` and the pitch each have two formulas**, depending on whether a note is turned on or a controller
   moved.
   > **And this point does NOT touch today's port.** I had written "a port that implements only one of them
   > will be heard": that is false. `src/sound/soundboard.cpp` instantiates an `m68000_device` over the
   > driver's ROM and runs it — **so both formulas run as Sega wrote them**, nothing is reimplemented and
   > nothing can diverge. What this result constrains is **a future standalone core**: the day the SCSP or
   > the driver were rewritten, both paths would be needed. Corrected on 2026-10-25, after checking what the
   > port runs instead of supposing it.
4. **Two MIDI output paths are thrown away** by `h.midi_out = [](int) {}`: `snd_irq2`'s periodic `0xF0` byte
   and the byte-by-byte pass-through at `0x603718`/`0x60361E`. If the main board reads them, it counts.

### And two measurements reconciled

The writes at `+0x600` — the FM buffer — that my probe had counted as **zero** over 4 200 frames of
*Fighting Vipers 2* and 2 400 of *Virtua Fighter 3*, with a positive control to back it up. `0x60155C` is the
code that would make them. It is reached only by eleven specific controllers. **Both facts hold together**:
the feature exists, these scenes do not use it. It was this document's only measurement that seemed to
contradict a reading.


## 5 quinquies. The eight points outstanding, taken one by one

§5 ter listed ten things unestablished; two were closed within the day. Here are the other eight, dealt with
on 2026-10-25. **Six close, two stay partial, one is out of the reach of reading.**

### A slot's bytes `0x16` and `0x17`: my layout was wrong

I noted that "CC 10 writes one, `0x602EB0` the other, both with a pan, and the register layout I have does
not explain both". The error was in my layout. The port's macros (`src/sound/scsp.cpp`) put **all four
fields in the same word** `data[0xB]`, that is offset `0x16`:

| | |
|---|---|
| `DISDL` bits 15-13, `DIPAN` bits 12-8 | the **byte `0x16`** — the direct output |
| `EFSDL` bits 7-5, `EFPAN` bits 4-0 | the **byte `0x17`** — the **send to the DSP** |

So CC 10 sets the **direct** pan (`move.b d0, $16(a5)` with `(level<<5) | (pan & 0x1F)`) and `0x602EB0` the
**effect**'s pan (`andi.b #$e0, $17(a5)`, which keeps `EFSDL`, then the same converter `0x601A98` into
`EFPAN`). Two pans, two destinations, no contradiction. I had placed `EFSDL`/`EFPAN` at register `0x18` from
memory; they are in the low byte of `0x16`.

### `0x6003B6` and its eighteen slots

The meaning is now clear: it clears byte `0x17`, so **`EFSDL` and `EFPAN`** — it cuts the send to the DSP.
And its caller says as much: `0x602AD6`, the routine that wipes the DSP area in one stroke (§5). Cutting the
sends at the same time as the effect is consistent.

**The count of 18 remains unexplained** (`moveq #$11, d7`), and none of the three polyphony ceilings is 18 —
they are all `0x20`. What can be said: the omission is **without audible effect**, since the same routine has
just put the whole DSP program to zero, and a send to a zeroed DSP goes nowhere. That is why it would never
have been noticed. I do not call it intentional for all that.

### The Duff's devices' 512 bytes: a chunk of a sample being streamed

```
00601F1C  move.b d1, $400001.l        ; the bank register
00601F22  movea.l $4(a6), a1          ; the descriptor's +4 : the SOURCE in the sample ROM
00601F26  move.l #$80000001, $c(a6)   ; +0x0C : a state marker
00601F2E  move.w #$200, d0 ; sub.w d5, d0 ; lsr.w #$1, d0 ; lea ... ; jmp (a0)
...                                    ; D5 bytes copied, then 512 - D5 cleared
00602144  move.l a2, $8(a6)           ; +8 : the DESTINATION, advanced
00602148  bra.w $601cc0               ; back to the main loop
```

`A6` is the cursor over the **32 sample copy descriptors at `0x1000`** (§2), walked round-robin by the main
loop. So each pass copies **at most 512 bytes of a sample** from the banked ROM into the sound RAM, the
destination being kept in the descriptor's `+8` and advanced. It is **streaming**, and the zero fill handles
the last partial chunk.

### The two data tables

**`0x600396` is a decompressor's dictionary.** `0x6002EC` reads bytes from a stream: bit 7 clear, it emits
that byte **and the next one** into a buffer `(a0)+`; bit 7 set, it is a code, and `0x600308`'s `jmp`
dispatches it over eight branches — one of which emits one of `0x600396`'s **sixteen words**. A dictionary of
frequent values, then, in a byte-compressed format.

**`0x60103C` is a release-duration curve.** The 32 descending words go into a voice record's `+0x0E`, and the
note off reads them: `0x6012B4` takes `+0x0E`, and `+0x10` keeps the **minimum** of the values seen
(`cmp.w $10(a4), d0` then `move.w d0, $10(a4)`). A release countdown, indexed by a counter that advances one
step at a time.

### The four globals

| | |
|---|---|
| **`0xF80`** | the **pitch wheel per MIDI channel**, one word each: `0x60134A` writes it (`move.w d2, (a4, d1.w)` with `d1 = channel × 2`), and **a note on reads it** (`0x600C9A`: `move.w (a1, d5.w), d5` then `bsr 0x601390`, the `OCT`/`FNS` converter). `0x601BD2` clears it on CC `0x50`, `0x603172` when a voice stops |
| **`0xC3F`** | a **MIDI thru**: `0x6005C8`, in `snd_irq3`, does `tst.b $c3f.w` then `move.b d0, $100407.l` — every byte received is sent straight back out in `MOBUF`. Set and cleared by two commands (`0x603274`, `0x60327C`). It is the driver's **third** MIDI output path |
| **`0xED8`** | a **32-bit mask**, one bit per `0x2000` block of sound RAM: `0x600D86` walks it bit by bit and takes `0x2000` off the address for each bit set — it **skips the reserved blocks**. It is `0xFF >> [0x60807D]`, and since the ceiling is 32, **the mask is zero on this ROM**: nothing is reserved |
| **`0xEFE`** | **written once** (`0x602F9A : move.w #$8000, $efe.w`) and **read nowhere** in all the code reached. Remains unestablished |

### The instrument record's three pairs

`+0x10` and `+0x11` **feed the `0xF00` table**, the software modulation's (§4 bis):

```
00600D38  move.b $10(a1), d5 ; move.b $11(a1), d6      ; the instrument
00600D40  move.w a5, d1 ; lsr.w #$3, d1                ; a5 = the slot's registers -> slot x 4
00600D44  lea.l $f00.w, a1 ; adda.l $c10.w, a1
00600D4C  move.b d0, (a1, d1.w)                        ; +0
00600D50  move.b d5, $1(a1, d1.w)                      ; +1 <- the instrument's +0x10
00600D54  move.b d6, $3(a1, d1.w)                      ; +3 <- the instrument's +0x11
00600D62  move.b d0, $2(a1, d1.w)                      ; +2, zeroed if +0x11 is 0x80
```

So the two are the **instrument's software modulation parameters**, and the pairs (flag bit 7, value bits
6-0) that the eight small handlers edit (§5 bis quaterdecies) are their per-controller settings. **`+0x0B`
remains unread**: the controllers write it, nothing in the code reached reads it back through `A1`.

### Which of the two voice categories is the music: measured

The question called for a measurement, not a reading: a voice's MIDI channel is in its record, and its
category in two marking bits. Putting both on one line answers it by correlation. So `tools/vf3run.cpp`
takes `VF3_FICHES=file`, which prints on every frame the records whose `+4` is non-zero. Scene
`fv2-fight.txt`, arena 2, 3 000 frames, **1 807 distinct allocations**:

| category | object indices (`+5`) | MIDI channels (`+6`) | chip | count |
|---|---|---|---|---|
| **A** (`+0xB` bit 7, `+4` bit 3) | **`0x0F`-`0x13`** | `0x0B`-`0x0F` | **chip 1** (36 of 40) | 40 |
| **B** (unmarked) | **`0x00`-`0x09`** and `0xFF` | `0x00`-`0x09` | **chip 0** (1 765 of 1 767) | 1 767 |

**Category A is the effects', category B the music's.** A's object indices are exactly the five
`group_of()` calls effects — `0x0F` to `0x13` — and 36 of the 40 are on the **second SCSP**, which is what
`soundboard.h` already says of the effects, the voices and the announcer. The 11 cases of B classed as
"effects" by the naive test are the `+5 = 0xFF`, the marker of a record whose index is not set yet: **no real
counter-example**.

**And that finally orients §5 bis decies' consequence.** Category A's `+0xB` has its bit 7 forced, so every A
value is `≥ 0x80` and every B value `< 0x80`: the search for the minimum in the voice stealer **always takes
a B voice before an A one**. In other words **the music is stolen before the effects**. For a fighting game
that is the right choice — the hits, the voices and the announcer must not drop — and it is now measured
rather than guessed.

It also cross-checks the trio of stops: entry 2 of table 4 clears track 0 and stops B, so it **stops the
music**; entry 3 clears tracks 1-7 and stops A, so it **stops the effects**. And `snd_irq2` treats track 0
apart, with its three timing hooks: it is the music's sequencer.

### What remains out of reach

**The hardware's behaviour on `0x7C0-0x7FF`.** This document has two lines of evidence on what the
**driver** does — a single linear sweep that would destroy any mirror, and a parameter interpreter whose 59
addresses never enter it — and **nothing on the chip**. No reading of this program can answer it: it would
take a Model 3 and a measurement. It is noted as such and not as open.


## 6. The tooling, and how to redo all of this

| | |
|---|---|
| extract the ROM and put the bytes the right way round | `tools/sound/extract-sound.py <game.zip> <out.bin>` |
| import into Ghidra and analyse, headless | `tools\sound\import-sound.cmd` |
| seed the entry points before the analysis | `tools/ghidra/FV2SoundSetup.java` (as `-preScript`) |
| **search** the whole program | `tools/sound/disasm_sound.py <image> --search 100600 1006ff` |
| read a function from a safe boundary | `tools/sound/disasm_sound.py <image> --at 600780 26` |
| the vector table | `tools/sound/disasm_sound.py <image> --map` |
| open the four jump tables to Ghidra | `tools/ghidra/FV2SoundJumpTables.java` |
| resolve the last five `jmp (An)` | `tools/ghidra/FV2SoundIndirectJumps.java` |
| declare the data, attach the unrolled blocks, recount | `tools/ghidra/FV2SoundData.java` |
| print the functions and their ranges, changing nothing | `tools/ghidra/FV2SoundList.java` |

To run one of these scripts headless, on the already-imported project:

```
"...\ghidra_12.1.2_PUBLIC\support\analyzeHeadless.bat" C:\Temp\fv2-ghidra FV2SON ^
  -process fv2-sound-68000.bin -noanalysis -scriptPath <repo>\tools\ghidra -postScript <Script>.java
```

Add `-readOnly` for those that must write nothing (`FV2SoundList`). And **filter the output with care**: three
times in this campaign a too-narrow `Select-String` swallowed half the dump while the total arrived anyway,
which looks like a result. The scripts print their counts so they can be set against the number of lines
received.

The Ghidra project is `C:\Temp\fv2-ghidra\FV2SON.gpr`, **apart from `FV2.rep`** (the PowerPC): it is neither
the same processor nor the same program, and a project open in the interface is locked against the headless
analyser.

The division of roles between the two tools, and it is not a duplication: **Ghidra to read** a function (it
follows the jump tables, `disasm_sound.py` does not), **`disasm_sound.py` to search** the 512 KB from a command
line, repeatably, without a project. The two answers this port needed — the records at `0x400`, the write at
`+0x600` — came from the search.


## 7. What the next pass should read, in order of interest

> **This list is closed as far as reading the code goes**: the 168 functions are open (§5 ter), and the
> **first five entries below are done**, the last of them by a measurement on 2026-10-25. What remains is no
> longer "which function to read" but **which meaning to establish** — §5 ter's table gives that list.
> **The twelve arenas have been measured** (§4 sexies), and it is that pass which turned the fifth entry
> over: no question on this list is still waiting to be run.

1. ~~**The function containing `0x602BA2`**~~ — **done on 2026-10-25**. It is a descriptor-driven
   interpreter, and its 59 addresses are enumerated: `0x714` to `0x794`, **none** in `0x7C0-0x7FF`. See
   5 bis bis. What the hardware does with those 64 bytes stays out of the reach of reading.
2. ~~**Who sets a record's byte `+5`**~~ — **done on 2026-10-25**, and through the write: `0x6010F2` for
   `+5` (the object's index, `(A0 − 0xC40) >> 5`), `0x600C5A` for `+6` (the MIDI channel). See 5 quater.
3. ~~**`snd_irq2` (`0x6032D2`)**~~ — **done on 2026-10-25**. It is timer A, the music's tick at `0xCE`. It
   queues the ticks rather than losing them (`0xC01` the lock, `0xC1E` the deferred counter), and it sends
   `0xF0` in MOBUF every ten ticks — which the port throws away. See 5 bis ter.
4. ~~**The `FF` that restarts a sound before cutting it**~~ — **explained in §3 ter**: it restarts the
   board, intercepted in `snd_irq3_reception_midi` before anything else. This list entry still said "never
   explained" while §3 ter was reading it: corrected on 2026-10-25. And the consumer confirms it from the
   other end — its `0xFn` slot is an `rts`, because nothing ever puts a system message into the ring.
5. ~~**Whether the DSP program changes by arena**~~ — **measured over the twelve arenas on 2026-10-25, and
   the answer is YES for three of them.** Arenas **6** and **9** load on chip 0, around frame 2730, a
   program (`efff4af1`) the other nine never see; arena **11** reloads an existing one. And the
   coefficients vary in **five sets** over the twelve. See §4 sexies — which also keeps the error: with two
   arenas only, 2 and 7, everything looked identical, and both are in the group of nine that load nothing
   more.
6. **What the two Duff's devices copy** (`0x601F3C`, `0x6023E2`): 512 bytes at most, `A1` to `A2`, and both
   are in `snd_reset`. Two hand-unrolled copies at the same point of the start-up is the installation of
   something bulky — a table or a DSP program. The size `D5` and the two pointers would say.
7. **The two tables whose meaning is missing**: `0x600396` (sixteen words emitted into a stream by
   `0x6002EC`'s dispatcher) and `0x60103C` (thirty-two descending words, indexed by a counter, written at
   `0x0E(A4)`). The second looks like a duration per envelope step; **that is not established**, and what
   would settle it is the identity of the `+0x0E` field of the record `A4` points at.
8. **What the 512 bytes are** that the copy-then-clear pair installs twice in `snd_reset`. It is the same
   question as point 6, and it now has figures: `D5` bytes from `(A1)`, `512 − D5` zeros, a buffer of
   `0x200`. The two pointers would settle it.
