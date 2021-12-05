//player for Physic PSG Packer
//psndcj//tbk - 5.12.2021
//source for sjasm cross-assembler

            align 256
init		jr mus_init
play		jp play_frame					; 10t					; 10t

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
            jr   c, pl_psg1

pl_psg1
            // TODO: implement me
            ret

pl_pause    
            ld   b, a
            ld   a, (pl_counter)
            dec  a
            ret  nc 
            cp   0xff
            jr   z, pl_pause_init

pl_pause_finis
            inc  hl
            ld   (pl_counter), a
            jp   pl_finish

pl_pause_init            
            ld a, b
            ld   (pl_counter), a
            ret


pl_ref_pause
            add  a
            jr   nc, pl_pause
pl_ref      
            ld   d, (hl)
            inc  hl
            ld   e, (hl)
            add  hl, de
            inc  hl
            ld   a, (hl)            // size and psg1/psg2 switch in low bit
            inc  hl
            ccf
            rr  a
            ld   (pl_counter), a
            jr   c, pl_psg1

mus_init	
            ld   (curPos), hl
			ret


pl_full_psg2_1:
			dup 5
				out (c), d
				exx
				outi
                exx
	    	    inc d
			edup
			dup 1
				out (c), d
				exx
				outi
                exx
1	    	    
			edup
            jp   pl_psg2_2

pl_psg2
            ld   bc, #FFFD
            exx
            ld   bc, #BFFD
            exx
            ld   d, 0
            jr z, pl_full_psg2_1
			dup 5
				add  a
				jr nc, 1F
				out (c), d
				exx
				outi
                exx
1	    	    inc d
			edup
			dup 1
				add  a
				jr nc, 1F
				out (c), d
				exx
				outi
                exx
1	    	    
			edup
pl_psg2_2
            ld   d, 6
            ld   a, (hl)
            inc  hl
            or   a
            jr   z, pl_full_psg2_2

			dup 6
				add  a
				jr nc, 1F
				out (c), d
				exx
				outi
                exx
1	    	    inc d
			edup
			dup 1
				add  a
				jr nc, 1F
				out (c), d
				exx
				outi
                exx
1	    	    
			edup
            jp   pl_finish

pl_full_psg2_2:
			dup 6
				out (c), d
				exx
				outi
                exx
	    	    inc d
			edup
			dup 1
				out (c), d
				exx
				outi
                exx
1	    	    
			edup
            jp  pl_finish

pl_finish   ld   a, (pl_counter)
            dec  a
            jr   nz, pl_end
            ld hl, (prevPos)
pl_end:     ld   (curPos), hl
            ret

curPos      DW 0
prevPos     DW 0
pl_counter  DW 0
