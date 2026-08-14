#!/usr/bin/env python3
"""Execute the Atlas movie track contracts on a 6502 simulator."""

from __future__ import annotations

import shutil
import subprocess
import tempfile
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
SOURCE = ROOT / "eoe_core/src/fe/AtlasMovieRuntime.s"


def checked(command: list[str], cwd: Path) -> None:
    result = subprocess.run(command, cwd=cwd, capture_output=True, text=True,
                            timeout=60, check=False)
    if result.returncode:
        raise RuntimeError(
            f"{Path(command[0]).name} failed with status {result.returncode}:\n"
            + result.stdout + result.stderr)


def contract_source() -> str:
    source = SOURCE.read_text(encoding="utf-8")
    result = source.replace('.segment "CODE"', '.segment "RUNTIME"', 1)
    result = result.replace(
        ".export AtlasDevPlayMovie, Bundle",
        ".export AtlasDevPlayMovie, Bundle\n"
        ".export InitTracks, UpdateTrack, DivMod8", 1)
    if result == source or '.segment "CODE"' in result:
        raise RuntimeError("failed to derive the runtime contract source")
    return result


HARNESS = r'''
.setcpu "6502"
.import InitTracks, UpdateTrack, DivMod8
.export _main

PTRLO = $E2
PTRHI = $E3
MV_TRACKS = $0603
TR_X = $0620
TR_Y = $0630
TR_TICK = $0650
TR_FRAME = $0660
TR_SLOT = $0668
TMP1 = $0684
TMP2 = $0685
TEST_DIVIDEND = $06F0
TEST_DIVISOR = $06F1
TEST_QUOTIENT = $06F2
TEST_REMAINDER = $06F3
TEST_COUNT = $06F4

.macro InitOne record
    lda #$01
    sta MV_TRACKS
    lda #<record
    sta PTRLO
    lda #>record
    sta PTRHI
    jsr InitTracks
.endmacro

.macro Expect address, value, failure
    lda address
    cmp #value
    beq :+
    lda #failure
    ldx #$00
    rts
:
.endmacro

.segment "CODE"
_main:
    InitOne PathRecord
    Expect TR_X, $10, $01
    Expect TR_Y, $20, $02
    Expect TR_TICK, $00, $03
    Expect TR_SLOT, $00, $04
    Expect TR_FRAME, $21, $05

    InitOne CyclicRecord
    Expect TR_X, $30, $06
    Expect TR_Y, $40, $07
    Expect TR_FRAME, $31, $08

    InitOne CounterRecord
    Expect TR_X, $50, $09
    Expect TR_Y, $60, $0A

    InitOne PathRecord
    ldx #$00
    jsr UpdateTrack
    Expect TR_TICK, $01, $0B
    Expect TR_SLOT, $00, $0C
    Expect TR_FRAME, $21, $0D
    ldx #$00
    jsr UpdateTrack
    ldx #$00
    jsr UpdateTrack
    Expect TR_TICK, $03, $0E
    Expect TR_SLOT, $01, $0F
    Expect TR_FRAME, $22, $10
    lda #253
    sta TEST_COUNT
@updates:
    ldx #$00
    jsr UpdateTrack
    dec TEST_COUNT
    bne @updates
    Expect TR_TICK, $00, $11
    Expect TR_SLOT, $00, $12
    Expect TR_FRAME, $21, $13

    lda #$01
    sta TEST_DIVISOR
@divisor:
    lda #$00
    sta TEST_DIVIDEND
@dividend:
    lda TEST_DIVISOR
    sta TMP1
    lda TEST_DIVIDEND
    jsr DivMod8
    sta TEST_QUOTIENT
    lda TMP2
    sta TEST_REMAINDER

    lda TEST_DIVIDEND
    ldy #$00
@reference:
    cmp TEST_DIVISOR
    bcc @reference_done
    sec
    sbc TEST_DIVISOR
    iny
    bne @reference
@reference_done:
    cmp TEST_REMAINDER
    beq :+
    lda #$14
    ldx #$00
    rts
:
    tya
    cmp TEST_QUOTIENT
    beq :+
    lda #$15
    ldx #$00
    rts
:
    inc TEST_DIVIDEND
    bne @dividend
    inc TEST_DIVISOR
    bne @divisor

    lda #$00
    tax
    rts

.segment "RODATA"
PathRecord:
    .word PathEnd-PathData
PathData:
    .byte $01,$10,$00,$20,$00,$00,$00,$01,$01,$02,$01
    .byte $FF,$00,$00
    .byte $03,$02,$05
    .byte $21,$22,$23,$24,$25
    .byte $41,$42,$43,$44,$45
PathEnd:

CyclicRecord:
    .word CyclicEnd-CyclicData
CyclicData:
    .byte $02,$30,$00,$40,$00,$00,$00,$01,$03,$01,$02,$31
CyclicEnd:

CounterRecord:
    .word CounterEnd-CounterData
CounterData:
    .byte $03,$50,$60,$1A,$00,$01,$02,$51,$52
CounterEnd:
'''


CONFIG = r'''
SYMBOLS {
  __EXEHDR__: type = import;
  __STACKSIZE__: type = weak, value = $0800;
}
MEMORY {
  ZP: file = "", start = $0000, size = $0100;
  HEADER: file = %O, start = $0000, size = $000C;
  MAIN: file = %O, define = yes, start = $2000, size = $DDF0 - __STACKSIZE__;
}
SEGMENTS {
  ZEROPAGE: load = ZP, type = zp;
  EXEHDR: load = HEADER, type = ro;
  STARTUP: load = MAIN, type = ro;
  LOWCODE: load = MAIN, type = ro, optional = yes;
  ONCE: load = MAIN, type = ro, optional = yes;
  CODE: load = MAIN, type = ro;
  RUNTIME: load = MAIN, type = ro;
  RODATA: load = MAIN, type = ro;
  DATA: load = MAIN, type = rw;
  BSS: load = MAIN, type = bss, define = yes;
}
FEATURES {
  CONDES: type = constructor, label = __CONSTRUCTOR_TABLE__, count = __CONSTRUCTOR_COUNT__, segment = ONCE;
  CONDES: type = destructor, label = __DESTRUCTOR_TABLE__, count = __DESTRUCTOR_COUNT__, segment = RODATA;
  CONDES: type = interruptor, label = __INTERRUPTOR_TABLE__, count = __INTERRUPTOR_COUNT__, segment = RODATA;
}
'''


def main() -> int:
    cl65, sim65 = shutil.which("cl65"), shutil.which("sim65")
    if not cl65 or not sim65:
        raise RuntimeError("cl65 and sim65 are required")
    with tempfile.TemporaryDirectory(prefix="atlas-movie-contract-") as raw:
        stage = Path(raw)
        (stage / "runtime.s").write_text(contract_source(), encoding="utf-8")
        (stage / "harness.s").write_text(HARNESS, encoding="utf-8")
        (stage / "contract.cfg").write_text(CONFIG, encoding="utf-8")
        checked([cl65, "-t", "sim6502", "-C", "contract.cfg",
                 "harness.s", "runtime.s", "-o", "contract.bin"], stage)
        checked([sim65, "contract.bin"], stage)
    print("Atlas movie 6502 track contracts passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
