//player for TBK PSG Packer
//psndcj//tbk - 11.02.2012,01.12.2013
//source for sjasm cross-assembler

/*
10hhhhhh llllllll nnnnnnnn	3	CALL_N - вызов с возвратом для проигрывания (nnnnnnnn + 1) значений по адресу 11hhhhhh llllllll
11hhhhhh llllllll			2	CALL_1 - вызов с возвратом для проигрывания одного значения по адресу 11hhhhhh llllllll
01MMMMMM mmmmmmmm			2+N	PSG2 проигрывание, где MMMMMM mmmmmmmm - битовая маска регистров, далее следуют значения регистров

00111100..00011110          1	PAUSE32 - пауза pppp+1 (1..32, N + 120)
00111111					1	маркер оцончания трека

0001hhhh vvvvvvvv			2	PSG1 проигрывание, 1 register, 0000hhhh - номер регистра, vvvvvvvv - значение
0000hhhh vvvvvvvv			4	PSG1 проигрывание, 2 registers, 0000hhhh - номер регистра, vvvvvvvv - значение

*/

							; MAX DURATION				; MIN DURATION
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

		
// 2 registr - maximum, second without check

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
			
pl00		sub 120
			jr nc, pl_pause
			ld de,#ffbf
		//psg1
			ld a, (hl)
			and #0f
			cp (hl)
			jp nz, 7f
			ld b,d
			outi
			ld b, e
			outi
			ld a,(hl)
			and #0f
7			inc hl
			ld b,d
			out (c),a
			ld b,e
			outi
			//jp trb_end	
			ret


trb_play		//play note
pl_track	ld hl, 0					; 10t (10+10 = 20t)
inside		// single repeat
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

			ld (pl_track+1), hl		
			add hl, bc

			ld a, (hl)
			add a		            
			call pl0x					; 16+11+7+4+17 = 65t (total 127t)
			
trb_rep		ld a, 0						
			sub 1
			ret c
			ld (trb_rep+1), a
			ret nz						; 7+7+5+13+5 = 37t (total 164t)
			// end of repeat, restore position in track
trb_rest	ld hl, 0

/*
			// TODO: Nested refs
			ld hl, (stack_ptr + 1)  // +6
			dec l
			ld d, (hl)
			dec l
			ld e, (hl)
			ld (stack_ptr + 1), hl
			ex de, hl
			// total: +42t
*/
			ld (trb_play+1), hl		; 
			ret						; 10+16+10=36t (total 200t + pl0x time)



pl_frame	call pl0x
			ld (pl_track+1), hl				; 16t (927+53+16 = 996t)
			jr trb_rep

pl10		set 6, b		    ; 7+6+7+8=28t
			ld a, (hl)
			ld (trb_rep+1), a		; 16+13=29t
			inc hl
			
			ld (trb_rest+1), hl

/*
			TODO: nested refs
			ex de, hl
stack_ptr	ld hl,  rest_data
			ld (hl), e
			inc l
			ld (hl), d
			inc l
			ld (stack_ptr +1), hl
			ex de, hl
			// total: +40t
*/			

			add hl, bc
			ld a, (hl)
			add a		            ; 15+6+7+4=32t (84+21+29+32 = 166t)

			call pl0x
			ld (pl_track+1), hl			; 16t (927+53+16 = 996t)
			ret


pl0x		ld c, #fd				; 7t
			add a
			jr nc, pl00				; 4+7=11t
pl01			// player PSG2
			inc hl
			ld de,#00bf					; 10t (166+7+11+27+10 = 221t)

			dup 6
				add a
				jr nc,1f
				ld b,#ff
				out (c),d
				ld b,e
				outi	; 4+7+7+12+4+16+4=54t (221+54*6 = 545t)
1				inc d
			edup

			ld a, (hl)
			inc hl

			dup 7
				add a
				jr nc,1f
				ld b,#ff
				out (c),d
				ld b,e
				outi	; 54t (545+4+7*54 = 927t)
1				inc d
			edup

 			add a
			ret nc
			ld b,#ff
			out (c),d
			ld b,e
			outi	//!!! 4+10+7+12+4+16=53t

			ret

/*
			TODO: nested refs
rest_data	DW 0, 0, 0, 0	// Maximum nested level for refs is 4
*/

			DISPLAY	"player code occupies ", /D, $-init, " bytes"
