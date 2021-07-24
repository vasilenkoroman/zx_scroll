        INCLUDE "wait_ticks.asm"

        DEVICE zxspectrum48
        SLDOPT COMMENT WPMEM, LOGPOINT, ASSERTION //< More debug symbols.

screen_addr:    equ 16384
screen_size:    equ 1024 * 6 + 768
screen_end:     equ screen_addr + screen_size
//generated_code: equ 0x5b00 ; screen_end + 768
generated_code: equ screen_end + 1024

/*************** Image data. ******************/

    org generated_code

        INCBIN "resources/thanos.bin.main"
        align	4
descriptors_data
        INCBIN "resources/thanos.bin.descriptor"
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
                // sp - descriptor
                // (stack_bottom + 2) - destinatin screen address to draw
                // iy - destinatin color address to draw

                ; ix - address to execute
                pop ix                                          ; 14

                ; hl - end execute address
                pop hl                                          ; 10

                ; bc - colors to execute
                pop bc                                          ; 10

                ; save descriptor
                ld (stack_bottom), sp                           ; 20

                ; drawing address
                ld sp, (stack_bottom + 2)                       ; 20

                ld (hl), JP_HL_CODE                             ; 10

                exx                                             ; 4
                ld hl, $ + 6    ; return address                ; 10
                ; free registers to use: bc, de, de'
                jp ix                                           ; 8
                exx                                             ; 4

                ; restore data
                ld (hl), LD_BC_XXXX_CODE                        ; 10

                // draw RT colors (1 line)

                ; save main draw address
                ld (stack_bottom + 2), sp                       ; 20
                ; set stack to color draw address
                ld sp, iy                                       ; 10

                ld hl, bc                                       ; 8
                jp hl                                           ; 4

                ; Move destination color address to the next line
                ld iy, -32                                      ; 14 
                add iy, sp                                      ; 15
                ; restore descriptor
                ld sp, (stack_bottom)                           ; 20

                // total ticks: 211
end_draw:
                jp 00      ; ret                                ; 10 ticks

draw_64_lines
        ld b, 8

draw_8_lines
                // hl - descriptor
                // sp - destinatin screen address to draw

                ; ix - address to execute
                ld e, (hl)                                      ; 7
                inc l                                           ; 4
                ld d, (hl)                                      ; 7
                inc l                                           ; 4
                ld ix, de                                       ; 16

                ; de - end exec address
                ld e, (hl)                                      ; 7
                inc l                                           ; 4
                ld d, (hl)                                      ; 7
                inc hl                                          ; 6

                ex de, hl                                       ; 4
                ld (hl), JP_HL_CODE                             ; 10

                exx                                             ; 4
                ld hl, $ + 5    ; return address                ; 10
                ; free registers to use: bc, de, bc'
                jp ix                                           ; 8
                exx                                             ; 4

                ; restore data
                ld (hl), LD_BC_XXXX_CODE                        ; 10
                ex de, hl                                       ; 4

                // total ticks: 116

        djnz draw_8_lines
        jp iy      ; ret                                        ; 8 ticks

draw_image
        ; bc - line number
        ; a - best byte constant

        ; hl = bc * sizeof(descriptor)
        ld hl, bc
        add hl, bc
        add hl, bc

        ld bc, descriptors_data
        add hl, bc

        ld sp, 16384 + 1024 * 6
        ld iy, $ + 7
        jp draw_64_lines

        ld sp, 16384 + 1024 * 4
        ld iy, $ + 7
        jp draw_64_lines

        ld sp, 16384 + 1024 * 2
        ld iy, $ + 7
        jp draw_64_lines


        ret                

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

        //ld bc, max_scroll_offset        ; 10  ticks
        ld bc, 0
        ld de, -1                       ; 10 ticks
.loop:  
        ld a, 1                         ; 7 ticks
        out 0xfe,a                      ; 11 ticks

        push bc                         ; 11 ticks
        push de                         ; 11 ticks
        call draw_image                 ; 67103 ticks
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

        ; it need to update code. Currently it don't increment low part of the register.
data_segment_end:

/*************** Commands to SJ asm ******************/

    SAVESNA "build/draw_image.sna", main
