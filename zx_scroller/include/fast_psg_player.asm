/*
Player for Fast PSG Packer for compression levels [0..3]
Source for sjasm cross-assembler.
Source code is based on psndcj/tbk player and bfox/tmk player.
Modified by physic 8.12.2021.
Max time is reduced from 1089t to 799t (-290t) for compression level 1.
Player size is increased from 348 to 485 bytes.

11hhhhhh llllllll nnnnnnnn	3	CALL_N - вызов с возвратом для проигрывания (nnnnnnnn + 1) значений по адресу 11hhhhhh llllllll
10hhhhhh llllllll			2	CALL_1 - вызов с возвратом для проигрывания одного значения по адресу 11hhhhhh llllllll
01MMMMMM mmmmmmmm			2+N	PSG2 проигрывание, где MMMMMM mmmmmmmm - инвертированная битовая маска регистров, далее следуют значения регистров.
							во 2-м байте также инвертирован порядок следования регистров (13..6)

001iiiii 					1	PSG2i проигрывание, где iiiii номер индексированной маски (0..31), далее следуют значения регистров
0001pppp					1	PAUSE16 - пауза pppp+1 (1..16)
0000hhhh vvvvvvvv			2	PSG1 проигрывание, 0000hhhh - номер регистра + 1, vvvvvvvv - значение
00001111					1	маркер оцончания трека
00000000 nnnnnnnn			2	PAUSE_N - пауза nnnnnnnn+1 фреймов (ничего не выводим в регистры)


Также эта версия частично поддерживает короткие вложенные ссылки уровня 2 (доп. ограничение - они не могут стоять в конце длинной ссылки уровня 1).
По-умолчанию пакер избегает пакованных фреймов, когда заполнены 5/6 регистров [0..5] или 5/7, 6/7 регистров [6..12]. В этом случае записывается "лишний" регистр(ы).
Т.о. проигрывание идет по ветке play_all_xx, что быстрее.
Дополнительно, эта же опция пакера избегает сочетания "заполнены все регистры(в том числе после заливки доп. регистров) + ссылка длиной более 1 байт".
Все это несколько ухудшает сжатие, но за счет частичной поддержки вложенных ссылок, оно остается на уровне оригинального плейера.
Максимальные тайминги расчитаны при уровне компрессии 1 (по-умолчанию).
Лупинг также не выходит за пределы макс. расчитанных таймингов, но формирует отдельную запись проигрывания, т.е. есть задержка между последним и 1-м фреймом трека в 1 frame.
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
			ret m
			jr 1b
		
mus_init	ld	 a, l
			ld	 (mus_low+1), a
			ld	 a, h
			ld	 (mus_high+1), a
			ld	de, 16*4
			add	 hl, de
			ld (pl_track+1), hl
			xor a
			ld (trb_rep+1), a
			ld a, LD_HL_CODE
			ld (trb_play), a
			ret							; 10+16+4+13+7+13+10=73
			// total for looping: 171+73=244

pause_rep	db 0
trb_pause	ld hl, pause_rep
			dec	 (hl)
			ret nz						; 10+11+5=26t

saved_track	
			ld hl, LD_HL_CODE			; end of pause
			ld (trb_play), hl
			jr trb_rep					; 10+16+12=38t
			// total: 34+38=72t

endtrack	//end of track
			jp 00
			//play note
trb_play	
pl_track	ld hl, 0				

			ld a, (hl)
			add a
			jr c, pl1x					    ; 10+7+4+7=28t

pl_frame	call pl0x
after_play_frame
			ld (pl_track+1), hl				;17+16=33t
			
trb_rep		ld a, 0						
			dec	 a
			ret m
			ld (trb_rep+1), a
			ret nz							; 7+4+5+13+5=34t
			// end of repeat, restore position in track
trb_rest	ld hl, 0
			inc hl
			ld (pl_track+1), hl
			ret								; 10+6+16+10=42t
			// total: 28+33+34+42=137t + pl0x time(661t) = 798t(max)

pl1x		// Process ref	
			ld b, (hl)
			inc hl
			ld c, (hl)
			inc hl
			jp p, pl10					; 7+6+7+6+10=36t

pl11		ld a, (hl)						
			ld (trb_rep+1), a		
			ld (trb_rest+1), hl			; 7+13+16=36t

			add hl, bc
			ld a, (hl)
			add a		            

			call pl0x
			ld (pl_track+1), hl		
			ret								; 11+7+4+17+16+10=65t
			// total: 28+5+36+36+65=170t + pl0x time (661-32)=799t (max pl0x time is blocked here by packer for level 1)

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
pause_cont	ld (pause_rep), a
			ld (pl_track+1), hl
			ld  a, l
			ld (saved_track+2), a

			ld hl, JR_CODE + (trb_pause - trb_play - 2) * 256
			ld (trb_play), hl
			
			pop	 hl						
			ret							; 4+4+13+4+13+10+16+10+10=84
			// total for pause: 94+41+84=219t

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
			ret							; 12+7+16+10=45

pl00		add	 a
			jr	 nc, pause_or_psg1
			ld de, #05bf
		// psg2i
			rrca:rrca						; 4+7+10+4+4=29

			exx
mus_low		add	 0
			ld	 e, a
mus_high	adc	 0
			sub	 e
			ld	 d, a					
			ld	 a,(de)
			inc	 de
			exx							
			inc	 hl						;  4+7+4+7+4+4+7+6+4+6=53
			call reg_left_6

			exx
			ld	 a, (de)
			exx
			add a
			ld b,#ff
			jp play_by_mask_13_6		; 4+7+4+4+7+10=36

pl10
			ld (pl_track+1), hl		
			set 6, b
			add hl, bc

			ld a, (hl)
			add a		            	; 16+8+11+7+4=46t
			// total: 28+5+36+46=115t + pl0x time(661t) = 776t(max)

pl0x		ld bc, #fffd				
			add a					
			jr nc, pl00				; 10+4+7=21t

pl01	// player PSG2
			inc hl
			ld de, #00bf
			jr z, play_all_0_5		; 21+6+10+7=44t
play_by_mask_0_5

			dup 5
				add a
				jr c,1f
				out (c),d
				ld b,e
				outi				
				ld b,#ff
1				inc d
			edup					;54*3 + 20*2=202

			add a
			jr c, play_all_0_5_end	; 44+54*4+20+ 4 + 12=296 (timing at play_all_0_5_end)
			out (c),d
			ld b,e
			outi					; 4+7+12+4+16=43

			ld a, (hl)
			inc hl					
			add a
			jr z,play_all_6_13		; 7+6+4+7=24
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

			DISPLAY	"player code occupies ", /D, $-stop, " bytes"
