; Veranda Solitaire's test build: sol_play(bank), Veranda's test player
; (tests/sdk_play.s in Veranda, MIT, by way of geoChess's port) with one
; change: the frame count stands still while the program is busy (_busy
; set: a deal, a redraw), so a script's clicks keep their place whatever
; the drawing's speed. The script (frame, x, y, flags: bit 0 the button,
; bit 6 y + 256, bit 7 a key; $FFFF ends it with the last frame) is
; played through mouseSource with absolute positions, keys into the
; keyboard queue; at the last frame a VRAM dump and the end. The script
; is at $A000 of a RAM bank of its own (the test build's last record), mapped
; while the player reads it.
.include "veranda.inc"
.include "macros.inc"
.include "x16hw.inc"

.export _sol_play
.import _busy

.segment "ZEROPAGE"
sa0:	.res 2                  ; the script's next entry
sa1:	.res 2

.segment "CODE"

; a = the script's RAM bank
_sol_play:
	sta sbank
	ldx $00
	phx
	sta $00
	LoadW sa0, $A000
	MoveW sa0, sa1            ; the end frame
@e:	ldy #0
	lda (sa1),y
	iny
	and (sa1),y
	cmp #$FF
	beq @e1
	AddVW 6, sa1
	bra @e
@e1:	iny
	lda (sa1),y
	sta endF
	iny
	lda (sa1),y
	sta endF+1
	pla
	sta $00
	LoadW mouseSource, Player
	rts

Player:	lda $00
	pha
	lda sbank
	sta $00
	jsr Play
	pla
	sta $00
	lda pb
	sec
	rts

Play:	lda _busy
	bne @out
	IncW frame
	lda frame
	cmp endF
	bne @next
	lda frame+1
	cmp endF+1
	bne @next
	lda #1
	sta EMU_SAVE_ON_EXIT
	jmp $FFFF
@next:	ldy #0
	lda (sa0),y
	tax
	iny
	lda (sa0),y
	cmp #$FF
	beq @out
	cmp frame+1
	bcc @take
	bne @out
	cpx frame
	beq @take
	bcs @out
@take:	ldy #5
	lda (sa0),y
	bmi @key
	and #1
	sta pb
	ldy #2
	lda (sa0),y
	sta px
	iny
	lda (sa0),y
	sta px+1
	iny
	lda (sa0),y
	sta py
	iny
	lda (sa0),y              ; flags bit 6: y + 256
	and #$40
	asl
	rol
	rol
	sta py+1
	bra @adv
@key:	ldy #2
	lda (sa0),y
	jsr k_kbdbuf_put
@adv:
	AddVW 6, sa0
	bra @next
@out:
	MoveW px, r0
	MoveW py, r1
	rts

.segment "DATA"
endF:	.word 0
frame:	.word $FFFF
px:	.word 320
py:	.word 240
pb:	.byte 0
sbank:	.byte 0
