        INCLUDE "wait_ticks.asm"

        DEVICE zxspectrum48
        SLDOPT COMMENT WPMEM, LOGPOINT, ASSERTION //< More debug symbols.

screen_addr:    equ 16384
screen_size:    equ 1024 * 6 + 768
screen_end:     equ 5b00h
generated_code: equ screen_end + 1024

/*************** Image data. ******************/

    org generated_code

        INCBIN "resources/compressed_data.main"
color_code        
        INCBIN "resources/compressed_data.color"
        align	4
descriptors
        INCBIN "resources/compressed_data.main_descriptor"

color_descriptor
        INCBIN "resources/compressed_data.color_descriptor"

timings_data
        INCBIN "resources/compressed_data.timings"
timings_data_end

src_data
        INCBIN "resources/samanasuke.bin", 0, 6144
        
color_data:
        INCBIN "resources/samanasuke.bin", 6144, 768
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


copy_image:
        ld hl, src_data
        ld de, 16384
        ld bc, 6144
        ldir
        ret

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

        MACRO draw_64_lines
                // hl - descriptor
                // sp - destinatin screen address to draw

                ld e, (hl)                                      ; 7
                inc l                                           ; 4
                ld d, (hl)                                      ; 7
                ex de, hl                                       ; 4
                ld ixl, 8                                       ; 11
                ; free registers to use: bc, de, hl
                ld iy, $ + 5                                    ; 14
                jp hl                                           ; 4
                // total: 51
        ENDM
        
        MACRO draw_8_color_lines
                ld e, (hl)                                      ; 7
                inc l                                           ; 4
                ld d, (hl)                                      ; 7
                ex de, hl                                       ; 4
                ld iy, $ + 5                                    ; 14
                jp hl                                           ; 4
                // total: 40
        ENDM

draw_image_and_color
        ; bc - line number
        ld (stack_bottom), sp                           ; 20

        ld hl, bc                                       ; 8
        ; save line number * 2
        add hl, hl                                      ; 11
        ld (stack_bottom + 2), hl                       ; 16

        ; save line number / 4
        ld a, c
        srl b
        rra
        srl b
        rra
        and ~1
        ld c, a
        ld (stack_bottom + 4), bc                       ; 20

        ld a, 1                                         ; 7

        // ----------- draw colors (top)
        ld hl, color_descriptor + 16 * 2                ; 10
        add hl, bc                                      ; 11
        ld sp, 16384 + 1024*6 + 256
         draw_8_color_lines

        // ----------- draw image (top)
        ld hl, (stack_bottom + 2)                      ; 16
        ld bc, descriptors  + 128 * 2                  ; 10
        add hl, bc                                     ; 11
        ld sp, 16384 + 1024 * 2
        draw_64_lines

        // ----------- draw colors (middle)
        ld hl, (stack_bottom + 4)
        ld bc, color_descriptor + 8 * 2                 ; 10
        add hl, bc                                      ; 11
        ld sp, 16384 + 1024*6 + 512
        draw_8_color_lines

        // --------- draw image (middle)
        ld hl, (stack_bottom + 2)                      ; 16
        ld bc, descriptors + 64 * 2                    ; 10
        add hl, bc                                      ; 11
        ld sp, 16384 + 1024 * 4
        draw_64_lines

        // ----------- draw colors (bottom)
        ld hl, (stack_bottom + 4)
        ld bc, color_descriptor                         ; 10
        add hl, bc                                      ; 11
        ld sp, 16384 + 1024*6 + 768
        draw_8_color_lines

        // -------- draw image (bottom)
        ld hl, (stack_bottom + 2)                       ; 16
        ld bc, descriptors                              ; 10
        add hl, bc                                      ; 11
        ld sp, 16384 + 1024 * 6
        draw_64_lines

        ld sp, (stack_bottom)
        ret                

draw_64_color_lines
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
        ; free registers to use: bc, de, bc', de'
        jp ix                                           ; 8
        exx                                             ; 4

        ; restore data
        ld (hl), LD_BC_XXXX_CODE                        ; 10
        ex de, hl                                       ; 4

        jp iy

draw_colors
        ld (stack_bottom), sp

        ; bc - line number

        ld a, c
        srl b
        rra
        and ~3

        ; hl = bc/8 * sizeof(descriptor)
        ld h, b
        ld l, a
        ld bc, color_descriptor + 4 * 16                ; 10
        add hl, bc                                      ; 11

        ld sp, 16384 + 1024 * 6 + 256
        ld iy, $ + 7
        jp draw_64_color_lines

        ld de, -9 * 4                                   ; 10
        add hl, de                                      ; 11
        ld sp, 16384 + 1024 * 6 + 256 * 2
        ld iy, $ + 7
        jp draw_64_color_lines

        ld de, -9 * 4                                   ; 10
        add hl, de                                      ; 11
        ld sp, 16384 + 1024 * 6 + 256 * 3
        ld iy, $ + 7
        jp draw_64_color_lines

        ld sp, (stack_bottom)

        ret                

/************** delay routine *************/

d1      equ     17+15+21+11+15+14
d2      equ     16
d3      equ     20+10+11+12

        ALIGN 256
base    
        org     base+256-58

delay   xor     a               ; 4
        or      h               ; 4
        jr      nz,wait1        ; 12/7  20/15
        ld      a,l             ; 4
        sub     d1              ; 7
        ld      hl,base+d2      ; 10    21
wait2   sub     l               ; 4
        jr      nc,wait2        ; 12/7  16/11
        ld      l,a             ; 4
        ld      l,(hl)          ; 7
        jp      (hl)            ; 4     15
wait1   ld      de,-d3          ; 10
        add     hl,de           ; 11
        jr      delay           ; 12    33

t26     nop
t22     nop
t18     nop
t14     nop
        ret

t27     nop
t23     nop
t19     nop
t15     ret     nc
        ret

t28     nop
t24     nop
t20     nop
t16     inc     hl
        ret

t29     nop
t25     nop
t21     nop
t17     ld l, (hl)
        ret

table   db      t14&255,t15&255,t16&255,t17&255
        db      t18&255,t19&255,t20&255,t21&255
        db      t22&255,t23&255,t24&255,t25&255
        db      t26&255,t27&255,t28&255,t29&255


/*************** Main. ******************/
main:
        di
        ld sp, stack_top

        ; Change border color
        ld a, 1
        out 0xfe,a

        call copy_image
        call copy_colors
	
       
        call prepare_interruption_table
        ei
        halt
after_interrupt:        
        ; Pentagon timings
first_timing_in_interrupt equ 21

        di                              ; 4 ticks
        ; remove interrupt data from stack
        pop af                          ; 10 ticks

ticks_after_interrupt equ first_timing_in_interrupt + 74

screen_ticks  equ 71680
first_rastr_line_tick equ  17920
ticks_per_line equ  224

sync_tick equ first_rastr_line_tick + ticks_per_line * 64
ticks_to_wait equ sync_tick - ticks_after_interrupt

        wait_ticks ticks_to_wait

max_scroll_offset equ 191 //(timings_data_end - timings_data) / 2 - 1
scroll_step     equ 1

        ld bc, 0h                       ; 10  ticks
.loop:  
        ld a, 1                         ; 7 ticks
        out 0xfe,a                      ; 11 ticks

        push bc                         ; 11 ticks
        call draw_image_and_color       ; ~55000 ticks
        pop bc                          ; 10 ticks

        ld a, 2                         ; 7 ticks
        out 0xfe,a                      ; 11 ticks

        ld hl, 65000
        call delay
        ld hl, 65000
        call delay
        ld hl, 65000
        call delay
        ld hl, 65000
        call delay
        ld hl, 65000
        call delay
        ld hl, 65000
        call delay
        ld hl, 65000
        call delay
        ld hl, 65000
        call delay

        ; delay
        ; hl = delay
/*        
        ld hl, bc
        add hl, bc
        ld de, timings_data
        add hl, de
        ld a, (hl)
        inc hl
        ld h, (hl)
        ld l, a
        ld de, -4139 ;  // extra delay
        add hl, de

        call delay

        pop de                          ; 10 ticks
*/        

        ; do increment
        dec bc
        ld a, b
        cp 0xff

        ; compare to N
        jp z, .lower_limit_reached      ; 10 ticks
        jp .common_branch               ; 10 ticks
.lower_limit_reached:
        ld bc, max_scroll_offset        ; 10 ticks
.common_branch:

/*
.total_ticks_per_loop: equ 65725 - 3
        wait_ticks (screen_ticks - .total_ticks_per_loop)
        //.50 wait_ticks screen_ticks
*/

        jp .loop                        ; 12 ticks
 
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
    savetap "build/draw_image.tap", main
