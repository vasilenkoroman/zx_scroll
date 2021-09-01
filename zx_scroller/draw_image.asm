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
rastr_descriptors
        INCBIN "resources/compressed_data.rastr.descriptors"
mc_descriptors
        INCBIN "resources/compressed_data.mc_descriptors"

color_descriptor
        INCBIN "resources/compressed_data.color_descriptor"

jpix_table
        INCBIN "resources/compressed_data.jpix"

timings_data
        INCBIN "resources/compressed_data.timings"
timings_data_end

src_data
        INCBIN "resources/samanasuke.bin", 0, 6144
color_data:
        INCBIN "resources/samanasuke.bin", 6144, 768
data_end:

imageHeight     equ (timings_data_end - timings_data) / 2

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

        MACRO draw_colors
        ; hl - line number / 4

        ld de, color_descriptor
        add hl, de                                      ; 10
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

        ld (hl), e                                      ; 10
        inc hl                                          ; 6
        ld (hl), d                                      ; 10

        ; total 140

        ENDM

        MACRO DRAW_OFFSCREEN_LINES N?:
                ld ix, $ + 7                                            ; 14
OFF_RASTR_N?    jp 00 ; rastr for multicolor ( up to 8 lines)           ; 10
                // total ticks: 24
        ENDM                


        MACRO draw_offscreen_rastr
                ld sp, 16384 + 1024 * 6
                exx
                DRAW_OFFSCREEN_LINES 0
                DRAW_OFFSCREEN_LINES 1
                DRAW_OFFSCREEN_LINES 2
                DRAW_OFFSCREEN_LINES 3
                DRAW_OFFSCREEN_LINES 4
                DRAW_OFFSCREEN_LINES 5
                DRAW_OFFSCREEN_LINES 6
                DRAW_OFFSCREEN_LINES 7
                DRAW_OFFSCREEN_LINES 8
                DRAW_OFFSCREEN_LINES 9
                DRAW_OFFSCREEN_LINES 10
                DRAW_OFFSCREEN_LINES 11
                DRAW_OFFSCREEN_LINES 12
                DRAW_OFFSCREEN_LINES 13
                DRAW_OFFSCREEN_LINES 14
                DRAW_OFFSCREEN_LINES 15
                DRAW_OFFSCREEN_LINES 16
                DRAW_OFFSCREEN_LINES 17
                DRAW_OFFSCREEN_LINES 18
                DRAW_OFFSCREEN_LINES 19
                DRAW_OFFSCREEN_LINES 20
                DRAW_OFFSCREEN_LINES 21
                DRAW_OFFSCREEN_LINES 22
                DRAW_OFFSCREEN_LINES 23
                exx
        ENDM

        ; max A value here is 63
        MACRO MUL_A value
                IF value == 40
                        ld h, 0
                        rla        ; * 2
                        rla        ; * 4
                        ld l, a
                        add hl, hl ; * 8
                        ld de, hl
                        add hl, hl ; * 16
                        add hl, hl ; * 32
                        add hl, de ; * 40
                ELSEIF value == 64
                        ld l, 0
                        rra
                        rr l
                        rra
                        rr l
                        ld h, a
                ELSE
                    ASSERT 0
                ENDIF
        ENDM

jp_ix_record_size       equ 8
write_initial_jp_ix_table
                ld sp, jpix_table
                ; skip several descriptors
                ld de, JP_IX_CODE

                ld c, 8
.loop:          ld b, 6                         ; write 0, 64, 128-th descriptors for start line
.rep:           pop hl                          ; address
                ld (hl), e
                inc hl
                ld (hl), d
                pop hl                          ; skip restore data
                djnz .rep
                ld hl, (imageHeight/64 - 3 + 2) * jp_ix_record_size
                add hl, sp
                ld sp, hl
                dec c
                jr nz, .loop


                ld sp, stack_top - 2
                ret

        MACRO restore_jp_ix
                pop hl ; address
                pop de ; restore data
                ld (hl), e
                inc hl
                ld (hl), d
        ENDM
        MACRO write_jp_ix_data skipRestoreData
                pop hl ; address
                ld (hl), e
                inc hl
                ld (hl), d
                IF (skipRestoreData == 1)
                        pop hl ; skip restore address
                ENDIF                        
        ENDM

