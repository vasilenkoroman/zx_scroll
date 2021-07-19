        INCLUDE "wait_ticks.asm"

        DEVICE zxspectrum48
        SLDOPT COMMENT WPMEM, LOGPOINT, ASSERTION //< More debug symbols.

screen_addr:    equ 16384
screen_size:    equ 6144
screen_end:     equ screen_addr + screen_size
//generated_code: equ 0x5b00 ; screen_end + 768
generated_code: equ 32768
                ASSERT generated_code % 256 == 0

/*************** Image data. ******************/

    org generated_code

        INCBIN "resources/thanos.bin.main"
color_data:
        //INCBIN "resources/thanos.bin", 6144, 768
data_end:

/*************** Ennd image data. ******************/

JP_HL_CODE      equ 0e9h
LD_BC_XXXX_CODE equ 01h
LD_DE_XXXX_CODE equ 11h

        MACRO write_byte data
                ld (hl), data
                inc hl
        ENDM

/*************** Draw 8 lines of image.  ******************/
JP_IY_COMMAND equ 0000

        MACRO draw_8_lines_with_iy
                ; hl - descriptor
                ; sp - destinatin screen address to draw

                ; bc = descriptor[line].offset (exec address)
                ld c, (hl)      ; 7
                inc  l          ; 4
                ld b, (hl)      ; 7     
                inc l           ; 4

                ld d, 1                                         ; 7
                ld e, (hl)                                      ; 7
                inc l                                           ; 4
                inc hl                                          ; 6
                ex de, hl                                       ; 4
                add hl, bc                                      ; 11
                ; hl now point to execution address end, de - saved descriptor


                ld (hl), high(JP_IY_COMMAND)                                                    ; 10
                inc hl                                                                          ; 6
                ld c, (hl)      ; save only 2-nd byte. Assume 1-st byte is always LD bc, xx     ; 7
                ld (hl), low(JP_IY_COMMAND)                                                     ; 10

                ld ix, bc                                                                       ; 16
                exx                                                                             ; 4
                ; TODO: put here 'the most used byte' from generated code
                ld a, 0xff                                                                      ; 7
                ld iy, $ + 5 ; return addr                                                      ; 14
                jp ix ; draw                                                                    ; 8
                exx                                                                             ; 4

                // restore code back
                ld (hl), c                                                                      ; 7
                dec hl                                                                          ; 6
                ld (hl), LD_BC_XXXX_CODE                                                        ; 10
                ex de, hl                                                                       ; 4

                // itself total 174 ticks per 8 lines, avg 21.75 ticks overhead/line
        ENDM


copy_colors:
        ld hl, color_data
        ld de, 16384 + 6144
        ld bc, 768
        ldir
        ret

prepare_interruption_table:
        ld A, 18h       ; JR instruction code
        ld  (65535), a

        ld   a, 0c3h    ; JP instruction code
        ld   (65524), a
        ld hl, after_interrupt
        ld   (65525), hl

        LD   hl, #FE00
        LD   de, #FE01
        LD   bc, #0100
        LD   (hl), #FF
        LD   a, h
        LDIR
        ld i, a
        im 2
        ret


draw_4_lines_and_rt_colors:
                // hl - descriptor
                // sp - destinatin screen address to draw
                // iy - color address to draw

                ; bc - address to execute
                ld c, (hl)                                      ; 7
                inc l                                           ; 4
                ld b, (hl)                                      ; 7
                inc l                                           ; 4

                ; exec address end
                ld e, (hl)                                      ; 7
                inc l                                           ; 4
                ld d, (hl)                                      ; 7
                inc hl                                          ; 10
                ex de, hl       ; save descriptor to de         ; 4

                ; update generated code line
                ld (hl), JP_HL_CODE                             ; 11
                ld ix, bc                                       ; 16
                exx                                             ; 4
                ld hl, $ + 5    ; return address                ; 10
                ; free registers to use: bc, de, bc'.   hl -return addr, hl' - exec end addr to restore, de' - descriptor
                jp ix                                           ; 8
                exx                                             ; 10

                ; restore data
                ld (hl), LD_BC_XXXX_CODE                        ; 11
                ex de, hl                                       ; 4

                // total ticks: 126

                // draw RT colors (1 line)

                ld d, (hl)                                      ; 7
                inc l                                           ; 4
                ld e, (hl)                                      ; 7
                inc hl                                          ; 10

                ; save descriptor
                ld (stack_bottom), hl                           ; 16
                ; save stack
                ld (stack_bottom + 2), sp                       ; 20
                ; set stack to color attributes
                ld sp, iy                                       ; 10

                ex de, hl                                       ; 4
                ld ix, $ + 4    ; return address                ; 14
                jp hl                                           ; 4
                ; restore descriptor
                ld hl, (stack_bottom)                           ; 16
                ; restore stack
                ld sp, (stack_bottom + 2)                       ; 20

                // total ticks:118 for colors, total: 118 + 126 = 244


                jp iy      ; ret                                               ; 10 ticks


