
.setcpu "6502"
.segment "CODE"

; USA Rev 0 ABI
LoadByte       = $87A4
QuiescePPU     = $CAF7
InitSystem     = $CA78
UploadPPU      = $F89E
StartNonGame   = $CB27
WaitMovieFrame = $CA25
PrepareSprites = $CB4F
ClearOAM       = $AA83
BlitMetasprite = $AA9F
QueuePalette   = $D090
PlaySFX        = $D0E4
AreaLoadTiles  = $CEB8
LoadSpritePal  = $D062
ResetGamePlay  = $CB17
LoadSpriteImgs = $C28D
; main-line bank switch, NMI uses $CC85
MainlineSwitchBank = $CC1A
BankedCall      = $F859
SetupScreen    = $DD13
LoadScreen     = $DD46
SetupSprites   = $DD4E
InitPlayer     = $E0AA
GameMainLoop   = $DB45
EnterBuilding  = $DE06
EnterArea      = $DF64
GameplayBank   = $0E
SetNewGame     = $9570
StartGame      = $DB26
ResetToTitle   = $C913

; reload_screen RAM
ScreenReady    = $000E
Joy1Buttons    = $0016
Joy1Previous   = $0018
Joy1Changed    = $0019
CurrentArea    = $0024
CurrentScreen  = $0063
LoadingScreen  = $0064
DestPalette    = $0065
StartPosYX     = $006C
AreaTiles      = $0095
PlayerX        = $009E
PlayerY        = $00A1
PlayerFlags    = $00A4
PlayerStatus   = $00A5
Music_Current  = $00FA
TextBoxContext = $0201
PortraitID     = $03C7
SelectedWeapon = $03BD
CurrentWeapon  = $03C8
ScreenPalette  = $03D0
DefaultMusic   = $03D1
SavedArea      = $03D5
SavedScreen    = $03D6
SavedPalette   = $03D7
SavedPosYX     = $03D8
BuildingTiles  = $03D9
DestScreen     = $03DA

; scratch pointers, reload after calls that can clobber them
PTRLO = $E2
PTRHI = $E3
DELTAF = $E6
DELTAW = $E7

; movie state
MV_EXIT       = $0600
MV_MUSIC      = $0601
MV_ASSETS     = $0602
MV_TRACKS     = $0603
MV_PHASES     = $0604
MV_SFX_COUNT  = $0605
MV_LIB_LO_LO  = $0606
MV_LIB_LO_HI  = $0607
MV_LIB_HI_LO  = $0608
MV_LIB_HI_HI  = $0609
MV_LIB_COUNT  = $060A
MV_PHASE_INDEX = $060B
MV_EFFECT_LO  = $060C
MV_EFFECT_HI  = $060D
MV_FRAME_LO   = $060E
MV_FRAME_HI   = $060F

TR_PTR_LO = $0610
TR_PTR_HI = $0618
TR_X      = $0620
TR_XF     = $0628
TR_Y      = $0630
TR_YF     = $0638
TR_VX     = $0640
TR_VY     = $0648
TR_TICK   = $0650
TR_AUX    = $0658             ; path stage / cyclic pose
TR_FRAME  = $0660             ; $FF means hidden
TR_SLOT   = $0668

PHASE_PTR_LO = $0670
PHASE_PTR_HI = $0671
SFX_PTR_LO   = $0672
SFX_PTR_HI   = $0673
UPDATE_MASK  = $0674
DRAW_MASK    = $0675
EFFECT_KIND  = $0676
EFFECT_TRACK = $0677
EFFECT_TRIGGER = $0678
EFFECT_PERIOD  = $0679
EFFECT_SUB     = $067A
EFFECT_FLOOR   = $067B
CONDITION_KIND = $067C
CONDITION_TRACK = $067D
CONDITION_LO    = $067E
CONDITION_HI    = $067F

ITER_LO = $0680
ITER_HI = $0681
LEFT    = $0682
TMP0    = $0683
TMP1    = $0684
TMP2    = $0685
EFFECT_CADENCE = $0686
TMP3    = $0687

SPLASH_CURSOR = $0688
SPLASH_X      = $0689
SPLASH_Y      = $068A