jpix_bank_size          EQU (imageHeight/64 + 2) * jp_ix_record_size
        MACRO update_jp_ix_table
                // bc - screen address to draw

                ; a = bank number
                ld a, 0x3f
                and c

                ; hl = bank offset
                MUL_A jpix_bank_size

                ; bc =  offset in bank (bc/8)
                ld a, 0xc0
                and c
                ld d, b
                rr d: rra
                rr d: rra
                rr d: rra
                ld e, a
                add hl, de

                // 1. write new JP_IX
                ld sp, jpix_table
                add hl, sp
                ld sp, hl

                ld de, JP_IX_CODE
                .5 write_jp_ix_data 1
                write_jp_ix_data 0

                // 2. restore data

                cp (imageHeight/64 - 1) * 8
                jr nz, eight_bank_correct
                ld hl, jpix_bank_size * 7 + jp_ix_record_size*3 - 11*2
                jr common_bank
eight_bank_correct:
                ld hl, jpix_bank_size * 8 + jp_ix_record_size - 11*2
                jr z, common_bank ; align branch duration
common_bank
                add hl, sp
                ld sp, hl

                .6 restore_jp_ix

                // total 712
        ENDM
       

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

        MACRO prepare_rastr_drawing
        ld hl, bc
        add hl, hl // * 2
        add hl, hl // * 4
        ld sp, rastr_descriptors
        add hl, sp
        ld sp, hl

        // Draw bottom 3-th of rastr during middle 3-th of colors
        exx
        pop hl: ld (RASTR_15+1), hl:    pop hl: ld (OFF_RASTR_0+1), hl
        pop hl: ld (RASTR_14+1), hl:    pop hl: ld (OFF_RASTR_1+1), hl
        pop hl: ld (RASTR_13+1), hl:    pop hl: ld (OFF_RASTR_2+1), hl
        pop hl: ld (RASTR_12+1), hl:    pop hl: ld (OFF_RASTR_3+1), hl
        pop hl: ld (RASTR_11+1), hl:    pop hl: ld (OFF_RASTR_4+1), hl
        pop hl: ld (RASTR_10+1), hl:    pop hl: ld (OFF_RASTR_5+1), hl
        pop hl: ld (RASTR_9+1), hl:     pop hl: ld (OFF_RASTR_6+1), hl
        pop hl: ld (RASTR_8+1), hl:     pop hl: ld (OFF_RASTR_7+1), hl
        exx

        // Draw middle 3-th of rastr during top 3-th of colors

        inc h
        ld sp, hl

        pop hl: ld (RASTR_7+1), hl:    pop hl: ld (OFF_RASTR_8+1), hl
        pop hl: ld (RASTR_6+1), hl:    pop hl: ld (OFF_RASTR_9+1), hl
        pop hl: ld (RASTR_5+1), hl:    pop hl: ld (OFF_RASTR_10+1), hl
        pop hl: ld (RASTR_4+1), hl:    pop hl: ld (OFF_RASTR_11+1), hl
        pop hl: ld (RASTR_3+1), hl:    pop hl: ld (OFF_RASTR_12+1), hl
        pop hl: ld (RASTR_2+1), hl:    pop hl: ld (OFF_RASTR_13+1), hl
        pop hl: ld (RASTR_1+1), hl:    pop hl: ld (OFF_RASTR_14+1), hl
        pop hl: ld (RASTR_0+1), hl:    pop hl: ld (OFF_RASTR_15+1), hl

        // Draw top 3-th of rastr during bottom 3-th of colors
        ; shift to 63 for MC rastr instead of 64 to move on next frame
        ld hl, (63-8) * 4  
        add hl, sp
        ld sp, hl

        pop hl: ld (RASTR_23+1), hl:    pop hl
        pop hl: ld (RASTR_22+1), hl:    pop hl: ld (OFF_RASTR_16+1), hl
        pop hl: ld (RASTR_21+1), hl:    pop hl: ld (OFF_RASTR_17+1), hl
        pop hl: ld (RASTR_20+1), hl:    pop hl: ld (OFF_RASTR_18+1), hl
        pop hl: ld (RASTR_19+1), hl:    pop hl: ld (OFF_RASTR_19+1), hl
        pop hl: ld (RASTR_18+1), hl:    pop hl: ld (OFF_RASTR_20+1), hl
        pop hl: ld (RASTR_17+1), hl:    pop hl: ld (OFF_RASTR_21+1), hl
        pop hl: ld (RASTR_16+1), hl:    pop hl: ld (OFF_RASTR_22+1), hl
        pop hl:                         pop hl: ld (OFF_RASTR_23+1), hl

        ENDM


