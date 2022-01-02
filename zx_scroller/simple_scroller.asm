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
                ld ixl,a
                ei

                ld bc,768
                ld hl,#c000+6144
                ld de,#4000+6144
                ldir

                ld b,192
1               halt
                djnz 1b

                ld b, 192
screen_loop 
                push bc
                ld de,#4000
                ld hl,#4100
                bit 0,b
                jr nz,draw_to_page7
draw_to_page5
                ld a, #ba       ; res 7,d
                ld (upd_de+1),a
                set 7,h

                call move_screen

                // copy_last_line to page 5
                ld bc, #7ffd
                ld a,1+8
                out (c),a
i1              ld hl, init_screen_i0
                ld bc,32
                ldir
                ld (i1+1),hl

                ld a, 7
                jr draw_common
draw_to_page7
                ld a, #fa       ; set 7,d
                ld (upd_de+1),a
                set 7,d
                call move_screen

                // copy_last_line to page 7
i0              ld hl, init_screen_i1
                ld bc,32
                ldir
                ld (i0+1),hl
                ld a, 7+8

draw_common
                halt
                ld bc, #7ffd
                ld ixl,a
                out (c),a
                pop bc
                djnz screen_loop

                ret

unpack_page
            halt
            ld bc, #7ffd
            out (c),a
            ld ixl,a

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

unpack_and_play_init_screen
            ld hl, after_play_intro
            ld(endtrack+1), hl

            // Play initial screen here
            call draw_init_screen

            // unpack initial screen to video memory (show shadow screen)
            halt
            LONG_SET_PAGE 0+8
            ld ixl,a
            LD HL, ram0_end
            LD DE, 16384
            CALL  dzx0_standard

            // unpack page0 (lose packed first screen)
            LD HL, ram0_end-1
            LD A,8
            CALL unpack_page

            // unpack page1
            LD HL, ram1_end-1
            LD A,1+8
            CALL unpack_page

            //create_write_off_rastr_helper
            //ld (draw_offrastr_offset), hl ; The value 0 is not used.
            ld hl, draw_off_rastr_1
            ld (draw_offrastr_offset + 2), hl
            ld hl, draw_off_rastr_2
            ld (draw_offrastr_offset + 4), hl
            ld hl, draw_off_rastr_3
            ld (draw_offrastr_offset + 6), hl
            ld hl, draw_off_rastr_4
            ld (draw_offrastr_offset + 8), hl
            ld hl, draw_off_rastr_5
            ld (draw_offrastr_offset + 10), hl
            ld hl, draw_off_rastr_6
            ld (draw_offrastr_offset + 12), hl
            ld hl, draw_off_rastr_7
            ld (draw_offrastr_offset + 14), hl
        
            jp unpack_main_page

simple_scroller_end

            ORG #BF01
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
                ld a, ixl
                out (c),a

                exx
                pop de
                pop af
                ei
                ret

unpack_main_page
                // move main data block
main_compressed_size       EQU main_data_end - main_code_end
                LD HL, main_data_end-1
                LD DE, update_jpix_helper-1
                LD BC, main_compressed_size
                LDDR
                // unpack main data block
                LD HL, update_jpix_helper - main_compressed_size
                LD DE, generated_code
                jp  dzx0_standard

            ASSERT $ <= #BFBF
