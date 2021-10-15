        DEVICE zxspectrum128
        SLDOPT COMMENT WPMEM, LOGPOINT, ASSERTION //< More debug symbols.

/*
 * Page locations:
 * 0,1, 3,4 - rastrData + JP_IX_TABLE
 * 2 - static page 0x8000
 * 5 - static page 0x4000
 * Pages 2,5 include: reach descriptors, MC data
 * 6 - music data
 * 7 - second screen, second screen code, other descriptors
 * page 7: color data from address #1b00
 * short port page selection:
 *    LD A, #50 + screen_num*8 + page_number
 *    OUT (#fd), A
 **/


screen_addr:    equ 16384
color_addr:     equ 5800h
screen_end:     equ 5b00h
start:          equ 5e00h

JPIX__REF_TABLE_START   EQU screen_end
JPIX__REF_TABLE_END     EQU JPIX__REF_TABLE_START + 16
UPDATE_JPIX_WRITE       EQU JPIX__REF_TABLE_END
UPDATE_JPIX_RESTORE     EQU JPIX__REF_TABLE_END + 2

SET_PAGE_HELPER         EQU high(JPIX__REF_TABLE_END)*256 + 0x50
SET_PAGE_HELPER_END     EQU SET_PAGE_HELPER + 8

JPIX__BANKS_HELPER      EQU SET_PAGE_HELPER_END
JPIX__BANKS_HELPER_END  EQU JPIX__BANKS_HELPER + 128

DEBUG_MODE              EQU 0

STACK_SIZE:     equ 4  ; in words
stack_bottom    equ JPIX__BANKS_HELPER_END
stack_top       equ stack_bottom + STACK_SIZE * 2

        INCLUDE "resources/compressed_data.asm"

    org start

        MACRO draw_colors
        ; hl - line number / 2

        ld de, color_descriptor                         ; 10
        add hl, de                                      ; 11
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
        ld sp, color_addr + 768                         ; 10
        ld ix, $ + 6                                    ; 14
        jp iy                                           ; 8
        exx                                             ; 4

        ; Restore data
        ex hl, de                                       ; 4

        ld (hl), e                                      ; 7
        inc hl                                          ; 6
        ld (hl), d                                      ; 7

        ; total 154 (162 with ret)

        ENDM

        MACRO DRAW_OFFSCREEN_LINES N?, Prev?
                IF (UNSTABLE_STACK_POS == 1)
                        ld sp, screen_addr + 6144 - N? * 256      ; 10
                ENDIF
OFF_BASE_N?                
                IF (N? == Prev?)
                        ld ix, $ + 7     ; 14
                ELSEIF (high(OFF_BASE_N? + 7) == high(OFF_BASE_Prev? + 7))
                        ld ixl, low($+6) ; 11
                ELSE
                        ld ix, $ + 7     ; 14
                ENDIF                        
OFF_RASTR_N?    jp 00 ; rastr for multicolor ( up to 8 lines)           ; 10
                // total ticks: 24
        ENDM                

        MACRO DRAW_OFFSCREEN_LINES2 N?, Prev?
                IF (UNSTABLE_STACK_POS == 1)
                        ld sp, screen_addr + 6144 - N? * 256      ; 10
                ENDIF
OFF_BASE2_N?                
                IF (N? == Prev?)
                        ld ix, $ + 7     ; 14
                ELSEIF (high(OFF_BASE2_N? + 7) == high(OFF_BASE2_Prev? + 7))
                        ld ixl, low($+6) ; 11
                ELSE
                        ld ix, $ + 7     ; 14
                ENDIF                        
OFF_RASTR2_N?   jp 00 ; rastr for multicolor ( up to 8 lines)           ; 10
                // total ticks: 24
        ENDM                

        MACRO restore_jp_ix
                pop hl ; address
                pop de ; restore data
                ld (hl), e
                inc hl
                ld (hl), d
        ENDM
        MACRO write_jp_ix_data skipRestoreData
                pop hl ; address
                ld (hl), c
                inc hl
                ld (hl), b
                IF (skipRestoreData == 1)
                        pop hl ; skip restore address
                ENDIF                        
        ENDM

