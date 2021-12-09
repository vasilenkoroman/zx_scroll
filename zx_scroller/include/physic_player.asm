//player for TBK PSG Packer
//psndcj//tbk - 11.02.2012,01.12.2013
//source for sjasm cross-assembler
//modified by physic 8.12.2021
//Max time reduced from 1089t to 833t (-256t)

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
По-умолчанию пакер избегает пакованных фреймов, когда заполнены 5/6 регистров [0..5] или 6/7 регистров [6..12]. В этом случае записывается "лишний" регистр.
Т.о. проигрывание идет по ветке play_all_xx, что быстрее. Максимальные тайминги расчитаны исходя из этого факта.
*/

LD_HL_CODE	EQU 0x21
JR_CODE		EQU 0x18
							
init		EQU mus_init
play		EQU trb_play
stop		ld c,#fd
			ld hl,#ffbf
			ld de,#0d00
1			ld b,h
			out (c),d
			ld b,l
			out (c),e
			dec d
			jr nz,1b
			ret
		
mus_init	ld hl, music
			ld (pl_track+1), hl
			xor a
			ld (trb_rep+1), a
			ld a, LD_HL_CODE
			ld (trb_play), a
			ret							; 10+16+4+13+7+13+10=73
			// total for looping: 171+73=244

pause_rep   db 0
trb_pause   ld hl, pause_rep
			dec	 (hl)
			ret nz						; 10+11+5=26t

saved_track	
			ld hl, LD_HL_CODE			; end of pause
			ld (trb_play), hl
			jr trb_rep					; 10+16+12=38t
			// total: 34+38=72t
		
// pause or end track
pl_pause								; 103t on enter
			inc hl
			ld (pl_track+1), hl
			ret z
			cp 4 * 63 - 120
			jr z, endtrack				; 6+16+5+7+7=41
			//set pause
			rrca
			rrca
			ld (pause_rep), a	
			ld  a, l
			ld (saved_track+2), a
			ld hl, JR_CODE + (trb_pause - trb_play - 2) * 256
			ld (trb_play), hl
			
			pop	 hl						; 10
			ret							; 4+4+13+4+13+10+16+10+10=84
			// total for pause: 103+41+84=228t

endtrack	//end of track
			pop	 hl
			jr mus_init
			// total: 103+41+5+10+12=171t

			//play note
trb_play	
pl_track	ld hl, 0				

			ld a, (hl)
			ld b, a
			add a
			jr nc, pl_frame				; 10+7+4+4+7=32t

			// Process ref
			inc hl
			ld c, (hl)
			add a
			inc hl
			jr nc, pl10					; 6+7+4+6+7=30t

pl11		ld a, (hl)						
			ld (trb_rep+1), a		
			ld (trb_rest+1), hl			; 7+13+16=36t

			add hl, bc
			ld a, (hl)
			add a		            

			call pl0x
			ld (pl_track+1), hl		
			ret								; 11+7+4+17+16+10=65t
			// total: 32+30+36+65=163t + pl0x time(670t) = 833t(max)

pl10
			ld (pl_track+1), hl		
			set 6, b
			add hl, bc

			ld a, (hl)
			add a		            
			jp pl0x							; 16+11+7+4+10=58t
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
			ld (pl_track+1), hl
			ret								; 10+16+10=36t
			// total: 36+5+37+36=114t

pl00		sub 120
			jr nc, pl_pause
			ld de, #ffbf
		//psg1
			// 2 registr - maximum, second without check
			ld a, (hl)
			sub #10
			jr nc, 7f
			outi
			ld b, e
			outi
			ld a, (hl)
7			inc hl
			ld b, d
			out (c),a
			ld b, e
			outi
			ret						; 7+7+10+7+7+7+16+4+16+7+6+4+12+4+16+10=140t

pl0x		ld bc, #fffd				
			add a					
			jr nc, pl00

pl01	// player PSG2
			inc hl
			ld de, #01bf
			jr z, play_all_0_6		; 10+4+7+6+10+7=44t
play_by_mask
			add a				
			jr c,1f
			out (c), 0
			ld b,e
			outi				
1									; 4+7+12+4+16=43
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
			jr c, psg2_continue

			ld b,#ff
			out (c),d
			ld b,e
			outi					; 4+7+7+12+4+16=50
			jp psg2_continue
			// total:  regular = 44+43+138+50+10=285

play_all_0_6
			cpl						; 0->ff

			ld b, a
			out (c), 0
			ld b,e
			outi					; 4+4+12+4+16=40

			dup 4
				ld b, a
				out (c),d
				ld b,e
				outi				
				inc d				
			edup					; 40

			ld b, a
			out (c),d
			ld b,e
			outi				; 5*40+36  = 236
			// total:  play_all_0_6 = 44+5+236=285

psg2_continue
			ld a, (hl)
			inc hl					
			ld	 d, 13

			add a
			jr z, play_all_12_6		; 7+6+7+4+7=31

			jr c,1f
			ld b,#ff
			out (c),d
			ld b,e
			outi					;  7+7+12+4+16=46
1			
			dup 6
				dec d
				add a
				jr c,1f
				ld b,#ff
				out (c),d
				ld b,e
				outi				
1									; 54*4 + 15*2=246
			edup

 			add a
			ret c
			dec d
			ld b,#ff
			out (c),d
			ld b,e
			outi					
			ret						; 4+5+4+7+12+4+16+10=62
			// total: 285+31+46+246+62=670

play_all_12_6
			cpl						; 0->ff, keep flag c

			jr c,1f					; Don't touch reg 13 if it unchanged in playAll mode as well
			ld b, a
			out (c),d
			ld b,e
			outi					;  4+7+4+12+4+16=47
1
			dup 7
				dec d				
				ld b, a
				out (c),d
				ld b,e
				outi				; 7*40=280
			edup

			ret						
			// total: 289+5+47+280+10=631

			DISPLAY	"player code occupies ", /D, $-init, " bytes"
