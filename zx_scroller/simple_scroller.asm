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
                jr nz, next_line
                ret

draw_init_screen
                ld hl, play_init_screen
                ld   (#BFBF+1), hl
   

                LONG_SET_PAGE 7+8
                ld (restore_page+1),a
                ei

                ld bc,768
                ld hl,#c000+6144
                ld de,#4000+6144
                ldir

                ld b,192
1               halt
                djnz 1b

                ld bc, 192*256
screen_loop 
                ld de,#4000
                ld hl,#4100
                bit 0,b
                jr nz,draw_to_page7
draw_to_page5
                ld a, #ba       ; res 7,d
                ld (upd_de+1),a
                set 7,h
                ld a, 7
                jr draw_common
draw_to_page7
                ld a, #fa       ; set 7,d
                ld (upd_de+1),a
                set 7,d
                ld a, 7+8
draw_common
                exa
                push bc
                call move_screen
copy_last_line
                ld hl, init_screen
                ld bc,32
                ldir
                ld (copy_last_line+1),hl

                halt
                exa
                ld bc, #7ffd
                out (c),a
                ld (restore_page+1),a
                pop bc
                djnz screen_loop

                ret

unpack_page
        halt
        ld bc, #7ffd
        out (c),a
        ld (restore_page+1),a

        //LD HL, page1_end-1
        LD DE, 65535
        LD C,L
        LD A,H
        and #3f
        LD B,A
        INC BC
        LDDR

        ex hl, de
        inc hl
        LD DE, #c000
        CALL  dzx0_standard
        ret

                         
play_init_screen
                push af
                push de
                exx

                ld bc, #7ffd
                ld a,6
                out (c),a
                push bc
                call play
                pop bc
restore_page
                ld a, 7+8
                out (c),a

                exx
                pop de
                pop af
                ei
                ret
                ASSERT $ <= #BFBF
            DISPLAY	"Draw init screen in bf01 mem size= ", /D, $ - move_screen
