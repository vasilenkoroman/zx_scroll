//player for Physic PSG Packer
//psndcj//tbk - 5.12.2021
//source for sjasm cross-assembler

            align 256
init		jr mus_init
play		jp play_frame					; 10t					; 10t


pl_pause    
            ld   b, a
            ld   a, (pl_counter)
            or   a
            jr   nz, pause_continue
pause_init  ld a, b
            rra: rra
            ld   (pl_counter), a
pause_continue
            dec  a
            ld   (pl_counter), a
            ret  nz

            inc hl
            ld  (curPos), hl
            ret

mus_init	
            ld   (curPos), hl
			ret


pl_psg1
            and  0x0f
            ld   bc, #FFFD
            out (c), a
            ld   b, #BF
            outi
			
            jp pl_finish

pl_full_psg2_1:
			dup 5
                ld   b, #ff
				out (c), d
                ld   b, e
				outi
	    	    inc d
			edup
			dup 1
                ld   b, #ff
				out (c), d
                ld   b, e
				outi
			edup
            jp   pl_psg2_2

play_frame
            ld   hl, (curPos)
            ld   a, (hl)

            // Execute command in high 2 bits:
            // 11 - ref to previous frames command
            // 10 - pause
            // 00 - Play PSG2
            // 01 - Play PSG1

            add  a
            jr   c, pl_ref_pause

            add  a
            inc  hl

            jr   c, pl_psg1
            jp   pl_psg2

pl_ref_pause
            add  a
            jr   nc, pl_pause
pl_ref      
            ld   d, (hl)
            inc  hl
            ld   e, (hl)
            inc  hl
            ld   a, (hl)            // size and psg1/psg2 switch in low bit
            ld   (pl_counter), a
            inc  hl
            ld   (prevPos), hl
            
            add  hl, de
            ld   a, (hl)
            inc  hl
            add  a: add  a

            jr   c, pl_psg1

pl_psg2
            ld   c, #fd
            ld   de, #00BF
            jr z, pl_full_psg2_1
			dup 6
				add  a
				jr c, 1F
                ld   b, #ff
				out (c), d
                ld   b, e
				outi
1	    	    inc d
			edup
pl_psg2_2
            ld   a, (hl)
            inc  hl
            or   a
            jr   z, pl_full_psg2_2

			dup 7
				add  a
				jr c, 1F
                ld   b, #ff
				out (c), d
                ld   b, e
				outi
1	    	    inc d
			edup
			dup 1
				add  a
				jr c, 1F
                ld   b, #ff
				out (c), d
                ld   b, e
                outi
1	    	    
			edup
            jp   pl_finish

pl_full_psg2_2:
			dup 6
                ld   b, #ff
				out (c), d
                ld   b, e
				outi
	    	    inc d
			edup
			dup 1
                ld   b, #ff
				out (c), d
                ld   b, e
				outi
1	    	    
			edup
            jp  pl_finish

pl_finish   ld   a, (pl_counter)
            sub  1
            jr   c, pl_end  // no counter
            ld   (pl_counter), a
            jr   nz, pl_end
            ld hl, (prevPos)
pl_end:     ld   (curPos), hl
            ret

curPos      DW 0
prevPos     DW 0
pl_counter  DW 0