; reload_screen snapshot in the unused tail of the screen buffer
RS_AREA       = $068C
RS_SCREEN     = $068D
RS_LOADING    = $068E
RS_PALETTE    = $068F
RS_POS        = $0690
RS_FLAGS      = $0691
RS_STATUS     = $0692
RS_TILES      = $0693
RS_BUILD_TILES = $0694
RS_MUSIC      = $0695
RS_DEFAULT_MUSIC = $0696
RS_WEAPON     = $0697

.export AtlasDevPlayMovie, Bundle

; play MovieId, then run its terminal continuation
AtlasDevPlayMovie:
    jsr LoadByte
    tax
    cpx Bundle+5
    bcc :+
    jmp BadMovie
:
    lda #<(Bundle+6)
    sta PTRLO
    lda #>(Bundle+6)
    sta PTRHI
@seek:
    cpx #$00
    beq @found
    ldy #$00
    lda (PTRLO),y
    sta TMP0
    iny
    lda (PTRLO),y
    sta TMP1
    clc
    lda TMP0
    adc #$02
    sta TMP0
    lda TMP1
    adc #$00
    sta TMP1
    clc
    lda PTRLO
    adc TMP0
    sta PTRLO
    lda PTRHI
    adc TMP1
    sta PTRHI
    dex
    bne @seek
@found:
    jsr AdvanceTwo
    ldy #$00
    lda (PTRLO),y
    cmp #'F'
    beq :+
    jmp BadMovie
:
    iny
    lda (PTRLO),y
    cmp #'M'
    beq :+
    jmp BadMovie
:
    iny
    lda (PTRLO),y
    cmp #'V'
    beq :+
    jmp BadMovie
:
    iny
    lda (PTRLO),y
    cmp #'1'
    beq :+
    jmp BadMovie
:
    iny
    lda (PTRLO),y
    cmp #$02
    beq :+
    jmp BadMovie
:

    ; FMV1 v2 header
    iny
    lda (PTRLO),y
    sta MV_EXIT
    iny
    lda (PTRLO),y
    sta MV_MUSIC
    iny
    lda (PTRLO),y
    sta MV_ASSETS
    iny
    lda (PTRLO),y
    sta MV_TRACKS
    iny
    lda (PTRLO),y
    sta MV_PHASES
    iny
    lda (PTRLO),y
    sta MV_SFX_COUNT
    iny                              ; library bank ($0C)
    iny
    lda (PTRLO),y
    sta MV_LIB_LO_LO
    iny
    lda (PTRLO),y
    sta MV_LIB_LO_HI
    iny
    lda (PTRLO),y
    sta MV_LIB_HI_LO
    iny
    lda (PTRLO),y
    sta MV_LIB_HI_HI
    iny
    lda (PTRLO),y
    sta MV_LIB_COUNT
    iny                              ; id length at offset 17
    lda (PTRLO),y
    clc
    adc #$12                         ; first asset = movie + 18 + id length
    jsr AdvancePtrA

    lda MV_EXIT
    cmp #$03
    bne :+
    jsr SaveReloadState              ; snapshot before movie PPU/audio takeover
:

    jsr QuiescePPU
    jsr SetupAssets
    jsr InitTracks                  ; leaves PTR at first SFX record
    lda PTRLO
    sta SFX_PTR_LO
    lda PTRHI
    sta SFX_PTR_HI
    ldx MV_SFX_COUNT
@skip_sfx:
    cpx #$00
    beq @phases
    lda #$07
    jsr AdvancePtrA
    dex
    bne @skip_sfx
@phases:
    lda PTRLO
    sta PHASE_PTR_LO
    lda PTRHI
    sta PHASE_PTR_HI

    lda MV_MUSIC
    cmp #$FF                         ; leave current music
    beq @audio_done
    cmp #$FE                         ; stop
    bne @set_music
    lda #$00
@set_music:
    sta $FA
@audio_done:
    jsr StartNonGame
    lda #$00
    sta MV_PHASE_INDEX

NextPhase:
    lda MV_PHASE_INDEX
    cmp MV_PHASES
    bcs MovieExit
    jsr LoadPhase

FrameLoop:
    jsr WaitMovieFrame
    jsr PrepareSprites
    jsr ClearOAM
    inc MV_FRAME_LO
    bne :+
    inc MV_FRAME_HI
