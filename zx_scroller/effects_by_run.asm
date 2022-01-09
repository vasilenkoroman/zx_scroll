            // Effects by run (perform on step 0)

effect0
            ld a, #3e                   ; make JR xx to LD A, XX
            ld (check_for_page7_effect),a
            jp loop

effect1
            ld a, #38                   ; JR C
            ld (check_for_page7_effect),a
            jp loop

effect2
            ld a, #38                   ; JR C
            ld (check_for_page7_effect),a
            jp loop

effect3
            ld a, #3e                   ; make JR xx to LD A, XX
            ld (check_for_page7_effect),a
            jp loop


ef2_counter DB 8
            // Effects inside run
effect_step
            // temporary line:
            jp after_draw_colors


            // 1. Just display "Scroller"
ef1         ld hl, 192*4 - 40
            dec hl
            ld (ef1+1),hl
            ld a,h
            or l
            jp nz, after_draw_colors

            // 2. Effect2: remove "Scroller" with animation
            ld hl,ef2
            ld (ef_x+1),hl
ef2         ld hl,#c800    
            ld de,hl
            inc de
            ld (hl),0
            ld bc,32
            ldir                ; clear line 0

            ld l,0
            ld e,l
            inc h
            inc d
            ldir                ; clear line 1

            inc h
            ld a,h
            ld (ef2+2),a
            and 7
            jp nz, after_draw_colors

            ld l,#c8
            ld (ef2+1),hl
            
            ld hl,ef2_counter
            dec (hl)
            jp nz, after_draw_colors

            ld hl,ef3
            ld (ef_x+1),hl
            jp after_draw_colors

            // Minor delay after removing "Scroller" text, prepare page7 for the scrolling text
ef3         
            // clear attributies
            ld sp,#da00
            ld hl,#40
            ld b,128
1           push hl
            djnz 1b
            ld hl,ef3_2
            ld (ef_x+1),hl
            jp after_draw_colors
ef3_2
            // clear rastr
            ld sp,#d000
            ld hl, #0f
            ld b,0
1           .2 push hl
            djnz 1b
            ld (ef3_2+1),sp
            ld hl,-#c800
            add hl,sp
            jp nz,after_draw_colors

            ld hl,ef4
            ld (ef_x+1),hl
            jp after_draw_colors

            // Start scrolling text
ef4         ld hl,encoded_text

            ld sp,stack_top+2
            call print_ch
            call copy_buffer

ch_loop     ld a,8
            dec a
            jp nz, after_draw_colors
            ld a,8
            ld (ch_loop+1),a
            ld hl,(ef4+1)
            inc hl
            ld (ef4+1),hl

            jp after_draw_colors
