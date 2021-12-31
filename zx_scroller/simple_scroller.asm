
copy_image
            ; hl - source bitmap (full screen width)
            ; b - screen line number to draw

            // calculate screen address
            ld a,b
            and 7
            or #c0
            ld d,a
            ld a,b
            rra: rra: rra
            and #18
            or d
            ld d,a

            ld a,b
            rla: rla
            and #e0
            ld e, a

next_line
            ld bc, 32 + 8
            push de
line_loop
            ldi
            ldi
            ldi
            ldi
            dec c
            jr nz, line_loop
            pop de
    
    //MACRO down_hl
            inc d
            ld a,d
            and 7
            jr nz, next ; It is slow, but we don't hurry here. Used for 25fps scrool only.
            ld a,e
            sub -32
            ld e,a
            sbc a
            and #f8
            add d
            cp #58 + #80
            ret z
            ld d,a
next        ; 27/56            
    //ENDM
            jr next_line

int_counter DB 0
draw_init_screen
            ld hl, play_init_screen
            ld   (#BFBF+1), hl
   

            LONG_SET_PAGE 7
            ld (restore_page+1),a
            ei

                ld bc,6144
                ld hl,#4000
                ld de,#c001
                ld (hl),0
                ldir

                ld bc,768
                ld hl,#4000+6144
                ld de,#4001+6144
                ld (hl),#7
                ldir

                ld bc,6912
                ld hl,#4000
                ld de,#c000
                ldir

            ld bc, 192*256
screen_loop 
            ld a, 5+8
            bit 0,b
            jr z,common
shadow_screen
            ld a, 7
common

            exx
            ld bc, #7ffd
            out (c),a
            ld (restore_page+1),a
            exx

            ld hl, init_screen
            push bc
            dec b
            call copy_image
            pop bc
            halt
            ld a, (int_counter)
            rra
            jr nc, 1f
            halt    //< Fast draw. Wait 1 frame more
1
            dec b
            jr nz, screen_loop

            ld bc, #7ffd
            ld a, 5+8
            out (c),a
            ld (restore_page+1),a

            ret
                         
play_init_screen
        push af
        push de
        exx

        ld bc, #7ffd
        ld a,6
        out (c),a
        call play
        ld hl, int_counter
        inc (hl)

        ld bc, #7ffd
restore_page
        ld a, 7
        out (c),a

        exx
        pop de
        pop af
        ei
        ret
