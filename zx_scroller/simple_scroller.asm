move_screen
next_line
                ld bc, 8*256 + 255
                push hl
1               ldi
                ldi
                ldi
                ldi
                djnz 1b
                pop hl
                ld de,hl
upd_de          set 7,d

                // down_hl
                inc h
                ld a,h
                and 7
                jr nz, next_line ; It is slow, but we don't hurry here. Used for 25fps scrool only.
                ld a,l
                sub -32
                ld l,a
                sbc a
                and #f8
                add h
                ld h,a
                and #7f
                cp #58
                ret z
next            jr next_line

copy_last_line
                ld hl, init_screen
                ld bc,32
                ldir
                ld (copy_last_line+1),hl
                ret            

draw_init_screen
            ld hl, play_init_screen
            ld   (#BFBF+1), hl
   

            LONG_SET_PAGE 7
            ld (restore_page+1),a
            ei

                ld bc,768
                ld hl,#4000+6144
                ld de,#4001+6144
                ld (hl),#7
                ldir

                ld bc, 192*256
screen_loop 
                bit 0,b
                jr nz,draw_to_page7
draw_to_page5
                ld a, #ba       ; res 7,d
                ld (upd_de+1),a
                ld de,#4000
                ld hl,#c100
                ld a, 7+8
                jr draw_common
draw_to_page7
                ld a, #fa       ; set 7,d
                ld (upd_de+1),a
                ld de,#c000
                ld hl,#4100
                ld a, 7
draw_common

            push bc
            ld bc, #7ffd
            out (c),a
            ld (restore_page+1),a

            call move_screen
            call copy_last_line
            pop bc
            halt
            djnz screen_loop

            ld bc, #7ffd
            ld a,8
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

        ld bc, #7ffd
restore_page
        ld a, 7
        out (c),a

        exx
        pop de
        pop af
        ei
        ret
