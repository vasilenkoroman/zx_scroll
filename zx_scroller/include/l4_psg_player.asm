/*
Player for Fast PSG Packer for compression levels [4..5]
Source for sjasm cross-assembler.
Source code is based on psndcj/tbk player and bfox/tmk player.
Modified by physic 8.12.2021.
Max time is 930t for compression level 4 (recomended), 1032t for compression level 5
Player size is increased from 348 to 543 bytes.

11hhhhhh llllllll nnnnnnnn	3	CALL_N - вызов с возвратом для проигрывания (nnnnnnnn + 1) значений по адресу 11hhhhhh llllllll
10hhhhhh llllllll			2	CALL_1 - вызов с возвратом для проигрывания одного значения по адресу 11hhhhhh llllllll
01MMMMMM mmmmmmmm			2+N	PSG2 проигрывание, где MMMMMM mmmmmmmm - инвертированная битовая маска регистров, далее следуют значения регистров.
							во 2-м байте также инвертирован порядок следования регистров (13..6)

001iiiii 					1	PSG2i проигрывание, где iiiii номер индексированной маски (0..31), далее следуют значения регистров
0001pppp					1	PAUSE16 - пауза pppp+1 (1..16)
0000hhhh vvvvvvvv			2	PSG1 проигрывание, 0000hhhh - номер регистра + 1, vvvvvvvv - значение
00001111					1	маркер оцончания трека
00000000 nnnnnnnn			2	PAUSE_N - пауза nnnnnnnn+1 фреймов (ничего не выводим в регистры)


Nested loops are fully supported at this version. Please make sure there is enough room for nested levels.
Packer shows max nested level. It need to increase MAX_NESTED_LEVEL variable if it not enough.
*/

MAX_NESTED_LEVEL EQU 4

LD_HL_CODE	EQU 0x2A
JR_CODE		EQU 0x18

			MACRO SAVE_POS
				ex	de,hl
				ld	hl, (pl_track+1)
				ld	(hl),e
				inc	l
				ld	(hl),d						; 4+16+7+4+7=38t
			ENDM
							
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
			ld	 a, l
			ld	 (mus_low+1), a
			ld	 a, h
			ld	 (mus_high+1), a
			ld	de, 16*4
			add	 hl, de
			ld (stack_pos+1), hl
			ld a, LD_HL_CODE
			ld (trb_play), a

			xor a
			ld hl, stack_pos
			ld (hl), a
			inc hl

			ld (pl_track+1), hl
			ret							; 10+4+13+4+13+10+11+16+10+13+4+10+7+6=302
			// total for looping: 171+131=244

pause_rep	db 0
trb_pause	ld hl, pause_rep
			dec	 (hl)
			ret nz						; 10+11+5=26t

saved_track	
			ld hl, LD_HL_CODE			; end of pause
			ld (trb_play), hl
			ld	hl, (trb_play+1)
			jr trb_rep					; 10+16+12=38t
			// total: 34+38=72t

endtrack	//end of track
			pop	 hl
			jr mus_init
			// total: 103+41+5+10+12=171t

pl_frame	call pl0x						; 17
after_play_frame
			xor	 a
			ld	 (stack_pos), a				
			SAVE_POS 						
			dec	 l							; 4+13+38+4=59
trb_rep		dec	 l
			dec (hl)
			ret	 nz							; 4+11+5=20
trb_rest	
			dec	 l
			dec	 l
			ld	 (pl_track+1), hl
			ret								; 4+4+16+10=34
			// total: 28+5+17+59+20+34=163  + pl0x time(661t) = 824t(max)

trb_play	
pl_track	ld hl, (stack_pos+1)
			ld a, (hl)
			add a
			jr nc, pl_frame				    ; 16+7+4+7=34t
pl1x		// Process ref	
			ld b, (hl)
			inc hl
			ld c, (hl)
			inc hl
			jp p, pl10						; 7+6+7+6+10=36t

pl11		
			ld a, (hl)			
			inc hl	
			ex	de,hl
			ld  hl, (pl_track+1)
			dec	 l
			dec (hl)
			jr	 z, same_level_ref			; 7+6+4+16+4+11+7=55
nested_ref
			// Save pos for the current nested level
			inc	 l
			ld	(hl),e
			inc	l
			ld	(hl),d
			inc  l							; 4+7+4+7+4+=26
same_level_ref
			ld	 (hl),a
			inc	 l
			// update nested level
			ld	 (pl_track+1),hl			; 7+4+16=27

			ex	de,hl					
			add hl, bc	
			ld a, (hl)
			add a		            		; 4+11+7+4=26
			call pl0x						; 17
			// Save pos for the new nested level
			SAVE_POS 						; 38
			ret							 
			// total: 34+36+55+26+27+26+17+38+10=269t + pl0x time (661)=930t

single_pause
			pop	 de
			jp	 after_play_frame
long_pause
			inc	 hl
			ld	 a, (hl)
			inc hl
			jr	 pause_cont
pl_pause	and	 #0f
			inc hl
			jr z, single_pause
