        DEVICE zxspectrum48
        SLDOPT COMMENT WPMEM, LOGPOINT, ASSERTION //< More debug symbols.

screen_addr:    equ 16384
color_addr:     equ 5800h
screen_end:     equ 5b00h
generated_code: equ 5e00h

/*************** Image data. ******************/

    org generated_code

        INCBIN "resources/compressed_data.main"
        INCBIN "resources/compressed_data.mt_and_rt_reach.descriptor"
color_code
        INCBIN "resources/compressed_data.color"
multicolor_code
        INCBIN "resources/compressed_data.multicolor"


        align	2
rastr_for_mc_descriptors
        INCBIN "resources/compressed_data.rastr_for_mc.descriptors"
mc_descriptors
        INCBIN "resources/compressed_data.mc_descriptors"
rastr_for_offscreen_descriptor
        INCBIN "resources/compressed_data.rastr_for_offscreen.descriptors"

color_descriptor
        INCBIN "resources/compressed_data.color_descriptor"

        align	2
jpix_table
        INCBIN "resources/compressed_data.jpix"

        align	2
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
JP_IX_CODE      equ #e9dd
LD_BC_XXXX_CODE equ 01h
LD_DE_XXXX_CODE equ 11h

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
                pop hl ; skip restore address
        ENDM


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

draw_colors
        ; bc - line number / 4

        ld hl, color_descriptor                         ; 10
        add hl, bc                                      ; 11
        ld sp, hl                                       ; 6
        ; iy - address to execute
        pop iy                                          ; 14
        ; hl - end of exec address
        pop hl                                          ; 10

        ; write JP_IX to end exec address, 
        ; store original data to HL
        ; store end exec address to de
        ld sp, hl                                       ; 6
        ld de, JP_IX_CODE                               ; 10
        ex de, hl                                       ; 4
        ex (sp), hl                                     ; 19

        exx                                             ; 4
        ld sp, color_addr + 768
        ld ix, $ + 6                                    ; 14
        jp iy                                           ; 4
        exx                                             ; 4

        ; Restore data
        ex hl, de                                       ; 4

        ld (hl), e                                      ; 7
        inc hl                                          ; 6
        ld (hl), d                                      ; 7

        ; total 140

        jp draw_colors_end

draw_offscreen_rastr
        ; bc - line number
        ld (stack_bottom), sp                           ; 20

        ld hl, bc                                       ; 8
        ; save line number * 2
        add hl, hl                                      ; 11

        // -------- draw image (bottom)
        ld sp, 16384 + 1024 * 6
        ld bc, rastr_for_offscreen_descriptor           ; 10
        add hl, bc                                      ; 11
        .8 draw_8_lines

        // --------- draw image (middle)
        ld bc, (64 - 8) * 2                            ; 10
        add hl, bc                                     ; 11
        .8 draw_8_lines

        // ----------- draw image (top)
        add hl, bc                                      ; 11
        .8 draw_8_lines

        ld sp, (stack_bottom)                           ; 20
        ; // total: 92
        ret                

jp_ix_record_size       equ 4
jp_ix_records_per_line  equ 2 * 3
jp_ix_size_for_line     equ jp_ix_records_per_line * jp_ix_record_size

write_initial_jp_ix_table
        ld sp, jpix_table
        ld de, JP_IX_CODE
        ld b, 8 * jp_ix_records_per_line
.rep:   pop hl ; address
        ld (hl), e
        inc hl
        ld (hl), d
        pop hl ; skip restore data
        djnz .rep

        ld sp, stack_top - 2
        ret