draw_64_lines_2:
                // sp - descriptor
                // de - destinatin screen address to draw

                ; bc - address to execute
                pop ix                                          ; 10

                ; hl - end execute address
                pop hl                                          ; 10

                ; save descriptor
                ld (stack_bottom + 2), sp                       ; 20

                ld (hl), JP_HL_CODE                             ; 11

                ex de, hl ; save end address in de              ; 4
                ld sp, hl                                       ; 6

                ld hl, $ + 6    ; return address                ; 10
                ; free registers to use: bc, de, bc'
                jp ix                                           ; 8

                ; restore data
                ex de, hl                                       ; 4
                ld (hl), LD_BC_XXXX_CODE                        ; 11

                ld hl, 0                                        ; 10
                add hl, sp                                      ; 11
                ex de, hl                                       ; 4
                ld sp, (stack_bottom + 2)                       ; 20
                // total ticks: 139

                jp iy      ; ret                                               ; 10 ticks

/*************** Main. ******************/
main:
        di
        ld sp, stack_top

        ; Change border color
        ld a, 1
        out 0xfe,a

        //call copy_colors
	
        call prepare_interruption_table
        ei
        halt
after_interrupt:        
        ; Pentagon timings
first_timing_in_interrupt equ 21

        di                              ; 4 ticks
        ; remove interrupt data from stack
        pop af                          ; 10 ticks

ticks_after_interrupt equ first_timing_in_interrupt + 12 + 10 + 4 + 10 ; jr + jp + di + pop

screen_ticks  equ 71680
first_rastr_line_tick equ  17920
ticks_per_line equ  224

sync_tick equ first_rastr_line_tick + ticks_per_line * 64
ticks_to_wait equ sync_tick - ticks_after_interrupt

        wait_ticks ticks_to_wait - 10 - 10 - 7

max_scroll_offset equ 191

        ld bc, max_scroll_offset        ; 10  ticks
        ld de, -1                       ; 10 ticks
.loop:  
        ld a, 1                         ; 7 ticks
        out 0xfe,a                      ; 11 ticks

        push bc                         ; 11 ticks
        push de                         ; 11 ticks
        //call draw_image                 ; 67103 ticks
        halt
        pop de                          ; 10 ticks
        pop bc                          ; 10 ticks

        ; do increment
        ld hl, bc
        add hl, de
        ld bc, hl

        ; compare to 0
        ld a, h
        or l
        jp z, .zero_reached

        ; compare to N
        push bc                         ; 11 ticks
        ld bc, -max_scroll_offset       ; 10 ticks
        add hl, bc                      ; 11 ticks
        pop bc                          ; 10 ticks
        
        jp c, .upper_limit_reached      ; 10 ticks
        jp .common_branch               ; 10 ticks
.zero_reached:
        ld de, 1                        ; 10 ticks
        wait_ticks 42
        jp .common_branch               ; 10 ticks
.upper_limit_reached:
        ld de, -1                        ; 10 ticks
.common_branch:

        ld a, 2                         ; 7 ticks
        out 0xfe,a                      ; 11 ticks

.total_ticks_per_loop: equ 67300
        //wait_ticks (screen_ticks - .total_ticks_per_loop)

        jr .loop                        ; 12 ticks
 
        ret

/*************** Data segment ******************/
STACK_SIZE: equ 16  ; in words
stack_bottom:
        defs STACK_SIZE * 2, 0
stack_top:

line_descriptor:
        INCBIN "resources/thanos.bin.descriptor"
line_descriptor_end:
        ; it need to update code. Currently it don't increment low part of the register.
data_segment_end:

/*************** Commands to SJ asm ******************/

    SAVESNA "build/draw_image.sna", main
