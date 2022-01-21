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
                // Init music callback
                ld hl, player_callback
                ld   (#BFBF+1), hl

                // Show page 7
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
                SET_PAGE 1
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

copy_page7_screen
                IF (HAS_PLAYER == 1)
                ld a, 0                         ; 7 ticks
                out 0xfe,a                      ; 11 ticks

                ld hl, mus_intro
                call  init // player init

                SET_PAGE 7+8
                ld bc,6912
                ld hl,#c000
                ld de,#4000
                ldir
                LONG_SET_PAGE 7
                ld bc,6912
                ld de,#c000
                ld hl,#4000
                ldir
                ENDIF                
                ret

prepare_interruption_table:
        // make interrupt table

        ld   a, 0c3h    ; JP instruction code
        ld   (#BFBF), a
        ld hl, AlignInt.IntEntry
        ld   (#BFBF+1), hl

        LD   hl, #BE00
        LD   de, #BE01
        LD   bc, #0100
        LD   (hl), #BF
        LD   a, h
        LDIR
        ld i, a
        im 2
        
        ld hl, AlignInt.MeasuringLoop
        ei
        halt


unpack_and_play_init_screen
        IF (HAS_PLAYER == 1)

            ld hl, after_play_intro
            ld(endtrack+1), hl

            // Play initial screen here
            call draw_init_screen
        ELSE
            di            
        ENDIF            

            // Create effects by run helper
        IF (HAS_PLAYER == 1)            
            ld sp, effects_by_run + 8
            ld hl, effect3
            push hl
            ld hl, effect2
            push hl
            ld hl, effect1
            push hl
            ld hl, effect0
            push hl
            ld sp, stack_top-2
            halt
        ELSE
            ld sp, stack_top-2
        ENDIF            
            jp continue_unpacking

simple_scroller_end

            ORG #BF01

continue_unpacking
                // This code is safe under unpack page2. Code above is lost
                // unpack screen3 screen to page 5 (show shadow screen)
                IF (HAS_PLAYER == 1)                
                        SET_PAGE 3+8
                        ld ixl,a
                        LD HL, screen3
                        LD DE, 16384
                        CALL  dzx0_standard

                        // copy page5->page7
                        halt
                        LONG_SET_PAGE 7+8
                        ld ixl,a
                        call copy_5_to_7_animated
                        xor a
                        out (#fe),a
                ENDIF     

                // unpack initial screen to video memory (show shadow screen)
                SET_PAGE 0+8
                ld ixl,a
                LD HL, first_multicolor_delta
                LD DE, 16384
                CALL  dzx0_standard

                // unpack page0
                //SET_PAGE 0+8
                LD HL, ram0_end-1
                LD A, #50 + 8
                CALL unpack_page

                // unpack page1
                LD HL, ram1_end-1
                LD A,#51 + 8
                CALL unpack_page

                // Unpack page 2
main_compressed_size       EQU main_data_end - main_code_end
                LD HL, main_data_end-1
                LD DE, static_data_page2-1
                LD BC, main_compressed_size
                LDDR
                // unpack main data block
                LD HL, static_data_page2 - main_compressed_size
                LD DE, generated_code
                call  dzx0_standard

                // unpack ram 3
                LD HL, ram3_end-1
                LD A,#53 + 8
                CALL unpack_page
                ret

unpack_page
                IF (HAS_PLAYER == 1)
                        halt
                ENDIF
                out (#fd),a
                ld ixl,a

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
                jp  dzx0_standard

                DISPLAY	"Above interruption vector free ", /D, #BF80 - $

                ASSERT $ <= #BF80
                IF ($ < #BF80)
                        ORG #BF80
                ENDIF

player_callback
                push af
                push de
                exx

set_player_page ld a, #56
                out (#fd),a

                IF (HAS_PLAYER == 1)
                        call play
                ENDIF
                ld a, ixl
                and 7
                cp 7
                ld a, ixl
                jr z, restore_long_page
                out (#fd),a
                jr 1f
restore_long_page                
                // Use full address here to correct address shadow screen
                ld bc, #7ffd
                out (c),a
1
                exx
                pop de
                pop af
                ei
                ret

            ASSERT $ <= #BFBF