:
    ldx #$00
@updates:
    cpx MV_TRACKS
    bcs @sfx
    lda BitMasks,x
    and UPDATE_MASK
    beq :+
    jsr UpdateTrack
:
    inx
    bne @updates
@sfx:
    jsr RunSFX
    ldx #$00
@draws:
    cpx MV_TRACKS
    bcs @effect
    lda BitMasks,x
    and DRAW_MASK
    beq :+
    jsr DrawTrack
:
    inx
    bne @draws
@effect:
    jsr ApplyEffect
    jsr ConditionMet
    cmp #$00
    beq FrameLoop

    inc MV_PHASE_INDEX
    clc
    lda PHASE_PTR_LO
    adc #$0E
    sta PHASE_PTR_LO
    bcc NextPhase
    inc PHASE_PTR_HI
    bne NextPhase

MovieExit:
    lda MV_EXIT
    cmp #$01
    beq @new_game
    cmp #$03
    beq @reload
    jmp ResetToTitle                 ; title_reset / bad exit fallback
@new_game:
    ; match reset clear, keep page $01 for stack and mapper state
    jsr QuiescePPU
    lda #$00
    ldx #$00
@new_game_clear:
    cpx #$FC
    bcs :+
    sta $00,x
:
    sta $0200,x
    sta $0300,x
    sta $0400,x
    sta $0500,x
    sta $0600,x
    sta $0700,x
    inx
    bne @new_game_clear
    jsr InitSystem
    jsr SetNewGame
    ; StartGame skips the outside-entry copy
    lda SelectedWeapon
    sta CurrentWeapon
    ; StartGame needs bank 14, bank 12 cannot return here after the switch
    ldx #GameplayBank
    lda #>(StartGame-1)
    pha
    lda #<(StartGame-1)
    pha
    jmp MainlineSwitchBank
@reload:
    jmp ReloadScreen

BadMovie:
    jmp ResetToTitle

; save enough state for vanilla room re-entry, position snaps to 16 pixels
SaveReloadState:
    lda CurrentArea
    sta RS_AREA
    lda CurrentScreen
    sta RS_SCREEN
    lda LoadingScreen
    sta RS_LOADING
    lda ScreenPalette
    sta RS_PALETTE
    lda PlayerY
    and #$F0
    sta RS_POS
    lda PlayerX
    and #$F0
    lsr
    lsr
    lsr
    lsr
    ora RS_POS
    sta RS_POS
    lda PlayerFlags
    sta RS_FLAGS
    lda PlayerStatus
    sta RS_STATUS
    lda AreaTiles
    sta RS_TILES
    lda BuildingTiles
    sta RS_BUILD_TILES
    lda Music_Current
    and #$7F                         ; clear playing latch so reload restarts it
    sta RS_MUSIC
    lda DefaultMusic
    sta RS_DEFAULT_MUSIC
    lda CurrentWeapon
    sta RS_WEAPON
    rts

; reload through stock area/building entry, keep persistent game RAM
ReloadScreen:
    jsr QuiescePPU

    lda RS_AREA
    sta CurrentArea
    lda RS_SCREEN
    sta CurrentScreen
    lda RS_LOADING
    sta LoadingScreen
    lda RS_PALETTE
    sta DestPalette
    lda RS_POS
    sta StartPosYX
    lda RS_TILES
    sta AreaTiles
    lda RS_BUILD_TILES
    sta BuildingTiles
    lda RS_MUSIC
    sta Music_Current
    lda RS_DEFAULT_MUSIC
    sta DefaultMusic
    lda RS_WEAPON
    sta CurrentWeapon

    ; reset movement state, keep facing and Wing Boots
    jsr InitPlayer
    lda RS_FLAGS
    and #$40
    sta PlayerFlags
    lda RS_STATUS
    and #$80
    sta PlayerStatus
    lda #$FF
    sta PortraitID
    sta ScreenReady
    lda #$00
    sta TextBoxContext

    lda CurrentArea
    cmp #$04
    beq @building

    ; full area re-entry restores pointers, keep current music
    lda CurrentScreen
    sta LoadingScreen
    lda RS_MUSIC
    pha
    lda RS_DEFAULT_MUSIC
    pha
    jsr BankedCall
    .byte GameplayBank
    .word EnterArea-1
    pla
    sta DefaultMusic
    pla
    sta Music_Current
    jmp @finish

