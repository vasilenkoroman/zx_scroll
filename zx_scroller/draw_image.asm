        DEVICE zxspectrum48
        SLDOPT COMMENT WPMEM, LOGPOINT, ASSERTION //< More debug symbols.

screen_addr:    equ 16384
color_addr:     equ 5800h
screen_end:     equ 5b00h
generated_code: equ 5e00h

JPIX__REF_TABLE_START   EQU screen_end
JPIX__REF_TABLE_END     EQU JPIX__REF_TABLE_START + 16
JPIX__BANKS_HELPER      EQU JPIX__REF_TABLE_END
JPIX__BANKS_HELPER_END  EQU JPIX__BANKS_HELPER + 128

STACK_SIZE:     equ 4  ; in words
stack_bottom    equ JPIX__BANKS_HELPER_END
stack_top       equ stack_bottom + STACK_SIZE * 2

        INCLUDE "resources/compressed_data.asm"

/*************** Image data. ******************/

    org generated_code

        INCBIN "resources/compressed_data.main"
        INCBIN "resources/compressed_data.mt_and_rt_reach.descriptor"
color_code
        INCBIN "resources/compressed_data.color"
multicolor_code
        INCBIN "resources/compressed_data.multicolor"

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

prepare_interruption_table:
        //save data
        LD   hl, #FE00
        LD   de, screen_end + 256
        LD   bc, #0200
        ldir

        // make interrupt table
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

        ei
        halt
after_interrupt:
        di                              ; 4 ticks
        ; remove interrupt data from stack
        pop af                          ; 10 ticks

        ; restore data
        LD   de, #FE00
        LD   hl, screen_end + 256
        LD   bc, #0200
        ldir

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

        ld (hl), e                                      ; 7
        inc hl                                          ; 6
        ld (hl), d                                      ; 7

        ; total 140

        ENDM

        MACRO DRAW_OFFSCREEN_LINES N?:
                IF (UNSTABLE_STACK_POS == 1)
                        ld sp, screen_addr + 6144 - N? * 256      ; 10
                ENDIF
                IF (N? == 0)
                        ld ix, $ + 7     ; 14
                ELSEIF (low($+6) > 10)
                        ld ixl, low($+6) ; 11
                ELSE
                        ld ix, $ + 7     ; 14
                ENDIF                        
OFF_RASTR_N?    jp 00 ; rastr for multicolor ( up to 8 lines)           ; 10
                // total ticks: 24
        ENDM                


jp_ix_record_size       equ 8
jp_ix_bank_size         equ (imageHeight/64 + 2) * jp_ix_record_size
write_initial_jp_ix_table
                // create banks helper (to avoid mul in runtime)
                ld b, 64
                ld hl, jpix_table + jp_ix_bank_size * 64
                ld de, -jp_ix_bank_size
                ld sp, JPIX__BANKS_HELPER_END
.helper:        add hl, de
                push hl
                djnz .helper

                // main data
                ld sp, jpix_table
                ; skip several descriptors
                ld c, 8
.loop:          ld b, 6                         ; write 0, 64, 128-th descriptors for start line
                ; read sp to de
                
                ld hl, 0
                add hl, sp
                ld de, hl

                ; fill JPIX_REF_TABLE
                ld a, 8
                sub c
                rla
                ld h, high(JPIX__REF_TABLE_START)
                ld l, a
                ld (hl), e
                inc l
                ld (hl), d

                ld de, JP_IX_CODE
.rep:           
                pop hl                          ; address
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
      

    MACRO DRAW_MULTICOLOR_LINE N?:
                exx                                     ; 4
                ld sp, color_addr + N? * 32 + 16        ; 10
                ld iy, color_addr + N? * 32 + 32        ; 14
                ld ix, $ + 5                            ; 14
                jp hl                                   ; 4
                exx                                     ; 4
                // total ticks: 50 (58 with ret)
    ENDM                

    MACRO DRAW_RASTR_LINE N?:
                ld sp, screen_addr + ((N? + 8) % 24) * 256 + 256        ; 10
                ld ix, $ + 7                                            ; 14
RASTR_N?        jp 00 ; rastr for multicolor ( up to 8 lines)           ; 10
                // total ticks: 34 (42 with ret)
    ENDM                

    MACRO DRAW_MULTICOLOR_AND_RASTR_LINE N?:
        DRAW_MULTICOLOR_LINE  N?
        DRAW_RASTR_LINE N?
        // total ticks: 70 (86 with ret)
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

long_delay:
        exx
        ld b, 5
rep:    ld hl, 65535
        call delay
        dec a
        djnz rep
        exx
        ret

/*************** Main. ******************/
main:
        di
        ld sp, stack_top

        call copy_image
        call copy_colors

        call prepare_interruption_table
        ; Pentagon timings
first_timing_in_interrupt       equ 22
screen_ticks                    equ 71680
first_rastr_line_tick           equ  17920
screen_start_tick               equ  17988
ticks_per_line                  equ  224


        call write_initial_jp_ix_table