jp_ix_line_delta EQU 8 * jp_ix_size_for_line
update_jp_ix_table
                ld (stack_bottom), sp

                // bc - screen address to draw
                ld hl, bc
                add hl, hl ; * 2
                add hl, hl ; * 4
                add hl, hl ; * 8
                ld de, hl
                add hl, hl ; * 16
                add hl, de ; * 24  // skip 6 records, 4 bytes each

                ld sp, jpix_table + jp_ix_line_delta
                add hl, sp
                ld sp, hl

                // 1. restore data
                exx
                .6 restore_jp_ix
                exx

                // 1. write new JP_IX
                ld sp, -jp_ix_line_delta
                add hl, sp
                ld sp, hl

                ld de, JP_IX_CODE
                .6 write_jp_ix_data
                // total 343

                ld sp, (stack_bottom)
                ret
       

    MACRO DRAW_RASTR_AND_MULTICOLOR_LINE N?:
        DRAW_MULTICOLOR_LINE  N?
        DRAW_RASTR_LINE N?
        // total ticks: 70 (86 with ret)
    ENDM                

    MACRO DRAW_RASTR_LINE N?:
                ld sp, screen_addr + ((N? + 8) % 24) * 256 + 256        ; 10
                ld ix, $ + 7                                            ; 14
RASTR_N?        jp 00 ; rastr for multicolor ( up to 8 lines)           ; 10
                // total ticks: 34 (40 with ret)
    ENDM                

    MACRO DRAW_MULTICOLOR_LINE N?:
                exx                                     ; 4
                ld sp, color_addr + N? * 32 + 32        ; 10
                ld ix, $ + 5                            ; 14
                jp hl                                   ; 4
                exx                                     ; 4
                // total ticks: 36 (44 with ret)
    ENDM                

draw_rastr_and_multicolor_lines:
        ld (stack_bottom), sp

        ld hl, bc
        add hl, hl // * 2
        ld sp, rastr_for_mc_descriptors
        add hl, sp
        ld sp, hl

        // Draw bottom 3-th of rastr during middle 3-th of colors
        exx
        pop hl: ld (RASTR_15+1), hl
        pop hl: ld (RASTR_14+1), hl
        pop hl: ld (RASTR_13+1), hl
        pop hl: ld (RASTR_12+1), hl
        pop hl: ld (RASTR_11+1), hl
        pop hl: ld (RASTR_10+1), hl
        pop hl: ld (RASTR_9+1), hl
        pop hl: ld (RASTR_8+1), hl
        exx

        // Draw middle 3-th of rastr during top 3-th of colors

        ld sp, 64*2
        add hl, sp
        ld sp, hl

        exx
        pop hl: ld (RASTR_7+1), hl
        pop hl: ld (RASTR_6+1), hl
        pop hl: ld (RASTR_5+1), hl
        pop hl: ld (RASTR_4+1), hl
        pop hl: ld (RASTR_3+1), hl
        pop hl: ld (RASTR_2+1), hl
        pop hl: ld (RASTR_1+1), hl
        pop hl: ld (RASTR_0+1), hl
        exx

        // Draw top 3-th of rastr during bottom 3-th of colors
        ld sp, 63*2  ; shift to 63 instead of 64 to move on next frame
        add hl, sp
        ld sp, hl

        exx
        pop hl: ld (RASTR_23+1), hl
        pop hl: ld (RASTR_22+1), hl
        pop hl: ld (RASTR_21+1), hl
        pop hl: ld (RASTR_20+1), hl
        pop hl: ld (RASTR_19+1), hl
        pop hl: ld (RASTR_18+1), hl
        pop hl: ld (RASTR_17+1), hl
        pop hl: ld (RASTR_16+1), hl
        exx

        ld a, 7
        and c
        ex af, af'

        ld hl, 7
        add hl, bc
        ld bc, hl

        // calculate flor(bc,8) / 2
        ld a, ~7
        and c
        srl b
        rra
        ld c, a

        jp draw_colors
draw_colors_end        

        // calculate ceil(bc,8) / 4
        srl b
        rr c

        ex af, af'
        // calculate address of the MC descriptors
        // Draw from 0 to 23 is backward direction
        jr nz, shift_line
        ld hl, mc_descriptors + 23*2
        jp common
shift_line: 
        ld hl, mc_descriptors + 22*2
common:

        add hl, bc
        ld e, (hl)
        inc l
        ld d, (hl)
        ex de, hl

        exx
        scf     // aligned data uses ret nc. prevent these ret

        ld a, 1                         ; 7 ticks
        out 0xfe,a                      ; 11 ticks