pause_cont	
			//set pause
			ld (pause_rep), a	
			//SAVE_POS
			ex	de,hl
			ld	hl, (pl_track+1)
			ld  a, l
			ld (saved_track+2), a			;13+4+16+4+13=50

			ld	(hl),e
			inc	l
			ld	(hl),d						
			ld hl, JR_CODE + (trb_pause - trb_play - 2) * 256
			ld (trb_play), hl				; 7+4+7+10+16=44
			
			pop	 hl						
			ret								; 50+44+10+10=114

pause_or_psg1
			add	 a
			ld a, (hl)
			jr c, pl_pause
			jr z, long_pause
		//psg1 or end of track
			cp #0f
			jr z, endtrack
			dec a	 
			inc hl

			out (c),a
			ld b, #bf
			outi
			ret								; 12+7+16+10=45

pl00		add	 a
			jr	 nc, pause_or_psg1
			ld de, #05bf
		// psg2i
			rrca:rrca						; 4+5+10+4=23

			exx
mus_low		add	 0
			ld	 e, a
mus_high	adc	 0
			sub	 e
			ld	 d, a					
			ld	 a,(de)
			inc	 de
			exx							
			inc	 hl							; 4+7+4+7+4+4+7+6+4+6=53
			call reg_left_6

			exx
			ld	 a, (de)
			exx
			add a
			ld b,#ff
			jp play_by_mask_13_6

			; total: 5+23+47+27+25 - 6-10-7-6-7-7-10 = 74 (longer that PSG2)

pl10
			SAVE_POS 						; 38

			ex	de,hl
			set 6, b
			add hl, bc

			ld a, (hl)
			add a		            		; 4+8+11+7+4=34
			
			call pl0x
			ld	hl, (pl_track+1)
			jp trb_rep						; 17+16+10=43
											; trb_rep=54
			// total:  142+43+54=239t  + pl0x time(661t) = 900t(max)


pl0x		ld bc, #fffd				
			add a					
			jr nc, pl00						; 10+4+7=21t

pl01	// player PSG2
			inc hl
			ld de, #00bf
			jr z, play_all_0_5				; 21+6+10+7=44t
play_by_mask_0_5

			dup 5
				add a
				jr c,1f
				out (c),d
				ld b,e
				outi				
				ld b,#ff
1				inc d
			edup							; 54*3 + 20*2=202

			add a
			jr c, play_all_0_5_end			; 44+54*4+20+ 4 + 12=296 (timing at play_all_0_5_end)
			out (c),d
			ld b,e
			outi							; 4+7+12+4+16=43

second_mask	ld a, (hl)
			inc hl					
before_6    add a
			jr z,play_all_6_13				; 7+6+4+7=24
			// total: 44+202+43+24+5=318  (till play_all_6_13)
			ld b,#ff
			jp play_by_mask_13_6
			//  total: 318-5+7+10=330 (play_by_mask_13_6)

play_all_0_5
			cpl						; 0->ff
			out (c),d
			ld b,e
			outi				
			inc d					; 40

			dup 4
				ld b, a
				out (c),d
				ld b,e
				outi				
				inc d				
			edup					; 40*4

			ld b, a
			out (c),d
			ld b,e
			outi					
			ld	 b,a				; 5*40+40  = 240
			// total:  play_all_0_5 = 44+5+240=289

play_all_0_5_end
			ld a, (hl)
			inc hl					
			add a
			jr nz,play_by_mask_13_6	; 7+6+4+7=24
			//  total: 296+24+5=325/318 (till play_by_mask_13_6)
			//  total: 296+24=320/313 (till play_all_6_13)
play_all_6_13
			cpl						; 0->ff, keep flag c
			// write regs [6..12] or [6..13] depend on flag
			jr	 c, 1f				; 4+7=11
			dup 8
				inc d				
				ld b, a
				out (c),d
				ld b,e
				outi				; 8*40=320
1				
			edup
			ret						; 11+320+10=341
			// total: 313 + 341 = 654 (all_0_5 + all_6_13)
			// total: 320 + 341 = 661 (mask_0_5 + all_6_13)

play_by_mask_13_6
			ld	d, 13
			jr c,1f
			out (c),d
			ld b,e
			outi					
			ld b,#ff				;  7+7+12+4+16+7=53
1			

			dec d
			add a
			jr c,1f
			out (c),d
			ld b,e
			outi				
			ld b,#ff
1			

			dec d
reg_left_6	add a
			jr c,1f
			out (c),d
			ld b,e
			outi				
			ld b,#ff
1			
			dup 4
				dec d
				add a
				jr c,1f
				out (c),d
				ld b,e
				outi				
				ld b,#ff
1									; 54*3 + 20*3=222
			edup

 			add a
			ret c
			dec d
			out (c),d
			ld b,e
			outi					
			ret						; 4+5+4+12+4+16+10=55, 53+222+55 = 330
			// total: 318 + 330 = 648 (all_0_5 + mask_6_13)
			// total: 325 + 330 = 655 (mask_0_5 + mask_6_13)

stack_pos	
			dup MAX_NESTED_LEVEL		// Make sure packed file has enough nested level here
				DB 0	// counter
				DW 0	// HL value (position)
			edup
stack_pos_end
			ASSERT high(stack_pos) == high(stack_pos_end), Please move player code in memory for several bytes.
			DISPLAY	"player code occupies ", /D, $-stop, " bytes"