@building:
    ; building entry overwrites outside return state
    lda CurrentScreen
    sta DestScreen
    lda SavedArea
    pha
    lda SavedScreen
    pha
    lda SavedPalette
    pha
    lda SavedPosYX
    pha
    lda RS_MUSIC
    pha
    lda RS_DEFAULT_MUSIC
    pha
    lda RS_WEAPON
    pha
    jsr BankedCall
    .byte GameplayBank
    .word EnterBuilding-1
    pla
    sta CurrentWeapon
    pla
    sta DefaultMusic
    pla
    sta Music_Current
    pla
    sta SavedPosYX
    pla
    sta SavedPalette
    pla
    sta SavedScreen
    pla
    sta SavedArea

@finish:
    jsr ResetGamePlay
    jsr WaitMovieFrame
    ; sprite-image tables require bank 14
    jsr BankedCall
    .byte GameplayBank
    .word LoadSpriteImgs-1

    ; clear held-button edges
    lda Joy1Buttons
    sta Joy1Previous
    lda #$00
    sta Joy1Changed
    jmp GameMainLoop                 ; resets SP to $FF; script never resumes

; advance descriptor pointer by A or 2
AdvancePtrA:
    clc
    adc PTRLO
    sta PTRLO
    bcc :+
    inc PTRHI
:
    rts

AdvanceTwo:
    lda #$02
    bne AdvancePtrA

; upload PPU assets and copy palette to RAM
SetupAssets:
    lda MV_ASSETS
    sta LEFT
    beq @done
@asset:
    ldy #$01
    lda (PTRLO),y
    tax                              ; source bank
    iny
    lda (PTRLO),y
    sta $DB
    iny
    lda (PTRLO),y
    sta $DC
    iny
    lda (PTRLO),y                    ; destination space
    pha
    iny
    lda (PTRLO),y
    sta $E8
    iny
    lda (PTRLO),y
    sta $E9
    iny
    lda (PTRLO),y
    sta TMP0                         ; byte length low
    iny
    lda (PTRLO),y
    sta TMP1                         ; byte length high
    lda PTRLO
    sta ITER_LO
    lda PTRHI
    sta ITER_HI
    pla
    bne @ram

    ; UploadPPU uses Y=0 for 4096 bytes
    lda TMP0
    lsr
    lsr
    lsr
    lsr
    sta TMP2
    lda TMP1
    asl
    asl
    asl
    asl
    ora TMP2
    tay
    jsr UploadPPU
    jmp @next

@ram:
    ldy #$00
@copy:
    lda ($DB),y
    sta ($E8),y
    iny
    cpy TMP0
    bne @copy
@next:
    lda ITER_LO
    sta PTRLO
    lda ITER_HI
    sta PTRHI
    lda #$09
    jsr AdvancePtrA
    dec LEFT
    bne @asset
@done:
    jmp QueuePalette

; load track pointers and initial state
InitTracks:
    ldx #$00
@track:
    cpx MV_TRACKS
    bcc :+
    rts
:
    ldy #$00
    lda (PTRLO),y
    sta LEFT                         ; validated track records are at most 255 bytes
    clc
    lda PTRLO
    adc #$02
    sta PTRLO
    sta TR_PTR_LO,x
    lda PTRHI
    adc #$00
    sta PTRHI
    sta TR_PTR_HI,x
    ldy #$00
    lda (PTRLO),y
    cmp #$03
    beq @counter
    ldy #$01
    lda (PTRLO),y
    sta TR_X,x
    iny
    lda (PTRLO),y
    sta TR_XF,x
    iny
    lda (PTRLO),y
    sta TR_Y,x
    iny
    lda (PTRLO),y
    sta TR_YF,x
    iny
    lda (PTRLO),y
    sta TR_VX,x
    iny
    lda (PTRLO),y
    sta TR_VY,x
    jmp @state
@counter:
    ldy #$01
    lda (PTRLO),y
    sta TR_X,x
    iny
    lda (PTRLO),y
    sta TR_Y,x
