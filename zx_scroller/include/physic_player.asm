//player for TBK PSG Packer
//psndcj//tbk - 11.02.2012,01.12.2013
//source for sjasm cross-assembler
//modified by physic 8.12.2021
//Max time reduced from 1089t to 911t (-178t)

/*
11hhhhhh llllllll nnnnnnnn	3	CALL_N - вызов с возвратом для проигрывания (nnnnnnnn + 1) значений по адресу 11hhhhhh llllllll
10hhhhhh llllllll			2	CALL_1 - вызов с возвратом для проигрывания одного значения по адресу 11hhhhhh llllllll
01MMMMMM mmmmmmmm			2+N	PSG2 проигрывание, где MMMMMM mmmmmmmm - инвертированная битовая маска регистров, далее следуют значения регистров

00111100..00011110          1	PAUSE32 - пауза pppp+1 (1..32, N + 120)
00111111					1	маркер оцончания трека

0001hhhh vvvvvvvv			2	PSG1 проигрывание, 1 register, 0000hhhh - номер регистра, vvvvvvvv - значение
0000hhhh vvvvvvvv			4	PSG1 проигрывание, 2 registers, 0000hhhh - номер регистра, vvvvvvvv - значение

*/

							
init		ld hl,music
			jr mus_init
play		jp trb_play					; 10t					; 10t
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
			ld (trb_play+1), hl
			xor a
			ld (trb_rep+1),a
			ld a,low trb_play
			ld (play+1),a
			jr stop

trb_pause	//pause - skip frame
			ld a, 0
			dec a
			ld (trb_pause+1), a
			ret nz
								; 7+4+13+10=34t (10+34 = 44t)
			ld a, low trb_play
			ld (play+1), a
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
			ld a, low trb_pause
			ld (play+1), a
			
			ret
//
endtrack	//end of track
			call init
			jp trb_play
			
trb_play		//play note
pl_track	ld hl, 0					; 10t (10+10 = 20t)

			ld a, (hl)
			ld b, a
			add a
			jr nc, pl_frame				; 7+4+4+7 = 22t (total 42t)

			// Process ref

			inc hl
			ld c, (hl)
			add a
			inc hl
			jr nc, pl10					; 6+7+4+6+7 = 30t  (total 72t)

pl11		ld a, (hl)						
			ld (trb_rep+1), a		
			ld (trb_rest+1), hl				; 7+13+16=36t

			add hl, bc
			ld a, (hl)
			add a		            

			call pl0x
			ld (pl_track+1), hl		
			ret								; 11+7+4+17+16+10=65t
			// total: 72+36+65=173t
			// 	+ pl0x time(738t) = 911t(max)

pl10
			ld (pl_track+1), hl		
			set 6, b
			add hl, bc

			ld a, (hl)
			add a		            
			jp pl0x					; 16+11+7+4+10 = 58t
			// total: 72+8+5+58=143t
			

pl_frame	call pl0x
			ld (pl_track+1), hl				
			
trb_rep		ld a, 0						
			sub 1
			ret c
			ld (trb_rep+1), a
			ret nz							; 7+7+5+13+5 = 37t
			// end of repeat, restore position in track
trb_rest	ld hl, 0
			inc hl
			ld (trb_play+1), hl		; 
			ret								; 10+16+10=36t, 73 for rep/rest



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

play_all1
			cpl
			dup 6
				ld b, a
				out (c),d
				ld b,e
				outi				
1				inc d				; 6*43t
			edup
			jr psg2_continue

pl0x		ld bc, #fffd				
			add a					
			jr nc, pl00
pl01			// player PSG2
			inc hl
			ld de,#00bf
			jr z, play_all1			; 10+4+7+6+10+7=44t

			add a
			jr c,1f
			out (c),d
			ld b,e
			outi				
1			inc d				

			dup 5
				add a
				jr c,1f
				ld b,#ff
				out (c),d
				ld b,e
				outi				
1				inc d				
			edup					; 47+ 4*54+15 = 278t max	(play_all1 261t with ret)
								; 44 + 278 = 322t
			
psg2_continue
			ld a, (hl)
			inc hl					

			add a
			jr c,1f
			jr z, play_all2

			ld b,#ff
			out (c),d
			ld b,e
			outi	
1			inc d					; 7+6+4+7+7+7+12+4+16+4=74t, 74t + 322t = 396t

			dup 6
				add a
				jr c,1f
				ld b,#ff
				out (c),d
				ld b,e
				outi	
1				inc d				; 54*5+14=284 + 396 = 680t
			edup

 			add a
			ret c
			ld b,#ff
			out (c),d
			ld b,e
			outi					
			ret						; 4+5+7+12+4+16+10=58 + 680 = 738t

play_all2
			cpl
			dup 7
				ld b, a
				out (c),d
				ld b,e
				outi				
1				inc d				; 6*43t
			edup

			ld b, a
			out (c),d
			ld b,e
			outi					
			ret

/*
			TODO: nested refs
rest_data	DW 0, 0, 0, 0	// Maximum nested level for refs is 4
*/

			DISPLAY	"player code occupies ", /D, $-init, " bytes"
