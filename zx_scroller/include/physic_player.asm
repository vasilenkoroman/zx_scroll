//player for TBK PSG Packer
//psndcj//tbk - 11.02.2012,01.12.2013
//source for sjasm cross-assembler
//modified by physic 8.12.2021
//Max time reduced from 1089t to 841t (-248t)

/*
11hhhhhh llllllll nnnnnnnn	3	CALL_N - вызов с возвратом для проигрывания (nnnnnnnn + 1) значений по адресу 11hhhhhh llllllll
10hhhhhh llllllll			2	CALL_1 - вызов с возвратом для проигрывания одного значения по адресу 11hhhhhh llllllll
01MMMMMM mmmmmmmm			2+N	PSG2 проигрывание, где MMMMMM mmmmmmmm - инвертированная битовая маска регистров, далее следуют значения регистров.
							во 2-м байте также инвертирован порядок следования регистров (13..6)

00111100..00011110          1	PAUSE32 - пауза pppp+1 (1..32, N + 120)
00111111					1	маркер окончания трека

0001hhhh vvvvvvvv			2	PSG1 проигрывание, 1 register, 0000hhhh - номер регистра, vvvvvvvv - значение
0000hhhh vvvvvvvv			4	PSG1 проигрывание, 2 registers, 0000hhhh - номер регистра, vvvvvvvv - значение

Также эта версия частично поддерживает короткие вложенные ссылки уровня 2 (доп. ограничение - они не могут стоять в конце длинной ссылки уровня 1).
*/

LD_HL_CODE	EQU 0x21
JR_CODE		EQU 0x18
							
init		ld hl, music
			jr mus_init
play		EQU trb_play
stop		ld c,#fd
			ld hl,#ffbf
			ld de,#0d00
1			ld b,h
			out (c),d
			ld b,l
			out (c),e
			dec d
			jp nz,1b
			ret
		
mus_init	
			ld (pl_track+1), hl
			xor a
			ld (trb_rep+1), a
			ld hl, LD_HL_CODE * 256
			ld (trb_play), hl
			jr stop

trb_pause	//pause - skip frame
			ld a, 0
			dec a
			ld (trb_pause+1), a
			ret nz
								; 7+4+13+10=34t (10+34 = 44t)
			ld hl, LD_HL_CODE * 256	; end of pause
			ld (trb_play), hl
			ret
		
// pause or end track
pl_pause
			inc hl
			ld (pl_track+1), hl
			ret z
			cp 4 * 63 - 120
			jr z, endtrack
			//set pause
			rrca
			rrca
			ld (trb_pause+1), a	

			ld de, JR_CODE + (trb_pause - trb_play - 2) * 256
			ld (trb_play), de
			
			ret
//
endtrack	//end of track
			call init
			jp pl_track
			
			//play note
trb_play	nop							; 4
pl_track	ld hl, 0					; 10t (10+4 = 14t)

			ld a, (hl)
			ld b, a
			add a
			jr nc, pl_frame				; 7+4+4+7 = 22t (total 36t)

			// Process ref

			inc hl
			ld c, (hl)
			add a
			inc hl
			jr nc, pl10					; 6+7+4+6+7 = 30t  (total 66t)

pl11		ld a, (hl)						
			ld (trb_rep+1), a		
			ld (trb_rest+1), hl				; 7+13+16=36t

			add hl, bc
			ld a, (hl)
			add a		            

			call pl0x
			ld (pl_track+1), hl		
			ret								; 11+7+4+17+16+10=65t
			// total: 66+36+65=167t + pl0x time(674t) = 841t(max)

pl10
			ld (pl_track+1), hl		
			set 6, b
			add hl, bc

			ld a, (hl)
			add a		            
			jp pl0x					; 16+11+7+4+10=58t
			// total: 72+8+5+58=143t

pl_frame	call pl0x
			ld (pl_track+1), hl				
			
trb_rep		ld a, 0						
			sub 1
			ret c
			ld (trb_rep+1), a
			ret nz							; 7+7+5+13+5=37t
			// end of repeat, restore position in track
trb_rest	ld hl, 0
			inc hl
			ld (pl_track+1), hl		; 
			ret								; 10+16+10=36t
			// total: 36+5+37+36=114t

pl00		sub 120
			jr nc, pl_pause
			ld de, #ffbf
		//psg1
			// 2 registr - maximum, second without check
			ld a, (hl)
			sub #10
			jp nc, 7f
			outi
			ld b, e
			outi
			ld a, (hl)
7			inc hl
			ld b, d
			out (c),a
			ld b, e
			outi
			ret

pl0x		ld bc, #fffd				
			add a					
			jr nc, pl00
pl01			// player PSG2
			inc hl
			ld de,#00bf
			jr z, play_all1		; 10+4+7+6+10+7=44t

play_by_mask
			add a				
			jr c,1f
			out (c),d
			ld b,e
			outi				
1			inc d					; 4+7+12+4+16+4=47

			dup 4
				add a
				jr c,1f
				ld b,#ff
				out (c),d
				ld b,e
				outi				
1				inc d				
			edup					;54*2 + 15*2=138

			add a
			jr c,1f
			ld b,#ff
			out (c),d
			ld b,e
			outi					; 4+7+7+12+4+16=50
1			jp psg2_continue
			// total:  regular = 44+47+138+50+10=289

play_all1
			cpl
			dup 5
				ld b, a
				out (c),d
				ld b,e
				outi				
1				inc d				
			edup					
			ld b, a
			out (c),d
			ld b,e
			outi				; 4 + 5*40+36  = 240

			// total:  playall1 = 	44+5+240=289

psg2_continue
			ld a, (hl)
			inc hl					
			ld	 d, 13

			add a
			jr z, play_all2			; 7+6+7+4+7=31

			jr c,1f
			ld b,#ff
			out (c),d
			ld b,e
			outi	
1			dec d					;  7+7+12+4+16+4=50

			dup 6
				add a
				jr c,1f
				ld b,#ff
				out (c),d
				ld b,e
				outi	
1				dec d				; 54*4 + 15*2=246
			edup

 			add a
			ret c
			ld b,#ff
			out (c),d
			ld b,e
			outi					
			ret						; 4+5+7+12+4+16+10=58
			// total: 289+31+50+246+58=674

play_all2
			cpl

			jr c,1f					; Don't touch reg 13 if it unchanged in playAll mode as well
			ld b, a
			out (c),d
			ld b,e
			outi	
1			dec d					;  4+7+4+12+4+16+4=51

			dup 6
				ld b, a
				out (c),d
				ld b,e
				outi				
				dec d				; 6*40=240
			edup

			ld b, a
			out (c),d
			ld b,e
			outi					
			ret						; 4+12+4+16+10=46
			// total: 294+51+240+46=631

			DISPLAY	"player code occupies ", /D, $-init, " bytes"