@state:
    lda #$00
    sta TR_TICK,x
    sta TR_AUX,x
    sta TR_SLOT,x

    ; a draw-only first phase still needs a valid initial frame
    ldy #$00
    lda (PTRLO),y
    cmp #$01
    beq @path_frame
    cmp #$02
    beq @cyclic_frame
    lda #$FF                         ; CounterToggle resolves during draw
    bne @store_frame
@path_frame:
    ldy #$0A
    lda (PTRLO),y
    sta TMP2
    asl
    clc
    adc TMP2
    adc #$0E                         ; first slot in stage zero
    tay
    lda (PTRLO),y
    jmp @store_frame
@cyclic_frame:
    ldy #$0B
    lda (PTRLO),y                    ; first visible pose
@store_frame:
    sta TR_FRAME,x

    ; advance past the length-prefixed record
    clc
    lda PTRLO
    adc LEFT
    sta PTRLO
    bcc :+
    inc PTRHI
:
    inx
    jmp @track

LoadPhase:
    lda PHASE_PTR_LO
    sta PTRLO
    lda PHASE_PTR_HI
    sta PTRHI
    ldy #$00
    lda (PTRLO),y
    sta UPDATE_MASK
    iny
    lda (PTRLO),y
    sta DRAW_MASK
    iny
    lda (PTRLO),y
    sta TMP0                         ; enter kind
    iny
    lda (PTRLO),y
    sta TMP1                         ; enter value
    iny
    lda (PTRLO),y
    sta EFFECT_KIND
    iny
    lda (PTRLO),y
    sta EFFECT_TRACK
    iny
    lda (PTRLO),y
    sta EFFECT_TRIGGER
    iny
    lda (PTRLO),y
    sta EFFECT_PERIOD
    iny
    lda (PTRLO),y
    sta EFFECT_SUB
    iny
    lda (PTRLO),y
    sta EFFECT_FLOOR
    iny
    lda (PTRLO),y
    sta CONDITION_KIND
    iny
    lda (PTRLO),y
    sta CONDITION_TRACK
    iny
    lda (PTRLO),y
    sta CONDITION_LO
    iny
    lda (PTRLO),y
    sta CONDITION_HI
    lda #$00
    sta MV_EFFECT_LO
    sta MV_EFFECT_HI
    sta MV_FRAME_LO
    sta MV_FRAME_HI
    sta EFFECT_CADENCE
    lda TMP0
    cmp #$01
    bne :+
    lda TMP1
    sta $1A
:
    rts

; signed velocity to 8.8 delta, preserves X/PTR
MakeDelta:
    sta DELTAW
    lda #$00
    sta DELTAF
    lda DELTAW
@shift:
    sta DELTAW
    rol DELTAW
    ror
    ror DELTAF
    dey
    bne @shift
    sta DELTAW
    rts

UpdateTrack:
    lda TR_PTR_LO,x
    sta PTRLO
    lda TR_PTR_HI,x
    sta PTRHI
    ldy #$00
    lda (PTRLO),y
    cmp #$03
    bne :+
    rts
:

    ldy #$07
    lda (PTRLO),y
    tay
    lda TR_VX,x
    jsr MakeDelta
    clc
    lda TR_XF,x
    adc DELTAF
    sta TR_XF,x
    lda TR_X,x
    adc DELTAW
    sta TR_X,x

    ldy #$07
    lda (PTRLO),y
    tay
    lda TR_VY,x
    jsr MakeDelta
    clc
    lda TR_YF,x
    adc DELTAF
    sta TR_YF,x
    lda TR_Y,x
    adc DELTAW
    sta TR_Y,x

    ldy #$00
    lda (PTRLO),y
    cmp #$01
    beq UpdatePath
    jmp UpdateCyclic

UpdatePath:
    ; one keyframe per update
    ldy #$0A
    lda TR_AUX,x
    cmp (PTRLO),y
    bcs @gate_done
    sta TMP0
    asl
    clc
    adc TMP0
    adc #$0B
    sta TMP0                         ; keyframe threshold offset
    ldy #$08
    lda (PTRLO),y                    ; coordinate enum
    cmp #$01
    bne @use_y
    lda TR_X,x
    jmp @compare
