//player for TBK PSG Packer
//psndcj//tbk - 11.02.2012,01.12.2013
//source for sjasm cross-assembler
//modified by physic 8.12.2021
//Max time is reduced from 1089t to 802t (-287t)
//Player size is increased from 348 to 442 bytes (+94 bytes)

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
По-умолчанию пакер избегает пакованных фреймов, когда заполнены 5/6 регистров [0..5] или 5/7, 6/7 регистров [6..12]. В этом случае записывается "лишний" регистр(ы).
Т.о. проигрывание идет по ветке play_all_xx, что быстрее.
Дополнительно, эта же опция пакера избегает сочетания "заполнены все регистры(в том числе после заливки доп. регистров) + ссылка длиной более 1 байт".
Все это несколько ухудшает сжатие, но за счет частичной поддержки вложенных ссылок, оно остается на уровне оригинального плейера.
Максимальные тайминги расчитаны при включенной опции пакера 'fast'. Если ее отключить, то сжатие улучшится, но максимальные тайминги поднимутся примерно на 120t (пока точно не считал).
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
pl_pause								; 58+7+12=77 on enter
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
			// total for pause: 77+41+84=202t

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
			jr c, pl1x					    ; 10+7+4+4+7=32t

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
			// total: 32+33+34+42=141t + pl0x time(661t) = 802t(max)

pl1x		// Process ref	
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
			// total: 32+5+30+36+65=168t + pl0x time (661-30)=799t (max pl0x time is blocked here by packer)

pl00		sub 120						; 32+21+5=58 on enter
			jr nc, pl_pause
			ld de, #ffbf
		//psg1
			// 2 registr - maximum, second without check
			ld a, (hl)
			sub #10
			jr nc, 7f					; 7+7+10+7+7+7=45
			outi
			ld b, e
			outi
			ld a, (hl)
7			inc hl
			ld b, d
			out (c),a
			ld b, e
			outi
			ret							; 45+16+4+16+7+6+4+12+4+16+10=140t

pl10
			ld (pl_track+1), hl		
			set 6, b
			add hl, bc

			ld a, (hl)
			add a		            	; 16+8+11+7+4=46t
			// total: 32+5+30+5+46=118t + pl0x time(661t) = 779t(max)

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
			//  total: 296+24+5=325 (till play_by_mask_13_6)
			//  total: 296+24=320 (till play_all_6_13)
play_all_6_13
			cpl						; 0->ff, keep flag c
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
			// total: 320 + 341 = 661 (all_0_5 + all_6_13)
			// total: 318 + 341 = 659 (mask_0_5 + all_6_13)

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
			// total: 325 + 330 = 655 (all_0_5 + mask_6_13)
			// total: 330 + 330 = 660 (mask_0_5 + mask_6_13)

			DISPLAY	"player code occupies ", /D, $-stop, " bytes"