mc_preambula_delay      equ 46
fixed_startup_delay     equ 46210
initial_delay           equ first_timing_in_interrupt + fixed_startup_delay +  mc_preambula_delay + MULTICOLOR_DRAW_PHASE
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
        // --------------------- update_jp_ix_table --------------------------------
        //MACRO update_jp_ix_table
                // bc - screen address to draw

                ; a = bank number
                ld a, 0x3f
                and c

                rla                             ; 4
                add low(JPIX__BANKS_HELPER)     ; 7
                ld l, a                         ; 4
                ld h, high(JPIX__BANKS_HELPER)  ; 7
                ld sp, hl                       ; 6
                pop hl  ; hl = bank offset      ; 10
                ; ticks: 38

                ; bc =  offset in bank (bc/8)
                ld a, 0xc0
                and c
                ld d, b
                rr d: rra
                rr d: rra
                rr d: rra
                ld e, a
                ; hl - pointer to new record in JPIX table
                add hl, de

                ; prepare  N-th element for jpix_table_ref in a
                ld a, 7                                 ; 4
                and c                                   ; 7
                rla                                     ; 4

                // 1. write new JP_IX
                ld sp, hl
                exx
                ld de, JP_IX_CODE
                .5 write_jp_ix_data 1
                write_jp_ix_data 0

                // 2. restore data
                ; hl = jpix_table_ref
                ld h, high(JPIX__REF_TABLE_START)       ; 7
                ld l, a                                 ; 4
                ld sp, hl                               ; 6
                ; put new pointer to jpix_table_ref, get old pointer to restore data in hl
                exx                                     ; 4
                ex (sp), hl                             ; 19
                ld sp, hl                               ; 6
                .6 restore_jp_ix
                // total 666
        // ------------------------- update jpix table end
        exx
        scf     // aligned data uses ret nc. prevent these ret
        DRAW_RASTR_LINE 23
        exx

loop1:
        prepare_rastr_drawing
        
        //MACRO draw_offscreen_rastr
                IF (UNSTABLE_STACK_POS == 0)        
                        ld sp, screen_addr + 6144
                ENDIF                        
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
        //ENDM

        // -------------------------------- DRAW_MULTICOLOR_AND_RASTR_LINES -----------------------------------------
        ld (stack_bottom), bc

        // calculate ceil(bc,8) / 2

        ld hl, 7
        add hl, bc

        ld a, ~7
        and l
        srl h : rra
        ld l, a

        draw_colors

        ; delay
        ld hl, timings_data
        add hl, bc
        add hl, bc
        ld sp, hl
        pop hl
        ld sp, stack_top
        call delay

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
        
        ld a, 1                         ; 7 ticks
        out 0xfe,a                      ; 11 ticks

        ; timing here on first frame: 91168
        scf     // aligned data uses ret nc. prevent these ret
        DRAW_MULTICOLOR_AND_RASTR_LINE 0
        DRAW_MULTICOLOR_AND_RASTR_LINE 1
        DRAW_MULTICOLOR_AND_RASTR_LINE 2
        DRAW_MULTICOLOR_AND_RASTR_LINE 3
        DRAW_MULTICOLOR_AND_RASTR_LINE 4
        DRAW_MULTICOLOR_AND_RASTR_LINE 5
        DRAW_MULTICOLOR_AND_RASTR_LINE 6
        DRAW_MULTICOLOR_AND_RASTR_LINE 7
        
        DRAW_MULTICOLOR_AND_RASTR_LINE 8
        DRAW_MULTICOLOR_AND_RASTR_LINE 9
        DRAW_MULTICOLOR_AND_RASTR_LINE 10
        DRAW_MULTICOLOR_AND_RASTR_LINE 11
        DRAW_MULTICOLOR_AND_RASTR_LINE 12
        DRAW_MULTICOLOR_AND_RASTR_LINE 13
        DRAW_MULTICOLOR_AND_RASTR_LINE 14

        DRAW_MULTICOLOR_AND_RASTR_LINE 15
        DRAW_MULTICOLOR_AND_RASTR_LINE 16
        DRAW_MULTICOLOR_AND_RASTR_LINE 17
        DRAW_MULTICOLOR_AND_RASTR_LINE 18
        DRAW_MULTICOLOR_AND_RASTR_LINE 19
        DRAW_MULTICOLOR_AND_RASTR_LINE 20
        DRAW_MULTICOLOR_AND_RASTR_LINE 21
        DRAW_MULTICOLOR_AND_RASTR_LINE 22
        ; draw rastr23 later, after updating JP_IX table
        DRAW_MULTICOLOR_LINE 23

        //ld sp, stack_top
        //call long_delay

        ld a, 2                         ; 7 ticks
        out 0xfe,a                      ; 11 ticks
        ld bc, (stack_bottom)
        // -------------------------- DRAW_MULTICOLOR_AND_RASTR_LINEs_done ------------------------------

        dec bc
        ; compare to -1
        ld a, b
        inc a
        jp z, lower_limit_reached      ; 10 ticks
        jp loop                        ; 12 ticks
 
/*************** Commands to SJ asm ******************/

    SAVESNA "build/draw_image.sna", main
    //savetap "build/draw_image.tap", main
