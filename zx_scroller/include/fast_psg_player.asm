//player for TBK PSG Packer
//psndcj//tbk - 11.02.2012,01.12.2013
//source for sjasm cross-assembler
//modified by physic 8.12.2021
//Max time is reduced from 1089t to 799t (-290t)
//Player size is increased from 348 to 470 bytes (+122 bytes)

/*
11hhhhhh llllllll nnnnnnnn	3	CALL_N - вызов с возвратом для проигрывания (nnnnnnnn + 1) значений по адресу 11hhhhhh llllllll
10hhhhhh llllllll			2	CALL_1 - вызов с возвратом для проигрывания одного значения по адресу 11hhhhhh llllllll
01MMMMMM mmmmmmmm			2+N	PSG2 проигрывание, где MMMMMM mmmmmmmm - инвертированная битовая маска регистров, далее следуют значения регистров.
							во 2-м байте также инвертирован порядок следования регистров (13..6)

00111100..00011110          1	PAUSE32 - пауза pppp+1 (1..32, N + 120)
00111111					1	маркер окончания трека

000hhhh1 vvvvvvvv			1	PSG1 проигрывание, 1 register, 	hhhh - номер регистра, vvvvvvvv - значение
000hhhh0 					1	PSG1i проигрывание, 2 registers, hhhh - номер индексной записи в начале файла

00111101					1   Not used.
00111110					1   Not used.


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
			jr nz,1b
			ret
		
mus_init	ld hl, music
			ld	 a, l
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
		
// pause or end track
pl_pause								; 90 on enter
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
			
			pop	 hl						
			ret							; 4+4+13+4+13+10+16+10+10=84
			// total for pause: 94+41+84=219t

endtrack	//end of track
			pop	 hl
			jr mus_init
			// total: 103+41+5+10+12=171t

			//play note
trb_play	
pl_track	ld hl, 0				

			ld a, (hl)
			add a
			jr c, pl1x					    ; 10+7+4+7=28t

pl_frame	call pl0x
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

pl00		sub 120						; 28+17+21+5=71 on enter
			jr nc, pl_pause
		//psg1
			// 2 registr - maximum, second without check
			ld a, (hl)
			inc hl
			rrca
			jr nc, 7f					; 7+7+7+6+4+7=38
			ex de, hl
			add	 a
			add	 a
mus_low		add	 0
			ld	 l, a
mus_high	adc	 0
			sub	 l
			ld	 h, a					; 4+4+4+7+4+7+4+4=38

			outi
			ld b, #bf
			outi
			ld b, #ff
			outi
			ld b, #bf
			outi
			ex	 de, hl
			ret							; 16+(7+16)*3+4+10=99
			; total: 38+38+99=175
7			out (c),a
			ld b, #bf
			outi
			ret							; 12+7+16+10=45
			; total: 38+5+45=88

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
			dup 6
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