jpix_bank_size          EQU (imageHeight/64 + 2) * jp_ix_record_size
      

    MACRO DRAW_MULTICOLOR_LINE N?:
                IF (N? != 0)
                        exx                                     ; 4
                ENDIF                        
                ld sp, color_addr + N? * 32 + 16        ; 10
                ld iy, color_addr + N? * 32 + 32        ; 14
MC_LINE_N?      ld ix, $ + 5                            ; 14
                jp hl                                   ; 4
                IF (N? != 23)
                        exx                             ; 4
                ENDIF                        
                // total ticks: 50 (58 with ret)
    ENDM                

    MACRO DRAW_MULTICOLOR_LINE2 N?:
                IF (N? != 0)
                        exx                                     ; 4
                ENDIF                        
                ld sp, color_addr + N? * 32 + 16        ; 10
                ld iy, color_addr + N? * 32 + 32        ; 14
MC_LINE2_N?     ld ix, $ + 5                            ; 14
                jp hl                                   ; 4
                IF (N? != 23)
                        exx                             ; 4
                ENDIF                        
                // total ticks: 50 (58 with ret)
    ENDM                

    MACRO DRAW_MULTICOLOR_AND_RASTR_LINE N?, N2?:
                DRAW_MULTICOLOR_LINE  N?
        
                //ld sp, screen_addr + ((N? + 8) % 24) * 256 + 256       ; 10
                ld sp, screen_addr + (((N? + 8) / 8) % 3) * 2048 + N2? * 256 + 256
                ASSERT(high($+6) == high(MC_LINE_N? + 5))
                ld ixl, low($ + 6)                             ; 11
RASTR_N?        jp 00 ; rastr for multicolor ( up to 8 lines)          ; 10        
        // total ticks: 70 (86 with ret)
    ENDM                

    MACRO DRAW_MULTICOLOR_AND_RASTR_LINE2 N?:
                DRAW_MULTICOLOR_LINE2  N?
        
                ld sp, screen_addr + ((N? + 8) % 24) * 256 + 256       ; 10
                ASSERT(high($+6) == high(MC_LINE2_N? + 5))
                ld ixl, low($ + 6)                             ; 11