draw_rastr_and_multicolor_lines:

        // calculate ceil(bc,8) / 2

        ld hl, 7
        add hl, bc

        ld a, ~7
        and l
        srl h : rra
        ld l, a

        draw_colors

        // calculate floor(bc,8) / 4

        ld a, ~7
        and c
        srl b : rra
        srl b : rra
        ld c, a

        // calculate address of the MC descriptors
        // Draw from 0 to 23 is backward direction
        ld hl, mc_descriptors + 23*2
        // prepare in HL' multicolor address to execute
        add hl, bc
        ld sp, hl
        pop hl
        exx
        
        scf     // aligned data uses ret nc. prevent these ret

        ld a, 1                         ; 7 ticks
        out 0xfe,a                      ; 11 ticks

        ; timing here on first frame: 91182
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

        jp draw_rastr_and_multicolor_lines_done

/*
long_delay:
        push bc
        ld b, 2
rep:    ld hl, 65535
        call delay
        djnz rep
        pop bc
        ret
*/

/*************** Main. ******************/
main:
        di
        ld sp, stack_top

        call copy_image
        call copy_colors

        call prepare_interruption_table
        ei
        halt
after_interrupt:        
        ; Pentagon timings
first_timing_in_interrupt       equ 22
screen_ticks                    equ 71680
first_rastr_line_tick           equ  17920
screen_start_tick               equ  17988
ticks_per_line                  equ  224

        di                              ; 4 ticks
        ; remove interrupt data from stack
        pop af                          ; 10 ticks

        call write_initial_jp_ix_table

mc_preambula_delay      equ 32
initial_delay           equ first_timing_in_interrupt + 32442 +  mc_preambula_delay
sync_tick equ screen_ticks + screen_start_tick  - initial_delay + 224*7
        assert (sync_tick <= 65535)

        ld hl, sync_tick
        call delay

max_scroll_offset equ imageHeight - 1

        ld bc, 0h                       ; 10  ticks
        jp loop1
lower_limit_reached:
        ld bc,  max_scroll_offset       ; 10 ticks
loop:  
        update_jp_ix_table

        exx
        scf
        DRAW_RASTR_LINE 23
        exx

loop1:
        ; delay
        ld hl, timings_data
        add hl, bc
        add hl, bc
        ld sp, hl
        pop hl

        ld sp, stack_top
        call delay

        prepare_rastr_drawing
        draw_offscreen_rastr

        ld (stack_bottom), bc
        jp draw_rastr_and_multicolor_lines
draw_rastr_and_multicolor_lines_done
        ld bc, (stack_bottom)

        ; do increment
        dec bc
        ld a, b
        inc a

        ; compare to N
        jp z, lower_limit_reached      ; 10 ticks
        jp loop                        ; 12 ticks
 
/*************** Data segment ******************/
STACK_SIZE: equ 1  ; in words
stack_bottom:
        defs STACK_SIZE * 2, 0
stack_top:

        ; it need to update code. Currently it don't increment low part of the register.
data_segment_end:

/*************** Commands to SJ asm ******************/

    SAVESNA "build/draw_image.sna", main
    //savetap "build/draw_image.tap", main