/*
        ld hl, 71680/2 - 10
        call delay
        ld hl, 71680/2 - 10
        call delay
*/        

        DRAW_RASTR_AND_MULTICOLOR_LINE 0
        DRAW_RASTR_AND_MULTICOLOR_LINE 1
        DRAW_RASTR_AND_MULTICOLOR_LINE 2
        DRAW_RASTR_AND_MULTICOLOR_LINE 3
        DRAW_RASTR_AND_MULTICOLOR_LINE 4
        DRAW_RASTR_AND_MULTICOLOR_LINE 5
        DRAW_RASTR_AND_MULTICOLOR_LINE 6
        DRAW_RASTR_AND_MULTICOLOR_LINE 7
        DRAW_RASTR_AND_MULTICOLOR_LINE 8
        DRAW_RASTR_AND_MULTICOLOR_LINE 9
        DRAW_RASTR_AND_MULTICOLOR_LINE 10
        DRAW_RASTR_AND_MULTICOLOR_LINE 11
        DRAW_RASTR_AND_MULTICOLOR_LINE 12
        DRAW_RASTR_AND_MULTICOLOR_LINE 13
        DRAW_RASTR_AND_MULTICOLOR_LINE 14
        DRAW_RASTR_AND_MULTICOLOR_LINE 15

        DRAW_RASTR_AND_MULTICOLOR_LINE 16
        DRAW_RASTR_AND_MULTICOLOR_LINE 17
        DRAW_RASTR_AND_MULTICOLOR_LINE 18
        DRAW_RASTR_AND_MULTICOLOR_LINE 19
        DRAW_RASTR_AND_MULTICOLOR_LINE 20
        DRAW_RASTR_AND_MULTICOLOR_LINE 21
        DRAW_RASTR_AND_MULTICOLOR_LINE 22
        ; draw rastr23 later, after updating JP_IX table
        DRAW_MULTICOLOR_LINE 23

        ld a, 2                         ; 7 ticks
        out 0xfe,a                      ; 11 ticks

        ld sp, (stack_bottom)
        ret


/*************** Main. ******************/
main:
        di
        ld sp, stack_top

        call copy_image
        call copy_colors

        call write_initial_jp_ix_table

        call prepare_interruption_table
        ei
        halt
after_interrupt:        
        ; Pentagon timings
first_timing_in_interrupt equ 21

        di                              ; 4 ticks
        ; remove interrupt data from stack
        pop af                          ; 10 ticks


screen_ticks  equ 71680
first_rastr_line_tick equ  17920
ticks_per_line equ  224

initial_delay equ 56170
sync_tick equ screen_ticks + first_rastr_line_tick - first_timing_in_interrupt - initial_delay

        ld hl, 32768
        call delay

        ld hl, sync_tick
        call delay

max_scroll_offset equ (timings_data_end - timings_data) / 2 - 1

        ld bc, 0h                       ; 10  ticks
        jp loop1
lower_limit_reached:
        ld bc,  max_scroll_offset       ; 10 ticks
loop:  
        //ld a, 1                         ; 7 ticks
        //out 0xfe,a                      ; 11 ticks

        call update_jp_ix_table
        exx
        scf
        ld (stack_top), sp
        DRAW_RASTR_LINE 23
        ld sp, (stack_top)
        exx

loop1:
        push bc
        call draw_offscreen_rastr
        pop bc

        push bc
        call draw_rastr_and_multicolor_lines
        pop bc

        //ld a, 2                         ; 7 ticks
        //out 0xfe,a                      ; 11 ticks
        ; delay
        ld hl, timings_data
        add hl, bc
        add hl, bc

        ld e, (hl)
        inc l
        ld d, (hl)
        ld hl,  71680-73809;  // extra delay
        add hl, de

        call delay

        ; do increment
        dec bc
        ld a, b
        cp 0xff

        ; compare to N
        jp z, lower_limit_reached      ; 10 ticks
        jp loop                        ; 12 ticks
 
/*************** Data segment ******************/
STACK_SIZE: equ 16  ; in words
stack_bottom:
        defs STACK_SIZE * 2, 0
stack_top:

        ; it need to update code. Currently it don't increment low part of the register.
data_segment_end:

/*************** Commands to SJ asm ******************/

    SAVESNA "build/draw_image.sna", main
    //savetap "build/draw_image.tap", main
