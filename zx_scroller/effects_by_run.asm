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
            //ret

            // 1. Just display "Scroller"
ef1         //ld hl, 192*2 - 35
            ld hl, 16
            dec hl
            ld (ef1+1),hl
            ld a,h
            or l
            ret nz

            // 2. Effect2: remove "Scroller" with animation
            ld hl,ef2
            ld (ef_x+1),hl
ef2         ld hl,#c800    
            ld de,hl
            inc de
            ld (hl),0
            ld bc,31
            ldir                ; clear line 0

            ld hl,(ef2+1)
            inc h
            ld de,hl
            inc de
            ld (hl),0
            ld bc,31
            ldir                ; clear line 1


            ld a,(ef2+2)
            add 2
            ld (ef2+2),a
            and 7
            ret nz

            ld h,#c8
            ld a,(ef2+1)
            add 32
            ld l,a
            ld (ef2+1),hl

            ld hl,ef2_counter
            dec (hl)
            ret nz

            ld hl,ef3
            ld (ef_x+1),hl
            ret

            // Minor delay after removing "Scroller" text, prepare page7 for the scrolling text
ef3         
            // clear attributies
            ld sp,#da00
            ld de,#40
            ld b,128
1           push de
            djnz 1b
            ld hl,ef3_2
            ld (ef_x+1),hl
            ld sp,stack_top-6
            ret
ef3_2
            // clear rastr
            ld sp,#d000
            ld de, #0f0f
            ld b,0
1           .2 push de
            djnz 1b
            ld (ef3_2+1),sp
            ld a,(ef3_2+2)
            cp #c8
            ld sp,stack_top-6
            ret nz

            ld hl,ef4
            ld (ef_x+1),hl
            ret

            // Start scrolling text
ef4         ld a,(encoded_text)
            call print_ch
            call copy_buffer

ch_loop     ld a,8
            dec a
            ld (ch_loop+1),a
            ret nz
            ld a,8
            ld (ch_loop+1),a
            ld hl,(ef4+1)
            inc hl
            ld (ef4+1),hl

            ret