@use_y:
    lda TR_Y,x
@compare:
    pha
    ldy TMP0
    pla
    cmp (PTRLO),y
    php
    ldy #$09
    lda (PTRLO),y                    ; comparison enum: 1 lt, 2 gte
    cmp #$01
    beq @want_lt_flags
    plp
    bcc @gate_done                   ; gte: carry clear means not crossed
    bcs @crossed
@want_lt_flags:
    plp
    bcs @gate_done
@crossed:
    inc TR_AUX,x
    ldy TMP0
    iny
    lda (PTRLO),y
    sta TR_VX,x
    iny
    lda (PTRLO),y
    sta TR_VY,x
@gate_done:
    ; animation header = 11 + keyframe_count*3
    ldy #$0A
    lda (PTRLO),y
    sta TMP0
    asl
    clc
    adc TMP0
    adc #$0B
    sta TMP0                         ; animation offset

    ; TR_TICK stays the SFX-visible 8-bit update count.
    inc TR_TICK,x
    ldy TMP0
    lda (PTRLO),y                    ; dwell
    sta TMP1
    lda TR_TICK,x
    jsr DivMod8                      ; A = tick / dwell
    pha
    ldy TMP0
    iny
    iny
    lda (PTRLO),y                    ; slots per stage
    sta TMP1
    pla
    jsr DivMod8                      ; TMP2 = quotient % slots
    lda TMP2
    sta TR_SLOT,x

    ; frame = anim + 3 + stage*slots + slot
    lda #$00
    ldy TR_AUX,x
@stage_mul:
    cpy #$00
    beq @stage_done
    clc
    adc TMP1
    dey
    bne @stage_mul
@stage_done:
    clc
    adc TR_SLOT,x
    adc TMP0
    adc #$03
    tay
    lda (PTRLO),y
    sta TR_FRAME,x
UpdateDone:
    rts

; A / TMP1 in eight iterations; quotient in A, remainder in TMP2
DivMod8:
    sta DELTAF
    lda #$00
    sta TMP2
    ldy #$08
@bit:
    asl DELTAF
    rol TMP2
    lda TMP2
    cmp TMP1
    bcc :+
    sbc TMP1
    sta TMP2
    inc DELTAF
:
    dey
    bne @bit
    lda DELTAF
    rts

UpdateCyclic:
    inc TR_TICK,x
    ldy #$08
    lda TR_TICK,x
    cmp (PTRLO),y
    bne @select
    lda #$00
    sta TR_TICK,x
    inc TR_AUX,x
    ldy #$0A
    lda TR_AUX,x
    cmp (PTRLO),y
    bne @select
    ; reset movement at reset_at_pose
    ldy #$01
    lda (PTRLO),y
    sta TR_X,x
    iny
    lda (PTRLO),y
    sta TR_XF,x
    iny
    lda (PTRLO),y
    sta TR_Y,x
    iny
    lda (PTRLO),y
    sta TR_YF,x
    iny
    lda (PTRLO),y
    sta TR_VX,x
    iny
    lda (PTRLO),y
    sta TR_VY,x
    lda #$00
    sta TR_AUX,x
    sta TR_TICK,x
@select:
    lda TR_AUX,x
    sta TR_SLOT,x
    ldy #$09
    cmp (PTRLO),y                    ; visible count
    bcs @hidden
    clc
    adc #$0B
    tay
    lda (PTRLO),y
    sta TR_FRAME,x
    rts
@hidden:
    lda #$FF
    sta TR_FRAME,x
    rts

; evaluate 7-byte SFX rules after update, before draw
RunSFX:
    lda SFX_PTR_LO
    sta ITER_LO
    lda SFX_PTR_HI
    sta ITER_HI
    lda MV_SFX_COUNT
    sta LEFT
    beq @done
@event:
    lda ITER_LO
    sta PTRLO
    lda ITER_HI
    sta PTRHI
    ldy #$00
    lda (PTRLO),y
    tax
    ldy #$02
    lda (PTRLO),y
    cmp #$FF
    beq @tick
    sta TMP0
    lda TR_AUX,x
    cmp TMP0
    bcs @next
