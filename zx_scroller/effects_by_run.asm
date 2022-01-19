            // Effects by run (perform on step 0)

effect0
            ld a, #3e                   ; make JR xx to LD A, XX
            ld (check_for_page7_effect),a
            jp loop

effect1
            ld a, #20                   ; JR nz
            ld (check_for_page7_effect),a
            jp loop

effect2
            ld a, #20                   ; JR nz
            ld (check_for_page7_effect),a
            jp loop

effect3
            ld a, #3e                   ; make JR xx to LD A, XX
            ld (check_for_page7_effect),a
            jp loop


ef2_counter DB 8
            // Effects inside run
effect_step

            ld de,0
            ld sp,#d900
            ld b,32
1           .4 push de
            djnz 1b
            ld sp,#db00
            ld b,32
1           .4 push de
            djnz 1b
            ld hl,ef1
            ld (ef_x+1),hl
            ld sp,stack_top-8
            ret

            // 1. Just display "Scroller"
ef1         ld hl, imageHeight/4 - 35
            dec hl
            ld (ef1+1),hl
            ld a,h
            or l
            ret nz
            // 10+6+16+4+4+11=51

            // 2. Effect2: remove "Scroller" with animation
            ld hl,ef2
            ld (ef_x+1),hl
            //51-6+10+16=71
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
            // total: 1430+11=1441
            ld h,#c8
            ld a,(ef2+1)
            add 32
            ld l,a
            ld (ef2+1),hl

            ld hl,ef2_counter
            dec (hl)
            ret nz
            // total: 7+13+7+4+16+10+11+11=79

            ld hl,ef3
            ld (ef_x+1),hl
            ret
            // total: 10+16+10=36

            // Minor delay after removing "Scroller" text, prepare page7 for the scrolling text
ef3         
            // clear attributies
            ld sp,#da00
            ld de,#4040
            ld b,128
1           push de
            djnz 1b
            ld hl,ef3_2
            ld (ef_x+1),hl
            ld sp,stack_top-8
            ret
            // total: 10+10+7+(11+13)*128-5+10+16+10+10=3140
ef3_2
            // clear rastr
            ld sp,#d000
            ld de, #0f0f
            ld b,64
1           .8 push de
            djnz 1b
            ld (ef3_2+1),sp

            ld a,(ef3_2+2)
            cp #c8
            ld sp,stack_top-8
            ret nz
            // total:9032+11
            ld hl,ef4
            ld (ef_x+1),hl

            ret
            // total:9063+10
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

///////////////////////////////////////////
fill_page7_step

pg7_fill_stack  ld sp, #d800
                ld bc,2
pg7_fill_value  ld de,0
pg7_fill        .16 push de
                ld hl,0
                add hl,sp
                dec h
                ld sp,hl
                djnz pg7_fill

                ld sp,stack_top-8
                ld (pg7_fill_stack),hl

                // up_hl
                ld a,h
                and 7
                cp 7
                ret nz

                cp #c0-1
                jr z, up_reached
                cp #d8-1
                jr z, bottom_reached

                ld de, 8*256 - 32
                add hl,de
                ld (pg7_fill_stack),hl
                ret

up_reached
                ld hl, #d800
                ld (pg7_fill_stack+1),hl

                ld a,(pg7_fill_value+1)
                cpl
                ld l,a
                ld h,a
                ld (pg7_fill_value+1),hl
                ret

bottom_reached
                ld hl, #c800
                ld (pg7_fill_stack+1),hl

                ld hl,pg7_fill_wait
                ld (ex_p+1),hl
                ret

pg7_fill_wait
                ld a,32
                dec a
                and 0x1f
                ld (pg7_fill_wait+1),a
                ret nz

                ld hl,fill_page7_step
                ld (ex_p+1),hl
                ret