RASTR2_N?       jp 00 ; rastr for multicolor ( up to 8 lines)          ; 10        
        // total ticks: 70 (86 with ret)
    ENDM                

        MACRO SET_PAGE page_number
                LD A, #50 + page_number
                OUT (#fd), A
        ENDM

        MACRO set_logical_page
                ; a - bank number
                cp 2
                ccf
                adc 0x50
                out (0xfd), a
        ENDM

        MACRO set_page_by_bank
                ; a - bank number
                rra
                set_logical_page
        ENDM

        MACRO next_page
                ld h, high(SET_PAGE_HELPER)     ; 7
                ld l, a                         ; 4
                ld a, (hl)                      ; 7
                out (0xfd), a                   ; 11
                ; total: 29
        ENDM

/************* Routines **********************/

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
        MACRO DO_DELAY

//d1      equ     17 + 15+21+11+15+14
d1      equ     15+21+11+15+14 + 7 - 5
d2      equ     16
d3      equ     20+10+11+12

base
delay   xor     a               ; 4
        or      h               ; 4
        jr      nz,wait1        ; 12/7  20/15
        ld      a,l             ; 4
        sub     d1              ; 7
        ld      hl,high(base)*256 + d2      ; 10    21
wait2   sub     l               ; 4
        jr      nc,wait2        ; 12/7  16/11
        sub (256-16) - low(table)
        ld      l,a             ; 4
        ld      l,(hl)          ; 7
        jp      (hl)            ; 4     15

table
        db      low(t09), low(t10), low(t11), low(t12)
        db      low(t13), low(t14), low(t15), low(t16)
        db      low(t17), low(t18), low(t19), low(t20)
        db      low(t21), low(t22), low(t23), low(t24)
        db      low(t25), low(t26), low(t27)
table_end

wait1   ld      de,-d3          ; 10
        add     hl,de           ; 11
        jr      delay           ; 12    33

t24     nop
t20     nop
t16     nop
t12     jr delay_end

t27     nop
t23     nop
t19     nop
t15     nop
t11     ld l, low(delay_end)
        jp (hl)

t26     nop
t22     nop
t18     nop
t14     nop
t10     jp delay_end

t25     nop
t21     nop
t17     nop
t13     nop
t09     ld a, r
delay_end
        ASSERT high(delay_end) == high(base)
        ENDM
/************** end delay routine *************/        

filler  defs 0, 0   // align code data

/*************** Main. ******************/
main:
        di
        ld sp, stack_top

        //call copy_image
        //call copy_colors

        call prepare_interruption_table
        ; Pentagon timings
first_timing_in_interrupt       equ 19 + 22
screen_ticks                    equ 71680
first_rastr_line_tick           equ  17920
screen_start_tick               equ  17988
ticks_per_line                  equ  224


        call create_page_helper
        call write_initial_jp_ix_table

mc_preambula_delay      equ 46
fixed_startup_delay     equ 17837 + 6 // I can see one blinking byte in spectaculator. moved forward just in case
initial_delay           equ first_timing_in_interrupt + fixed_startup_delay +  mc_preambula_delay + MULTICOLOR_DRAW_PHASE
sync_tick               equ screen_ticks + screen_start_tick  - initial_delay - FIRST_LINE_DELAY
        assert (sync_tick <= 65535)
        call static_delay

max_scroll_offset equ imageHeight - 1

        ld a, 3                         ; 7 ticks
        out 0xfe,a                      ; 11 ticks

        // Prepare data
        xor a   ; TODO: load from RASTR_REG_A. In current version its const 0
        ld hl, COLOR_REG_AF2
        push hl
        ex af, af'
        pop af
        ex af, af'

        ld bc, 0h                       ; 10  ticks
        jp loop1
lower_limit_reached:
        ld bc,  max_scroll_offset       ; 10 ticks
loop:  
jp_ix_line_delta_in_bank EQU 2 * 6*4
        // --------------------- update_jp_ix_table (second half) --------------------------------
        //MACRO update_jp_ix_table
                LD sp, (UPDATE_JPIX_WRITE)
                LD DE, JP_IX_CODE
                .3 write_jp_ix_data 1
                write_jp_ix_data 0
                LD sp, (UPDATE_JPIX_RESTORE)
                .3 restore_jp_ix
                // total 666
        // ------------------------- update jpix table end

loop1:
        // calculate ceil(bc,8) / 2
        ld hl, 7
        add hl, bc

        ld a, ~7
        and l
        srl h : rra
        ld l, a

        SET_PAGE 7
        draw_colors


        //MACRO prepare_rastr_drawing
        ld hl, bc
        add hl, hl // * 2
        add hl, hl // * 4
        ld sp, rastr_descriptors
        add hl, sp
        ld sp, hl

        // Swap odd/even bank drawing if need. Always start drawing from odd bank number
        // because it has set_page command in descriptor (even banks don't have it).
        // Also, draw in order 0..7 instead of 7..0. It can be used on this third because there is no ray conflict here.
        // It need to finish drawing on the last page and followed update_jpix routine will use same page.
        bit 0, c
        jp nz, odd_bank_drawing

                // Draw bottom 3-th of rastr during middle 3-th of colors
                exx
                pop hl: ld (OFF_RASTR_0+1), hl:    pop hl: ld (RASTR_15+1), hl
                pop hl: ld (OFF_RASTR_1+1), hl:    pop hl: ld (RASTR_14+1), hl
                pop hl: ld (OFF_RASTR_2+1), hl:    pop hl: ld (RASTR_13+1), hl
                pop hl: ld (OFF_RASTR_3+1), hl:    pop hl: ld (RASTR_12+1), hl
                pop hl: ld (OFF_RASTR_4+1), hl:    pop hl: ld (RASTR_11+1), hl
                pop hl: ld (OFF_RASTR_5+1), hl:    pop hl: ld (RASTR_10+1), hl
                pop hl: ld (OFF_RASTR_6+1), hl:    pop hl: ld (RASTR_9+1), hl
                pop hl: ld (OFF_RASTR_7+1), hl:    pop hl: ld (RASTR_8+1), hl
                exx

                // Draw middle 3-th of rastr during top 3-th of colors

                inc h
                ld sp, hl

                pop hl: ld (OFF_RASTR_8+1), hl:    pop hl: ld (RASTR_7+1), hl
                pop hl: ld (OFF_RASTR_9+1), hl:    pop hl: ld (RASTR_6+1), hl
                pop hl: ld (OFF_RASTR_10+1), hl:   pop hl: ld (RASTR_5+1), hl
                pop hl: ld (OFF_RASTR_11+1), hl:   pop hl: ld (RASTR_4+1), hl
                pop hl: ld (OFF_RASTR_12+1), hl:   pop hl: ld (RASTR_3+1), hl
                pop hl: ld (OFF_RASTR_13+1), hl:   pop hl: ld (RASTR_2+1), hl
                pop hl: ld (OFF_RASTR_14+1), hl:   pop hl: ld (RASTR_1+1), hl
                pop hl: ld (OFF_RASTR_15+1), hl:   pop hl: ld (RASTR_0+1), hl

                // Draw top 3-th of rastr during bottom 3-th of colors
                ; shift to 63 for MC rastr instead of 64 to move on next frame
                ld hl, (63-8) * 4 + 2
                add hl, sp
                ld sp, hl

                                                    pop hl: ld (RASTR_22+1), hl
                pop hl: ld (OFF_RASTR_16+1), hl:    pop hl: ld (RASTR_21+1), hl
                pop hl: ld (OFF_RASTR_17+1), hl:    pop hl: ld (RASTR_20+1), hl
                pop hl: ld (OFF_RASTR_18+1), hl:    pop hl: ld (RASTR_19+1), hl
                pop hl: ld (OFF_RASTR_19+1), hl:    pop hl: ld (RASTR_18+1), hl
                pop hl: ld (OFF_RASTR_20+1), hl:    pop hl: ld (RASTR_17+1), hl
                pop hl: ld (OFF_RASTR_21+1), hl:    pop hl: ld (RASTR_16+1), hl
                pop hl: ld (OFF_RASTR_22+1), hl:    pop hl: ld (RASTR_23+1), hl
                pop hl: ld (OFF_RASTR_23+1), hl:    

                //ENDM
        
                // -------------------------------- (even) DRAW_RASTR_LINES -----------------------------------------

                ld a, c
                and 7
                set_page_by_bank
                scf
                exx

                DRAW_OFFSCREEN_LINES 17, 17
                DRAW_OFFSCREEN_LINES 16, 17
                DRAW_OFFSCREEN_LINES 9, 16
                DRAW_OFFSCREEN_LINES 8, 9
                DRAW_OFFSCREEN_LINES 1, 8
                DRAW_OFFSCREEN_LINES 0, 1

                next_page

                DRAW_OFFSCREEN_LINES 19, 0
                DRAW_OFFSCREEN_LINES 18, 19
                DRAW_OFFSCREEN_LINES 11, 18
                DRAW_OFFSCREEN_LINES 10, 11
                DRAW_OFFSCREEN_LINES 3, 10
                DRAW_OFFSCREEN_LINES 2, 3

                next_page

                DRAW_OFFSCREEN_LINES 21, 2
                DRAW_OFFSCREEN_LINES 20, 21
                DRAW_OFFSCREEN_LINES 13, 20
                DRAW_OFFSCREEN_LINES 12, 13
                DRAW_OFFSCREEN_LINES 5, 12
                DRAW_OFFSCREEN_LINES 4, 5

                next_page

                DRAW_OFFSCREEN_LINES 23, 4
                DRAW_OFFSCREEN_LINES 22, 23
                DRAW_OFFSCREEN_LINES 15, 22
                DRAW_OFFSCREEN_LINES 14, 15
                DRAW_OFFSCREEN_LINES 7, 14
                DRAW_OFFSCREEN_LINES 6, 7
                
                exx
                jp bank_drawing_common
odd_bank_drawing:        

                // Draw bottom 3-th of rastr during middle 3-th of colors
                exx
                pop hl: ld (OFF_RASTR2_0+1), hl:    pop hl: ld (RASTR2_15+1), hl
                pop hl: ld (OFF_RASTR2_1+1), hl:    pop hl: ld (RASTR2_14+1), hl
                pop hl: ld (OFF_RASTR2_2+1), hl:    pop hl: ld (RASTR2_13+1), hl
                pop hl: ld (OFF_RASTR2_3+1), hl:    pop hl: ld (RASTR2_12+1), hl
                pop hl: ld (OFF_RASTR2_4+1), hl:    pop hl: ld (RASTR2_11+1), hl
                pop hl: ld (OFF_RASTR2_5+1), hl:    pop hl: ld (RASTR2_10+1), hl
                pop hl: ld (OFF_RASTR2_6+1), hl:    pop hl: ld (RASTR2_9+1), hl
                pop hl: ld (OFF_RASTR2_7+1), hl:    pop hl: ld (RASTR2_8+1), hl
                exx

                // Draw middle 3-th of rastr during top 3-th of colors

                inc h
                ld sp, hl

                pop hl: ld (OFF_RASTR2_8+1), hl:    pop hl: ld (RASTR2_7+1), hl
                pop hl: ld (OFF_RASTR2_9+1), hl:    pop hl: ld (RASTR2_6+1), hl
                pop hl: ld (OFF_RASTR2_10+1), hl:   pop hl: ld (RASTR2_5+1), hl
                pop hl: ld (OFF_RASTR2_11+1), hl:   pop hl: ld (RASTR2_4+1), hl
                pop hl: ld (OFF_RASTR2_12+1), hl:   pop hl: ld (RASTR2_3+1), hl
                pop hl: ld (OFF_RASTR2_13+1), hl:   pop hl: ld (RASTR2_2+1), hl
                pop hl: ld (OFF_RASTR2_14+1), hl:   pop hl: ld (RASTR2_1+1), hl
                pop hl: ld (OFF_RASTR2_15+1), hl:   pop hl: ld (RASTR2_0+1), hl

                // Draw top 3-th of rastr during bottom 3-th of colors
                ; shift to 63 for MC rastr instead of 64 to move on next frame
                ld hl, (63-8) * 4 + 2
                add hl, sp
                ld sp, hl

                                                     pop hl: ld (RASTR2_23+1), hl
                pop hl: ld (OFF_RASTR2_16+1), hl:    pop hl: ld (RASTR2_22+1), hl
                pop hl: ld (OFF_RASTR2_17+1), hl:    pop hl: ld (RASTR2_21+1), hl
                pop hl: ld (OFF_RASTR2_18+1), hl:    pop hl: ld (RASTR2_20+1), hl
                pop hl: ld (OFF_RASTR2_19+1), hl:    pop hl: ld (RASTR2_19+1), hl
                pop hl: ld (OFF_RASTR2_20+1), hl:    pop hl: ld (RASTR2_18+1), hl
                pop hl: ld (OFF_RASTR2_21+1), hl:    pop hl: ld (RASTR2_17+1), hl
                pop hl: ld (OFF_RASTR2_22+1), hl:    pop hl: ld (RASTR2_16+1), hl
                pop hl: ld (OFF_RASTR2_23+1), hl:    

                // -------------------------------- (odd) DRAW_RASTR_LINES -----------------------------------------

                ld a, c
                and 7
                set_page_by_bank
                ld l, a
                scf
                exx

                DRAW_OFFSCREEN_LINES2 23, 23
                DRAW_OFFSCREEN_LINES2 16, 23
                DRAW_OFFSCREEN_LINES2 15, 16
                DRAW_OFFSCREEN_LINES2 8, 15
                DRAW_OFFSCREEN_LINES2 7, 8
                DRAW_OFFSCREEN_LINES2 0, 7

                next_page

                DRAW_OFFSCREEN_LINES2 18, 0
                DRAW_OFFSCREEN_LINES2 17, 18
                DRAW_OFFSCREEN_LINES2 10, 17
                DRAW_OFFSCREEN_LINES2 9, 10
                DRAW_OFFSCREEN_LINES2 2, 9
                DRAW_OFFSCREEN_LINES2 1, 2

                next_page

                DRAW_OFFSCREEN_LINES2 20, 1
                DRAW_OFFSCREEN_LINES2 19, 20
                DRAW_OFFSCREEN_LINES2 12, 19
                DRAW_OFFSCREEN_LINES2 11, 12
                DRAW_OFFSCREEN_LINES2 4, 11
                DRAW_OFFSCREEN_LINES2 3, 4

                next_page

                DRAW_OFFSCREEN_LINES2 22, 3
                DRAW_OFFSCREEN_LINES2 21, 22
                DRAW_OFFSCREEN_LINES2 14, 21
                DRAW_OFFSCREEN_LINES2 13, 14
                DRAW_OFFSCREEN_LINES2 6, 13
                DRAW_OFFSCREEN_LINES2 5, 6

                exx
                ld a, l
                out (0xfd), a
bank_drawing_common:
        ; delay
        ld hl, timings_data
        add hl, bc
        add hl, bc
        ld sp, hl
        pop hl
        DO_DELAY

        // --------------------- update_jp_ix_table (first half) --------------------------------
        //MACRO update_jp_ix_table
                ; a = bank number

                ; de =  offset in bank (bc/8)
                ld a, 0xc0
                and c
                ld d, b
                rr d: rra
                rr d: rra
                rr d: rra
                ld e, a

                ld a, 0x3f
                and c
                rla                             ; 4

                add low(JPIX__BANKS_HELPER)     ; 7
                ld l, a                         ; 4
                ld h, high(JPIX__BANKS_HELPER)  ; 7
                ld sp, hl                       ; 6
                pop hl  ; hl = bank offset      ; 10
                ; ticks: 38

                ; hl - pointer to new record in JPIX table
                add hl, de

                ; prepare  N-th element for jpix_table_ref in a

                // 1. write new JP_IX
                ld sp, hl
                exx
                ld de, JP_IX_CODE
                .2 write_jp_ix_data 1
                LD (UPDATE_JPIX_WRITE), sp

                // 2. restore data
                ; hl = jpix_table_ref
                ld h, high(JPIX__REF_TABLE_START)       ; 7
                and 0x0e                                ; 7
                ld l, a                                 ; 4
                ld sp, hl                               ; 6
                ; put new pointer to jpix_table_ref, get old pointer to restore data in hl
                exx                                     ; 4
                ex (sp), hl                             ; 19
                ld sp, hl                               ; 6
                .3 restore_jp_ix
                LD (UPDATE_JPIX_RESTORE), sp
                // total 698
        // ------------------------- update jpix table end

        // calculate address of the MC descriptors
        // Draw from 0 to 23 is backward direction
        ld hl, mc_descriptors + 23*2

        // calculate floor(bc,8) / 4
        ld a, ~6
        and c
        srl b : rra

        jp c, odd_mc_drawing

        srl b : rra
        ld c, a

        // prepare in HL' multicolor address to execute
        add hl, bc
        ld sp, hl
        pop hl

        ; timing here on first frame: 91153
        scf     // aligned data uses ret nc. prevent these ret
        DRAW_MULTICOLOR_AND_RASTR_LINE 0, 0
        DRAW_MULTICOLOR_AND_RASTR_LINE 1, 1
        DRAW_MULTICOLOR_AND_RASTR_LINE 2, 2
        DRAW_MULTICOLOR_AND_RASTR_LINE 3, 3
        DRAW_MULTICOLOR_AND_RASTR_LINE 4, 4
        DRAW_MULTICOLOR_AND_RASTR_LINE 5, 5
        DRAW_MULTICOLOR_AND_RASTR_LINE 6, 6
        DRAW_MULTICOLOR_AND_RASTR_LINE 7, 7
        
        DRAW_MULTICOLOR_AND_RASTR_LINE 8,  0
        DRAW_MULTICOLOR_AND_RASTR_LINE 9,  1
        DRAW_MULTICOLOR_AND_RASTR_LINE 10, 2
        DRAW_MULTICOLOR_AND_RASTR_LINE 11, 3
        DRAW_MULTICOLOR_AND_RASTR_LINE 12, 4
        DRAW_MULTICOLOR_AND_RASTR_LINE 13, 5
        DRAW_MULTICOLOR_AND_RASTR_LINE 14, 6
        DRAW_MULTICOLOR_AND_RASTR_LINE 15, 7

        DRAW_MULTICOLOR_AND_RASTR_LINE 16, 1
        DRAW_MULTICOLOR_AND_RASTR_LINE 17, 2
        DRAW_MULTICOLOR_AND_RASTR_LINE 18, 3
        DRAW_MULTICOLOR_AND_RASTR_LINE 19, 4
        DRAW_MULTICOLOR_AND_RASTR_LINE 20, 5
        DRAW_MULTICOLOR_AND_RASTR_LINE 21, 6
        DRAW_MULTICOLOR_AND_RASTR_LINE 22, 7
        DRAW_MULTICOLOR_AND_RASTR_LINE 23, 0

        ld bc, (stack_bottom)
        dec bc
        ; compare to -1
        ld l, b
        inc l

        jp z, lower_limit_reached      ; 10 ticks
        jp loop                        ; 12 ticks

filler2  defs 12, 0   // align code data

odd_mc_drawing        
        srl b : rra
        ld c, a
       
        // prepare in HL' multicolor address to execute
        add hl, bc
        ld sp, hl
        pop hl

        ; timing here on first frame: 91153
        scf     // aligned data uses ret nc. prevent these ret
        DRAW_MULTICOLOR_AND_RASTR_LINE2 0
        DRAW_MULTICOLOR_AND_RASTR_LINE2 1
        DRAW_MULTICOLOR_AND_RASTR_LINE2 2
        DRAW_MULTICOLOR_AND_RASTR_LINE2 3
        DRAW_MULTICOLOR_AND_RASTR_LINE2 4
        DRAW_MULTICOLOR_AND_RASTR_LINE2 5
        DRAW_MULTICOLOR_AND_RASTR_LINE2 6
        DRAW_MULTICOLOR_AND_RASTR_LINE2 7
        
        DRAW_MULTICOLOR_AND_RASTR_LINE2 8
        DRAW_MULTICOLOR_AND_RASTR_LINE2 9
        DRAW_MULTICOLOR_AND_RASTR_LINE2 10
        DRAW_MULTICOLOR_AND_RASTR_LINE2 11
        DRAW_MULTICOLOR_AND_RASTR_LINE2 12
        DRAW_MULTICOLOR_AND_RASTR_LINE2 13
        DRAW_MULTICOLOR_AND_RASTR_LINE2 14
        DRAW_MULTICOLOR_AND_RASTR_LINE2 15

        DRAW_MULTICOLOR_AND_RASTR_LINE2 16
        DRAW_MULTICOLOR_AND_RASTR_LINE2 17
        DRAW_MULTICOLOR_AND_RASTR_LINE2 18
        DRAW_MULTICOLOR_AND_RASTR_LINE2 19
        DRAW_MULTICOLOR_AND_RASTR_LINE2 20
        DRAW_MULTICOLOR_AND_RASTR_LINE2 21
        DRAW_MULTICOLOR_AND_RASTR_LINE2 22
        DRAW_MULTICOLOR_AND_RASTR_LINE2 23

        ld bc, (stack_bottom)
        dec bc
        jp loop                        ; 12 ticks


/*********************** routines *************/

create_page_helper
        ld hl, SET_PAGE_HELPER
        ld (hl), 0x51   ; 0-th
        inc l
        ld (hl), 0x53   ; 1-th
        inc l
        ld (hl), 0x53   ; 2-th, just filler
        inc l
        ld (hl), 0x54   ; 3-th
        inc l
        ld (hl), 0x50   ; 4-th
        inc l
        ret

JP_IX_CODE      equ #e9dd

jp_ix_record_size       equ 8
data_page_count         equ 4
jp_ix_bank_size         equ (imageHeight/64 + 2) * jp_ix_record_size
jp_first_bank_offset    equ jp_ix_record_size * 2

write_initial_jp_ix_table
                // create banks helper (to avoid mul in runtime)
                and a
                ld b, 64
                ld hl, jpix_table + jp_ix_bank_size * 15 + jp_first_bank_offset
                ld de, jp_ix_bank_size
                ld sp, JPIX__BANKS_HELPER_END

                ld c, 64 / 8
.helper:                
                ld b, 4
.helper_page:   push hl
                sbc hl, de
                push hl
                add hl, de
                djnz .helper_page

                sbc hl, de
                sbc hl, de
                dec c
                jr nz, .helper

                // main data
                ld a, 0
page_loop:
                ld c, a
                and a
                set_page_by_bank
                ld a, c

                ld sp, jpix_table + jp_first_bank_offset
                ld c, 2
.loop:          ld b, 6                         ; write 0, 64, 128-th descriptors for start line, 6 descriptors per shift
                
                ; fill JPIX_REF_TABLE
                ; read sp to de                
                ld hl, 0
                add hl, sp
                ex de, hl

                rla
                ld h, high(JPIX__REF_TABLE_START)
                ld l, a
                rra
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

                ; skip several descriptors
                ld hl,  jp_ix_bank_size - 3  * jp_ix_record_size
                add hl, sp
                ld sp, hl
                inc a
                dec c
                jr nz, .loop
          
                cp 8
                jr nz, page_loop

                ld sp, stack_top - 2
                ret

/*
write_initial_jp_ix_table
        ld a, 0
page_loop:
        bit 0, a
        jr nz, continue_page        
        ld sp, jpix_table
continue_page:
        ld b, 5
        ld c, a
        
        ; fill JPIX_REF_TABLE
        ld hl, 0
        add hl, sp
        ex de, hl

        ld h, high(JPIX__REF_TABLE_START)
        rla
        ld l, a
        rra
        ld (hl), e
        inc l
        ld (hl), d

        and a
        set_page_by_bank

        ld de, JP_IX_CODE
.rep:   pop hl ; address
        ld (hl), e
        inc hl
        ld (hl), d
        pop hl ; skip restore data
        djnz .rep
        pop hl
        // write to the prev page
        ld a, c
        dec a
        and 7
        set_page_by_bank
        ld (hl), e
        inc hl
        ld (hl), d
        pop hl ; skip restore data

        ld a, c
        inc a
        cp 8
        jr nz, page_loop

        ld sp, stack_top - 2
        ret
*/

/*
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
*/        

static_delay
D0              EQU 17 + 10 + 10        ; call, initial LD, ret
D1              EQU     6 + 4 + 4 + 12     ; 26
DELAY_STEPS     EQU (sync_tick-D0) / D1
TICKS_REST      EQU  (sync_tick-D0) - DELAY_STEPS * D1 + 5
                LD hl, DELAY_STEPS      ; 10
.rep            dec hl                  ; 6
                ld a, h                 ; 4 
                or l                    ; 4
                jr nz, .rep             ; 7/12
                ASSERT(TICKS_REST >= 5 && TICKS_REST <= 30)
t1              EQU TICKS_REST                
                IF (t1 >= 26)
                        add hl, hl
t2                      EQU t1 - 11                        
                ELSE
t2                      EQU t1
                ENDIF

                IF (t2 >= 15)
                        add hl, hl
t3                      EQU t2 - 11
                ELSE
t3                      EQU t2
                ENDIF

                IF (t3 >= 10)
                        inc hl
t4                      EQU t3 - 6                        
                ELSE
t4                      EQU t3
                ENDIF

                IF (t4 == 9)
                        ret c
                        nop
                ELSEIF (t4 == 8)
                        ld hl, hl
                ELSEIF (t4 == 7)
                        ld l, (hl)
                ELSEIF (t4 == 6)
                        inc hl
                ELSEIF (t4 == 5)
                        ret c
                ELSEIF (t4 == 4)
                        nop
                ELSE
                        ASSERT 0
                ENDIF
        ret


/*************** Image data. ******************/
	IF (DEBUG_MODE == 1)
        	ORG 28000
	ENDIF	
        ASSERT $ <= 26700
        ORG 26700
generated_code:
        INCBIN "resources/compressed_data.mt_and_rt_reach.descriptor"
multicolor_code
        INCBIN "resources/compressed_data.multicolor"

mc_descriptors
        INCBIN "resources/compressed_data.mc_descriptors"

timings_data
        INCBIN "resources/compressed_data.timings"
timings_data_end

jpix_table EQU 0xc000

        ORG 0xc000
        PAGE 0
        INCBIN "resources/compressed_data.jpix0"
        INCBIN "resources/compressed_data.main0"

        ORG 0xc000
        PAGE 1
        INCBIN "resources/compressed_data.jpix1"
        INCBIN "resources/compressed_data.main1"

        ORG 0xc000
        PAGE 3
        INCBIN "resources/compressed_data.jpix2"
        INCBIN "resources/compressed_data.main2"

        ORG 0xc000
        PAGE 4
        INCBIN "resources/compressed_data.jpix3"
        INCBIN "resources/compressed_data.main3"

        ORG 0xc000 + 0x1b00
        PAGE 7
color_code
        INCBIN "resources/compressed_data.color"
color_descriptor
        INCBIN "resources/compressed_data.color_descriptor"
rastr_descriptors
        INCBIN "resources/compressed_data.rastr.descriptors"

/*
src_data
        INCBIN "resources/samanasuke.bin", 0, 6144
color_data:
        INCBIN "resources/samanasuke.bin", 6144, 768
*/        
data_end:

imageHeight     equ (timings_data_end - timings_data) / 2

/*************** Ennd image data. ******************/


/*************** Commands to SJ asm ******************/

    SAVESNA "build/draw_image.sna", main
    savetap "build/draw_image.tap", main