@tick:
    ldy #$03
    lda (PTRLO),y
    and TR_TICK,x
    iny
    cmp (PTRLO),y
    bne @next
    iny
    lda (PTRLO),y
    and TR_SLOT,x
    iny
    cmp (PTRLO),y
    bne @next
    ldy #$01
    lda (PTRLO),y
    jsr PlaySFX
@next:
    clc
    lda ITER_LO
    adc #$07
    sta ITER_LO
    bcc :+
    inc ITER_HI
:
    dec LEFT
    bne @event
@done:
    rts

DrawTrack:
    lda TR_PTR_LO,x
    sta PTRLO
    lda TR_PTR_HI,x
    sta PTRHI
    ldy #$00
    lda (PTRLO),y
    cmp #$03
    beq @counter
    lda TR_X,x
    sta SPLASH_X
    lda TR_Y,x
    sta SPLASH_Y
    lda TR_FRAME,x
    cmp #$FF
    beq @done
    jmp DrawFrame

@counter:
    iny
    lda (PTRLO),y
    sta SPLASH_X
    iny
    lda (PTRLO),y
    sta SPLASH_Y
    iny
    lda (PTRLO),y
    sta DELTAF                       ; arbitrary counter pointer
    iny
    lda (PTRLO),y
    sta DELTAW
    ldy #$00
    lda (DELTAF),y
    sta TMP0
    ldy #$05
    lda (PTRLO),y
    and TMP0
    beq @first
    ldy #$08
    bne @pick
@first:
    ldy #$07
@pick:
    lda (PTRLO),y
    jmp DrawFrame
@done:
    rts

; resolve frame A through $AA9F, $AA9C is an operand byte
DrawFrame:
    cmp MV_LIB_COUNT
    bcs @done
    tay
    lda MV_LIB_LO_LO
    sta PTRLO
    lda MV_LIB_LO_HI
    sta PTRHI
    lda (PTRLO),y
    sta $EC
    lda MV_LIB_HI_LO
    sta PTRLO
    lda MV_LIB_HI_HI
    sta PTRHI
    lda (PTRLO),y
    sta $ED
    jsr BlitMetasprite
@done:
    rts

ApplyEffect:
    lda EFFECT_KIND
    cmp #$01
    bne @done
    ldx EFFECT_TRACK
    lda TR_AUX,x
    cmp EFFECT_TRIGGER
    bcc @done
    inc MV_EFFECT_LO
    bne :+
    inc MV_EFFECT_HI
:
    inc EFFECT_CADENCE
    lda EFFECT_CADENCE
    cmp EFFECT_PERIOD
    bne @done
    lda #$00
    sta EFFECT_CADENCE
    ldy #$1F
@palette:
    lda $0293,y
    sec
    sbc EFFECT_SUB
    bcc @floor
    cmp EFFECT_FLOOR
    bcs @store
@floor:
    lda EFFECT_FLOOR
@store:
    sta $0293,y
    dey
    bpl @palette
    jsr QueuePalette
@done:
    rts

; A=1 when the phase is done
ConditionMet:
    lda CONDITION_KIND
    cmp #$01
    beq @effect_calls
    cmp #$02
    beq @track_y
    cmp #$03
    beq @music
    cmp #$04
    beq @counter
    cmp #$05
    beq @frames
@false:
    lda #$00
    rts
@effect_calls:
    lda MV_EFFECT_HI
    cmp CONDITION_HI
    bcc @false
    bne @true
    lda MV_EFFECT_LO
    cmp CONDITION_LO
    bcc @false
    bcs @true
@track_y:
    lda CONDITION_HI
    bne @false
    ldx CONDITION_TRACK
    lda TR_Y,x
    cmp CONDITION_LO
    bcc @false
    bcs @true
@music:
    lda $FA
    beq @true
    bne @false
@counter:
    lda $1A
    beq @true
    bne @false
@frames:
    lda MV_FRAME_HI
    cmp CONDITION_HI
    bcc @false
    bne @true
    lda MV_FRAME_LO
    cmp CONDITION_LO
    bcc @false
@true:
    lda #$01
    rts

BitMasks:
    .byte $01,$02,$04,$08,$10,$20,$40,$80

; installer appends FMB1 here
Bundle:
