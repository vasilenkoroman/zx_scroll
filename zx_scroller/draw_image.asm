        INCLUDE "wait_ticks.asm"

        DEVICE zxspectrum48
        SLDOPT COMMENT WPMEM, LOGPOINT, ASSERTION //< More debug symbols.

screen_addr:    equ 16384
screen_size:    equ 1024 * 6 + 768
screen_end:     equ 5b00h
generated_code: equ screen_end + 768

/*************** Image data. ******************/

    org generated_code

        INCBIN "resources/compressed_data.main"
color_code        
        INCBIN "resources/compressed_data.color"
        align	4
descriptors
        INCBIN "resources/compressed_data.main_descriptor"

        align	2
color_descriptor
        INCBIN "resources/compressed_data.color_descriptor"

        align	2
multicolor_descriptor
        INCBIN "resources/compressed_data.multicolor"

        align	2
jpix_table
        INCBIN "resources/compressed_data.jpix"
timings_data
        align	2
        INCBIN "resources/compressed_data.timings"
timings_data_end

src_data
        INCBIN "resources/samanasuke.bin", 0, 6144
color_data:
        INCBIN "resources/samanasuke.bin", 6144, 768
data_end:

/*************** Ennd image data. ******************/

JP_HL_CODE      equ 0e9h
JP_IX_CODE      equ #e9dd
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
                // (stack_bottom) - descriptor
                // (stack_bottom + 2) - destinatin rastr address to draw
                // (stack_bottom + 4) - destinatin multicolor address to draw

                ld sp, (stack_bottom)                           ; 20
                ; hl - colors to execute
                pop hl                                          ; 10
                exx                                             ; 4
                ; hl - rastr address to execute
                pop hl                                          ; 10
                pop de ; delay                                  ; 10
                ; save descriptor
                ld (stack_bottom), sp                           ; 20

                ; rastr drawing address
                ld sp, 16384+1024*4                             ; 10
                //call delay
                // draw rastr  (4 line)
                ld ix, $ + 5 ; ret addr                         ; 14
                jp hl                                           ; 4

                ; draw RT colors (1 line)
                exx                                             ; 4
                ld sp, 16384+1024*6+768                         ; 10
                ld ix, $ + 5                                    ; 14
                jp hl                                           ; 4
                // total ticks: 134


        MACRO draw_8_lines
                // hl - descriptor
                // sp - destinatin screen address to draw
                ld a, (hl)                                      ; 7
                ex af, af'                                      ; 4
                inc l                                           ; 4
                ld a, (hl)                                      ; 7
                inc hl                                          ; 6

                exx                                             ; 4
                ld h, a                                         ; 4
                ex af, af'                                      ; 4
                ld l, a                                         ; 4
                ld ix, $ + 5                                    ; 14
                jp hl                                           ; 4
                exx                                             ; 4
                // total: 66
        ENDM
                // total 66 ticks (15 bytes)

        MACRO draw_8_color_lines
                ld e, (hl)                                      ; 7
                inc l                                           ; 4
                ld d, (hl)                                      ; 7
                ex de, hl                                       ; 4
                ld ix, $ + 5                                    ; 14
                jp hl                                           ; 4
                // total: 40
        ENDM

        MACRO restore_jp_ix
                pop hl ; address
                pop de ; restore data
                ld (hl), e
                inc hl
                ld (hl), d
        ENDM
        MACRO write_jp_ix_data
                pop hl ; address
                ld (hl), e
                inc hl
                ld (hl), d
        ENDM
        MACRO update_jp_ix_table
                // hl - screen address to draw*2
                add hl, hl ; * 4
                ld de, hl
                add hl, hl ; * 8
                add hl, de ; * 12

                ld sp, jpix_table + 8 * 3*4
                add hl, sp
                ld sp, hl

                // 1. restore data
                exx
                .3 restore_jp_ix
                exx

                // 1. write new JP_IX
                ld sp, -(8 * 3*4)
                add hl, sp
                ld sp, hl

                ld de, JP_IX_CODE
                write_jp_ix_data
                pop hl ; skip restore address
                write_jp_ix_data
                pop hl ; skip restore address
                write_jp_ix_data
                // total 343
        ENDM

draw_image_and_color
        ; bc - line number
        ld (stack_bottom), sp                           ; 20

        ld hl, bc                                       ; 8
        ; save line number * 2
        add hl, hl                                      ; 11
        ld (stack_bottom + 2), hl                       ; 16

        update_jp_ix_table

        ; save line number / 4
/*
        ld a, c
        srl b
        rra
        srl b
        rra
        and ~1
        ld c, a
        ld (stack_bottom + 4), bc                       ; 20

        // ----------- draw colors (top)
        ld hl, color_descriptor + 16 * 2                ; 10
        add hl, bc                                      ; 11
        ld sp, 16384 + 1024*6 + 256
         //draw_8_color_lines
*/         

        // ----------- draw image (top)
        ld hl, (stack_bottom + 2)                      ; 16
        ld bc, descriptors  + 128 * 2                  ; 10
        add hl, bc                                     ; 11
        ld sp, 16384 + 1024 * 2
        .8 draw_8_lines

/*
        // ----------- draw colors (middle)
        ld hl, (stack_bottom + 4)
        ld bc, color_descriptor + 8 * 2                 ; 10
        add hl, bc                                      ; 11
        ld sp, 16384 + 1024*6 + 512
        draw_8_color_lines
*/        

        // --------- draw image (middle)
        ld hl, (stack_bottom + 2)                      ; 16
        ld bc, descriptors + 64 * 2                    ; 10
        add hl, bc                                      ; 11
        ld sp, 16384 + 1024 * 4
        .8 draw_8_lines

/*
        // ----------- draw colors (bottom)
        ld hl, (stack_bottom + 4)
        ld bc, color_descriptor                         ; 10
        add hl, bc                                      ; 11
        ld sp, 16384 + 1024*6 + 768
        //draw_8_color_lines
*/

        // -------- draw image (bottom)
        ld hl, (stack_bottom + 2)                       ; 16
        ld bc, descriptors                              ; 10
        add hl, bc                                      ; 11
        ld sp, 16384 + 1024 * 6
        .8 draw_8_lines

        ld sp, (stack_bottom)
        ret                

write_initial_jp_ix_table
        ld sp, jpix_table
        ld de, JP_IX_CODE
        ld b, 24
.rep:   pop hl ; address
        ld (hl), e
        inc hl
        ld (hl), d
        pop hl ; skip restore data
        djnz .rep

        ld sp, stack_top - 2
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


long_delay:
        push bc
        ld b, 2
rep:    ld hl, 65535
        call delay
        djnz rep
        pop bc
        ret

/*************** Main. ******************/
main:
        di
        ld sp, stack_top

        jp multicolor_descriptor

        ; Change border color
        ld a, 1
        out 0xfe,a

        call copy_image
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

        call write_initial_jp_ix_table

ticks_after_interrupt equ first_timing_in_interrupt + 34

screen_ticks  equ 71680
first_rastr_line_tick equ  17920
ticks_per_line equ  224

sync_tick equ first_rastr_line_tick + ticks_per_line * 64
ticks_to_wait equ sync_tick - ticks_after_interrupt

        wait_ticks ticks_to_wait

max_scroll_offset equ (timings_data_end - timings_data) / 2 - 1

        ld bc, 0h                       ; 10  ticks
        jp .loop
.lower_limit_reached:
        ld bc,  max_scroll_offset       ; 10 ticks
.loop:  
        ld a, 1                         ; 7 ticks
        out 0xfe,a                      ; 11 ticks

        push bc                         ; 11 ticks
        call draw_image_and_color       ; ~55000 ticks
        pop bc                          ; 10 ticks

        ld a, 2                         ; 7 ticks
        out 0xfe,a                      ; 11 ticks

        ; delay
        ld hl, timings_data
        add hl, bc
        add hl, bc

        ld e, (hl)
        inc l
        ld d, (hl)
        ld hl,  71680-74411;  // extra delay
        add hl, de

        call delay

        ; do increment
        dec bc
        ld a, b
        cp 0xff

        ; compare to N
        jp z, .lower_limit_reached      ; 10 ticks
        jp .loop                        ; 12 ticks
 
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
