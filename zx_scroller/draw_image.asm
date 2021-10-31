        DEVICE zxspectrum128
        SLDOPT COMMENT WPMEM, LOGPOINT, ASSERTION //< More debug symbols.

/*
 * Page locations:
 * 0,1, 3,4 - rastrData + JP_IX_TABLE
 * 2 - static page 0x8000
 * 5 - static page 0x4000
 * Pages 2,5 include: reach descriptors, MC data
 * 7 - music data
 * 6 - second screen, second screen code, other descriptors
 * page 6: color data from address #1b00
 * short port page selection:
 *    LD A, #50 + screen_num*8 + page_number
 *    OUT (#fd), A
 **/


screen_addr:    equ 16384
color_addr:     equ 5800h
screen_end:     equ 5b00h
start:          equ 5e00h


draw_offrastr_offset    equ screen_end

STACK_SIZE:             equ 4  ; in words
stack_bottom            equ screen_end + 16
stack_top               equ stack_bottom + STACK_SIZE * 2
color_data_to_restore   equ stack_top
//saved_bc                equ color_data_to_restore + 2

SET_PAGE_HELPER         EQU screen_end + 0x50
SET_PAGE_HELPER_END     EQU SET_PAGE_HELPER + 8

DEBUG_MODE              EQU 0

        INCLUDE "resources/compressed_data.asm"

    org 16384
    INCBIN "resources/jorg10.scr", 0, 6144+768

        MACRO FILL_OFF_RASTR_64_0

        pop hl: FILL_OFF_JP 0, 0
        pop hl: FILL_OFF_SP 0, 0

        pop hl: FILL_OFF_JP 0, 1: FILL_OFF_JP 1, 0
        pop hl: FILL_OFF_SP 0, 1: CONT_OFF_SP 1, 0

        pop hl: FILL_OFF_JP 0, 2: FILL_OFF_JP 1, 1: FILL_OFF_JP 2, 0
        pop hl: FILL_OFF_SP 0, 2: CONT_OFF_SP 1, 1: CONT_OFF_SP 2, 0

        pop hl: FILL_OFF_JP 0, 3: FILL_OFF_JP 1, 2: FILL_OFF_JP 2, 1: FILL_OFF_JP 3, 0
        pop hl: FILL_OFF_SP 0, 3: CONT_OFF_SP 1, 2: CONT_OFF_SP 2, 1: CONT_OFF_SP 3, 0

        pop hl: FILL_OFF_JP 0, 4: FILL_OFF_JP 1, 3: FILL_OFF_JP 2, 2: FILL_OFF_JP 3, 1: FILL_OFF_JP 4, 0
        pop hl: FILL_OFF_SP 0, 4: CONT_OFF_SP 1, 3: CONT_OFF_SP 2, 2: CONT_OFF_SP 3, 1: CONT_OFF_SP 4, 0

        pop hl: FILL_OFF_JP 0, 5: FILL_OFF_JP 1, 4: FILL_OFF_JP 2, 3: FILL_OFF_JP 3, 2: FILL_OFF_JP 4, 1: FILL_OFF_JP 5, 0
        pop hl: FILL_OFF_SP 0, 5: CONT_OFF_SP 1, 4: CONT_OFF_SP 2, 3: CONT_OFF_SP 3, 2: CONT_OFF_SP 4, 1: CONT_OFF_SP 5, 0

        pop hl: FILL_OFF_JP 0, 6: FILL_OFF_JP 1, 5: FILL_OFF_JP 2, 4: FILL_OFF_JP 3, 3: FILL_OFF_JP 4, 2: FILL_OFF_JP 5, 1: FILL_OFF_JP 6, 0
        pop hl: FILL_OFF_SP 0, 6: CONT_OFF_SP 1, 5: CONT_OFF_SP 2, 4: CONT_OFF_SP 3, 3: CONT_OFF_SP 4, 2: CONT_OFF_SP 5, 1: CONT_OFF_SP 6, 0

        pop hl: FILL_OFF_JP 0, 7: FILL_OFF_JP 1, 6: FILL_OFF_JP 2, 5: FILL_OFF_JP 3, 4: FILL_OFF_JP 4, 3: FILL_OFF_JP 5, 2: FILL_OFF_JP 6, 1: FILL_OFF_JP 7, 0
        pop hl: FILL_OFF_SP 0, 7: CONT_OFF_SP 1, 6: CONT_OFF_SP 2, 5: CONT_OFF_SP 3, 4: CONT_OFF_SP 4, 3: CONT_OFF_SP 5, 2: CONT_OFF_SP 6, 1: CONT_OFF_SP 7, 0

        pop hl:                   FILL_OFF_JP 1, 7: FILL_OFF_JP 2, 6: FILL_OFF_JP 3, 5: FILL_OFF_JP 4, 4: FILL_OFF_JP 5, 3: FILL_OFF_JP 6, 2: FILL_OFF_JP 7, 1
        pop hl:                   FILL_OFF_SP 1, 7: CONT_OFF_SP 2, 6: CONT_OFF_SP 3, 5: CONT_OFF_SP 4, 4: CONT_OFF_SP 5, 3: CONT_OFF_SP 6, 2: CONT_OFF_SP 7, 1

        pop hl:                                     FILL_OFF_JP 2, 7: FILL_OFF_JP 3, 6: FILL_OFF_JP 4, 5: FILL_OFF_JP 5, 4: FILL_OFF_JP 6, 3: FILL_OFF_JP 7, 2
        pop hl:                                     FILL_OFF_SP 2, 7: CONT_OFF_SP 3, 6: CONT_OFF_SP 4, 5: CONT_OFF_SP 5, 4: CONT_OFF_SP 6, 3: CONT_OFF_SP 7, 2

        pop hl:                                                       FILL_OFF_JP 3, 7: FILL_OFF_JP 4, 6: FILL_OFF_JP 5, 5: FILL_OFF_JP 6, 4: FILL_OFF_JP 7, 3
        pop hl:                                                       FILL_OFF_SP 3, 7: CONT_OFF_SP 4, 6: CONT_OFF_SP 5, 5: CONT_OFF_SP 6, 4: CONT_OFF_SP 7, 3

        pop hl:                                                                         FILL_OFF_JP 4, 7: FILL_OFF_JP 5, 6: FILL_OFF_JP 6, 5: FILL_OFF_JP 7, 4
        pop hl:                                                                         FILL_OFF_SP 4, 7: CONT_OFF_SP 5, 6: CONT_OFF_SP 6, 5: CONT_OFF_SP 7, 4

        pop hl:                                                                                           FILL_OFF_JP 5, 7: FILL_OFF_JP 6, 6: FILL_OFF_JP 7, 5
        pop hl:                                                                                           FILL_OFF_SP 5, 7: CONT_OFF_SP 6, 6: CONT_OFF_SP 7, 5

        pop hl:                                                                                                             FILL_OFF_JP 6, 7: FILL_OFF_JP 7, 6
        pop hl:                                                                                                             FILL_OFF_SP 6, 7: CONT_OFF_SP 7, 6

        pop hl:                                                                                                                               FILL_OFF_JP 7, 7
        pop hl:                                                                                                                               FILL_OFF_SP 7, 7


        ENDM

        MACRO FILL_OFF_RASTR_64_8

        pop hl: FILL_OFF_JP 0, 8
        pop hl: FILL_OFF_SP 0, 8

        pop hl: FILL_OFF_JP 0, 9: FILL_OFF_JP 1, 8
        pop hl: FILL_OFF_SP 0, 9: CONT_OFF_SP 1, 8

        pop hl: FILL_OFF_JP 0, 10: FILL_OFF_JP 1, 9: FILL_OFF_JP 2, 8
        pop hl: FILL_OFF_SP 0, 10: CONT_OFF_SP 1, 9: CONT_OFF_SP 2, 8

        pop hl: FILL_OFF_JP 0, 11: FILL_OFF_JP 1, 10: FILL_OFF_JP 2, 9: FILL_OFF_JP 3, 8
        pop hl: FILL_OFF_SP 0, 11: CONT_OFF_SP 1, 10: CONT_OFF_SP 2, 9: CONT_OFF_SP 3, 8

        pop hl: FILL_OFF_JP 0, 12: FILL_OFF_JP 1, 11: FILL_OFF_JP 2, 10: FILL_OFF_JP 3, 9: FILL_OFF_JP 4, 8
        pop hl: FILL_OFF_SP 0, 12: CONT_OFF_SP 1, 11: CONT_OFF_SP 2, 10: CONT_OFF_SP 3, 9: CONT_OFF_SP 4, 8

        pop hl: FILL_OFF_JP 0, 13: FILL_OFF_JP 1, 12: FILL_OFF_JP 2, 11: FILL_OFF_JP 3, 10: FILL_OFF_JP 4, 9: FILL_OFF_JP 5, 8
        pop hl: FILL_OFF_SP 0, 13: CONT_OFF_SP 1, 12: CONT_OFF_SP 2, 11: CONT_OFF_SP 3, 10: CONT_OFF_SP 4, 9: CONT_OFF_SP 5, 8

        pop hl: FILL_OFF_JP 0, 14: FILL_OFF_JP 1, 13: FILL_OFF_JP 2, 12: FILL_OFF_JP 3, 11: FILL_OFF_JP 4, 10: FILL_OFF_JP 5, 9: FILL_OFF_JP 6, 8
        pop hl: FILL_OFF_SP 0, 14: CONT_OFF_SP 1, 13: CONT_OFF_SP 2, 12: CONT_OFF_SP 3, 11: CONT_OFF_SP 4, 10: CONT_OFF_SP 5, 9: CONT_OFF_SP 6, 8

        pop hl: FILL_OFF_JP 0, 15: FILL_OFF_JP 1, 14: FILL_OFF_JP 2, 13: FILL_OFF_JP 3, 12: FILL_OFF_JP 4, 11: FILL_OFF_JP 5, 10: FILL_OFF_JP 6, 9: FILL_OFF_JP 7, 8
        pop hl: FILL_OFF_SP 0, 15: CONT_OFF_SP 1, 14: CONT_OFF_SP 2, 13: CONT_OFF_SP 3, 12: CONT_OFF_SP 4, 11: CONT_OFF_SP 5, 10: CONT_OFF_SP 6, 9: CONT_OFF_SP 7, 8

        pop hl:                    FILL_OFF_JP 1, 15: FILL_OFF_JP 2, 14: FILL_OFF_JP 3, 13: FILL_OFF_JP 4, 12: FILL_OFF_JP 5, 11: FILL_OFF_JP 6, 10: FILL_OFF_JP 7, 9
        pop hl:                    FILL_OFF_SP 1, 15: CONT_OFF_SP 2, 14: CONT_OFF_SP 3, 13: CONT_OFF_SP 4, 12: CONT_OFF_SP 5, 11: CONT_OFF_SP 6, 10: CONT_OFF_SP 7, 9

        pop hl:                                       FILL_OFF_JP 2, 15: FILL_OFF_JP 3, 14: FILL_OFF_JP 4, 13: FILL_OFF_JP 5, 12: FILL_OFF_JP 6, 11: FILL_OFF_JP 7, 10
        pop hl:                                       FILL_OFF_SP 2, 15: CONT_OFF_SP 3, 14: CONT_OFF_SP 4, 13: CONT_OFF_SP 5, 12: CONT_OFF_SP 6, 11: CONT_OFF_SP 7, 10

        pop hl:                                                          FILL_OFF_JP 3, 15: FILL_OFF_JP 4, 14: FILL_OFF_JP 5, 13: FILL_OFF_JP 6, 12: FILL_OFF_JP 7, 11
        pop hl:                                                          FILL_OFF_SP 3, 15: CONT_OFF_SP 4, 14: CONT_OFF_SP 5, 13: CONT_OFF_SP 6, 12: CONT_OFF_SP 7, 11

        pop hl:                                                                             FILL_OFF_JP 4, 15: FILL_OFF_JP 5, 14: FILL_OFF_JP 6, 13: FILL_OFF_JP 7, 12
        pop hl:                                                                             FILL_OFF_SP 4, 15: CONT_OFF_SP 5, 14: CONT_OFF_SP 6, 13: CONT_OFF_SP 7, 12

        pop hl:                                                                                                FILL_OFF_JP 5, 15: FILL_OFF_JP 6, 14: FILL_OFF_JP 7, 13
        pop hl:                                                                                                FILL_OFF_SP 5, 15: CONT_OFF_SP 6, 14: CONT_OFF_SP 7, 13

        pop hl:                                                                                                                   FILL_OFF_JP 6, 15: FILL_OFF_JP 7, 14
        pop hl:                                                                                                                   FILL_OFF_SP 6, 15: CONT_OFF_SP 7, 14

        pop hl:                                                                                                                                      FILL_OFF_JP 7, 15
        pop hl:                                                                                                                                      FILL_OFF_SP 7, 15

        ENDM

        MACRO FILL_OFF_RASTR_64_16

        pop hl: FILL_OFF_JP 0, 16
        pop hl: FILL_OFF_SP 0, 16

        pop hl: FILL_OFF_JP 0, 17: FILL_OFF_JP 1, 16
        pop hl: FILL_OFF_SP 0, 17: CONT_OFF_SP 1, 16

        pop hl: FILL_OFF_JP 0, 18: FILL_OFF_JP 1, 17: FILL_OFF_JP 2, 16
        pop hl: FILL_OFF_SP 0, 18: CONT_OFF_SP 1, 17: CONT_OFF_SP 2, 16

        pop hl: FILL_OFF_JP 0, 19: FILL_OFF_JP 1, 18: FILL_OFF_JP 2, 17: FILL_OFF_JP 3, 16
        pop hl: FILL_OFF_SP 0, 19: CONT_OFF_SP 1, 18: CONT_OFF_SP 2, 17: CONT_OFF_SP 3, 16

        pop hl: FILL_OFF_JP 0, 20: FILL_OFF_JP 1, 19: FILL_OFF_JP 2, 18: FILL_OFF_JP 3, 17: FILL_OFF_JP 4, 16
        pop hl: FILL_OFF_SP 0, 20: CONT_OFF_SP 1, 19: CONT_OFF_SP 2, 18: CONT_OFF_SP 3, 17: CONT_OFF_SP 4, 16

        pop hl: FILL_OFF_JP 0, 21: FILL_OFF_JP 1, 20: FILL_OFF_JP 2, 19: FILL_OFF_JP 3, 18: FILL_OFF_JP 4, 17: FILL_OFF_JP 5, 16
        pop hl: FILL_OFF_SP 0, 21: CONT_OFF_SP 1, 20: CONT_OFF_SP 2, 19: CONT_OFF_SP 3, 18: CONT_OFF_SP 4, 17: CONT_OFF_SP 5, 16

        pop hl: FILL_OFF_JP 0, 22: FILL_OFF_JP 1, 21: FILL_OFF_JP 2, 20: FILL_OFF_JP 3, 19: FILL_OFF_JP 4, 18: FILL_OFF_JP 5, 17: FILL_OFF_JP 6, 16
        pop hl: FILL_OFF_SP 0, 22: CONT_OFF_SP 1, 21: CONT_OFF_SP 2, 20: CONT_OFF_SP 3, 19: CONT_OFF_SP 4, 18: CONT_OFF_SP 5, 17: CONT_OFF_SP 6, 16

        pop hl: FILL_OFF_JP 0, 23: FILL_OFF_JP 1, 22: FILL_OFF_JP 2, 21: FILL_OFF_JP 3, 20: FILL_OFF_JP 4, 19: FILL_OFF_JP 5, 18: FILL_OFF_JP 6, 17: FILL_OFF_JP 7, 16
        pop hl: FILL_OFF_SP 0, 23: CONT_OFF_SP 1, 22: CONT_OFF_SP 2, 21: CONT_OFF_SP 3, 20: CONT_OFF_SP 4, 19: CONT_OFF_SP 5, 18: CONT_OFF_SP 6, 17: CONT_OFF_SP 7, 16

        pop hl:                    FILL_OFF_JP 1, 23: FILL_OFF_JP 2, 22: FILL_OFF_JP 3, 21: FILL_OFF_JP 4, 20: FILL_OFF_JP 5, 19: FILL_OFF_JP 6, 18: FILL_OFF_JP 7, 17
        pop hl:                    FILL_OFF_SP 1, 23: CONT_OFF_SP 2, 22: CONT_OFF_SP 3, 21: CONT_OFF_SP 4, 20: CONT_OFF_SP 5, 19: CONT_OFF_SP 6, 18: CONT_OFF_SP 7, 17

        pop hl:                                       FILL_OFF_JP 2, 23: FILL_OFF_JP 3, 22: FILL_OFF_JP 4, 21: FILL_OFF_JP 5, 20: FILL_OFF_JP 6, 19: FILL_OFF_JP 7, 18
        pop hl:                                       FILL_OFF_SP 2, 23: CONT_OFF_SP 3, 22: CONT_OFF_SP 4, 21: CONT_OFF_SP 5, 20: CONT_OFF_SP 6, 19: CONT_OFF_SP 7, 18

        pop hl:                                                          FILL_OFF_JP 3, 23: FILL_OFF_JP 4, 22: FILL_OFF_JP 5, 21: FILL_OFF_JP 6, 20: FILL_OFF_JP 7, 19
        pop hl:                                                          FILL_OFF_SP 3, 23: CONT_OFF_SP 4, 22: CONT_OFF_SP 5, 21: CONT_OFF_SP 6, 20: CONT_OFF_SP 7, 19

        pop hl:                                                                             FILL_OFF_JP 4, 23: FILL_OFF_JP 5, 22: FILL_OFF_JP 6, 21: FILL_OFF_JP 7, 20
        pop hl:                                                                             FILL_OFF_SP 4, 23: CONT_OFF_SP 5, 22: CONT_OFF_SP 6, 21: CONT_OFF_SP 7, 20

        pop hl:                                                                                                FILL_OFF_JP 5, 23: FILL_OFF_JP 6, 22: FILL_OFF_JP 7, 21
        pop hl:                                                                                                FILL_OFF_SP 5, 23: CONT_OFF_SP 6, 22: CONT_OFF_SP 7, 21

        pop hl:                                                                                                                   FILL_OFF_JP 6, 23: FILL_OFF_JP 7, 22
        pop hl:                                                                                                                   FILL_OFF_SP 6, 23: CONT_OFF_SP 7, 22

        pop hl:                                                                                                                                      FILL_OFF_JP 7, 23
        pop hl:                                                                                                                                      FILL_OFF_SP 7, 23

        ENDM


    org start

        MACRO update_colors_jpix
        //MACRO draw_colors
        ; bc - line number

        ld hl, bc
        rr hl

        ld de, color_descriptor                         ; 10
        add hl, de                                      ; 11
        ld sp, hl                                       ; 6
        ; end of exec address
        pop de                                          ; 10
        ;  address to execute
        pop hl                                          ; 14
        ld (start_draw_colors+1), hl
        ld (start_draw_colors0+1), hl

        // restore data from prev MC drawing steps
        pop hl
        ld sp, hl
        inc sp
        inc sp
        ld hl, (color_data_to_restore)
        push hl

        ex de, hl

        ; write JP_IX to end exec address, 
        ; store original data to HL
        ; store end exec address to de
        ld sp, hl                                       ; 6
        ld de, JP_VIA_HL_CODE                           ; 10
        ex de, hl                                       ; 4
        ex (sp), hl                                     ; 19
        ld (color_data_to_restore), hl
        ENDM

        MACRO FILL_OFF_JP Step?, Iteration?
                ld (OFF_Step?_Iteration?_JP + 1), hl
        ENDM
        
        MACRO FILL_OFF_SP Step?, Iteration?
                ld de, color_addr
                add hl, de
                ld (OFF_Step?_Iteration?_SP + 1), hl
        ENDM

        MACRO CONT_OFF_SP Step?, Iteration?
                dec h
                ld (OFF_Step?_Iteration?_SP + 1), hl
        ENDM

        MACRO restore_jp_ix
                pop hl ; address
                pop de ; restore data
                ld (hl), e
                inc hl
                ld (hl), d
        ENDM
        MACRO write_jp_ix_data_via_de
                pop hl ; address
                ld (hl), e
                inc hl
                ld (hl), d
        ENDM
        MACRO write_jp_ix_data_via_bc
                pop hl ; address
                ld (hl), c
                inc hl
                ld (hl), b
        ENDM

jpix_bank_size          EQU (imageHeight/64 + 2) * jp_ix_record_size

    MACRO DRAW_MULTICOLOR_LINE N?:
                ld sp, color_addr + N? * 32 + 16        ; 10
MC_LINE_N?      jp 00                                   ; 10
                // total ticks: 20 (30 with ret)
    ENDM                

    MACRO DRAW_RASTR_LINE N?:
                ld sp, screen_addr + ((N? + 8) % 24) * 256 + 256       ; 10
                ld hl, $ + 7                                           ; 10
                exx                                                    ; 4
RASTR_N?        jp 00 ; rastr for multicolor ( up to 8 lines)          ; 10        
    ENDM                

    MACRO DRAW_MULTICOLOR_AND_RASTR_LINE N?:
                DRAW_MULTICOLOR_LINE  N?
                DRAW_RASTR_LINE N?
    ENDM                

        MACRO PREPARE_MC_DRAWING N?
                pop hl                          ; begin addr
                ld (MC_LINE_N? + 1), hl

                pop hl                          ; JP XX command+1 address
                ld (hl), low(MC_LINE_N? + 3)
                inc hl
                ld (hl), high(MC_LINE_N? + 3)

                ; e- first stack moving value, d - 2-nd stack moving value
                ; The value is relative from the line end
                ld hl, color_addr + N? * 32
                pop de                          
                ld d, 0x00 ; //< Currently unused. Reserved for the future use (3 part MC drawing)
                add hl, de
                ex hl, de

                // LD SP, XX first byte addr
                pop hl                          
                ld (hl), e
                inc hl
                ld (hl), d

        ENDM               

        MACRO SET_PAGE page_number
                LD A, #50 + page_number
                OUT (#fd), A
        ENDM

        MACRO set_page_by_logical_num
                ; a - bank number
                cp 2
                ccf
                adc 0x50
                out (0xfd), a
        ENDM

        MACRO set_page_by_bank
                ; a - bank number
                rra
                set_page_by_logical_num
        ENDM

/************* Routines **********************/

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

create_write_off_rastr_helper
        ld hl, draw_off_rastr_0
        ld (draw_offrastr_offset), hl
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
        call create_write_off_rastr_helper
        call write_initial_jp_ix_table

mc_preambula_delay      equ 46
fixed_startup_delay     equ 42554 - 8
initial_delay           equ first_timing_in_interrupt + fixed_startup_delay +  mc_preambula_delay
sync_tick               equ screen_ticks + screen_start_tick  - initial_delay +  FIRST_LINE_DELAY
        assert (sync_tick <= 65535 && sync_tick >= 4)
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

        ld iy, 0h                       ; 14  ticks
        ld bc, 0h                       ; 10  ticks
        jp loop1
lower_limit_reached:
        ld iy,  max_scroll_offset       ; 14 ticks
loop:  
        ld bc, iy
jp_ix_line_delta_in_bank EQU 2 * 6*4
        // --------------------- update_jp_ix_table --------------------------------

        ld a, 7
        and c
        set_page_by_bank

        // ------------------------- update jpix table
        // bc - screen address to draw
        // between frames: 73248/71456
        ; a = bank number
        // set bits that match page number to 0
        ld a, ~6                        ; 7
        and c                           ; 4
        ld h, b                         ; 4
        rr h                            ; 8
        rra                             ; 4
        jr nc, no                       ; 7/12
        add 2                           ; 7
no:
        ld l, a                         ; 4
        ld de,  update_jpix_helper      ; 10
        add hl, de                      ; 11
        
        ld sp, hl
        pop hl
        ld sp, hl

        exx
        ld bc, JP_VIA_HL_CODE
        .2 restore_jp_ix
        .2 write_jp_ix_data_via_bc
        .2 restore_jp_ix
        .2 write_jp_ix_data_via_bc
        .2 restore_jp_ix
        .2 write_jp_ix_data_via_bc
        // total: 562/560 (with switching page)
        exx     ; go main regs
        // ------------------------- update jpix table end
        scf
        DRAW_RASTR_LINE 23

loop1:
        SET_PAGE 6

        ld a, 7
        and iyl
        jp nz, mc_step_drawing

        /************************* no-mc step drawing *********************************************/
        
        // calculate address of the MC descriptors
        // Draw from 0 to 23 is backward direction
        ld hl, mc_descriptors

        // prepare  multicolor drawing (for next 7 mc steps)
        // calculate bc/8 * 8
        add hl, bc
        ld sp, hl

        //pop hl: ld (MC_LINE_23 + 5), hl: ld (MC_LINE2_23 + 5), hl
        PREPARE_MC_DRAWING 23
        PREPARE_MC_DRAWING 22
        PREPARE_MC_DRAWING 21
        PREPARE_MC_DRAWING 20
        PREPARE_MC_DRAWING 19
        PREPARE_MC_DRAWING 18
        PREPARE_MC_DRAWING 17
        PREPARE_MC_DRAWING 16
        PREPARE_MC_DRAWING 15
        PREPARE_MC_DRAWING 14
        PREPARE_MC_DRAWING 13
        PREPARE_MC_DRAWING 12
        PREPARE_MC_DRAWING 11
        PREPARE_MC_DRAWING 10
        PREPARE_MC_DRAWING 9
        PREPARE_MC_DRAWING 8
        PREPARE_MC_DRAWING 7
        PREPARE_MC_DRAWING 6
        PREPARE_MC_DRAWING 5
        PREPARE_MC_DRAWING 4
        PREPARE_MC_DRAWING 3
        PREPARE_MC_DRAWING 2
        PREPARE_MC_DRAWING 1
        PREPARE_MC_DRAWING 0

        update_colors_jpix        
        ld sp, color_addr + 768                         ; 10
        ld hl, $ + 7                                    ; 10
        exx                                             ; 4
start_draw_colors0:
        jp 00                                           ; 8
        //MACRO prepare_rastr_drawing (for the current step)

        sla c : rl b    // bc*2
        ld hl, off_rastr_descriptors
        add hl,bc       // * 2
        add hl,bc       // * 4
        ld sp, hl

        // arguments: step, iteration in step
        FILL_OFF_RASTR_64_0

        ld hl, (64-15) * 4
        add hl, sp
        ld sp, hl
        FILL_OFF_RASTR_64_8

        ld hl, (64-15) * 4
        add hl, sp
        ld sp, hl
        FILL_OFF_RASTR_64_16


        ld hl, mc_rastr_descriptors
        add hl,bc       // * 2
        ld sp, hl

        // Draw bottom 3-th of rastr during middle 3-th of colors
        pop hl: ld (RASTR0_15+1), hl
        pop hl: ld (RASTR0_14+1), hl
        pop hl: ld (RASTR0_13+1), hl
        pop hl: ld (RASTR0_12+1), hl
        pop hl: ld (RASTR0_11+1), hl
        pop hl: ld (RASTR0_10+1), hl
        pop hl: ld (RASTR0_9+1), hl
        pop hl: ld (RASTR0_8+1), hl

        // Draw middle 3-th of rastr during top 3-th of colors

        ld hl, (64-8) * 2
        add hl, sp
        ld sp, hl

        pop hl: ld (RASTR0_7+1), hl
        pop hl: ld (RASTR0_6+1), hl
        pop hl: ld (RASTR0_5+1), hl
        pop hl: ld (RASTR0_4+1), hl
        pop hl: ld (RASTR0_3+1), hl
        pop hl: ld (RASTR0_2+1), hl
        pop hl: ld (RASTR0_1+1), hl
        pop hl: ld (RASTR0_0+1), hl

        // Draw top 3-th of rastr during bottom 3-th of colors
        ; shift to 63 for MC rastr instead of 64 to move on next frame
        ld hl, (63-8) * 2
        add hl, sp
        ld sp, hl

         pop hl: ld (RASTR_23+1), hl
         pop hl: ld (RASTR0_22+1), hl
         pop hl: ld (RASTR0_21+1), hl
         pop hl: ld (RASTR0_20+1), hl
         pop hl: ld (RASTR0_19+1), hl
         pop hl: ld (RASTR0_18+1), hl
         pop hl: ld (RASTR0_17+1), hl
         pop hl: ld (RASTR0_16+1), hl


        ld hl, after_delay
        ld (hl), 0x21   // comment JP command by modify this byte

        jp draw_off_rastr_0

//*************************************************************************************
mc_step_drawing:
        ld sp, color_addr + 768                         ; 10
        ld hl, $ + 7                                    ; 14
        exx
start_draw_colors:
        jp 00                                           ; 8

        //MACRO prepare_rastr_drawing
        sla c : rl b    // bc*2
        ld hl, mc_rastr_descriptors
        add hl, bc      // *2
        ld sp, hl
        
        // Draw bottom 3-th of rastr during middle 3-th of colors
        pop hl: ld (RASTR_15+1), hl
        pop hl: ld (RASTR_14+1), hl
        pop hl: ld (RASTR_13+1), hl
        pop hl: ld (RASTR_12+1), hl
        pop hl: ld (RASTR_11+1), hl
        pop hl: ld (RASTR_10+1), hl
        pop hl: ld (RASTR_9+1), hl
        pop hl: ld (RASTR_8+1), hl

        // Draw middle 3-th of rastr during top 3-th of colors

        ld hl, (63-8) * 2
        add hl, sp
        ld sp, hl

        pop hl: ld (RASTR_7+1), hl
        pop hl: ld (RASTR_6+1), hl
        pop hl: ld (RASTR_5+1), hl
        pop hl: ld (RASTR_4+1), hl
        pop hl: ld (RASTR_3+1), hl
        pop hl: ld (RASTR_2+1), hl
        pop hl: ld (RASTR_1+1), hl
        pop hl: ld (RASTR_0+1), hl

        // Draw top 3-th of rastr during bottom 3-th of colors
        ; shift to 63 for MC rastr instead of 64 to move on next frame
        ld hl, (63-8) * 2
        add hl, sp
        ld sp, hl

        pop hl: ld (RASTR_23+1), hl
        pop hl: ld (RASTR_22+1), hl
        pop hl: ld (RASTR_21+1), hl
        pop hl: ld (RASTR_20+1), hl
        pop hl: ld (RASTR_19+1), hl
        pop hl: ld (RASTR_18+1), hl
        pop hl: ld (RASTR_17+1), hl
        pop hl: ld (RASTR_16+1), hl
                
        ld hl, draw_offrastr_offset
        add hl, bc
        jp hl


bank_drawing_common:
        ; delay
        ld hl, timings_data
        add hl, bc
        ld sp, hl
        pop hl
        DO_DELAY

after_delay        
        jp continue_mc_drawing

        ld hl, after_delay
        ld (hl), 0xc3   // restore JP command by modify this byte

        dec iy
        ; compare to -1
        ld c, iyh
        inc c
        jp z, lower_limit_reached      ; 10 ticks
        jp loop                        ; 12 ticks

continue_mc_drawing
        ; timing here on first frame: 71680 * 2 + 17988 + 224*6 - (19 + 22) - 20 = 162631
        ; after non-mc frame: 144704, between regular lines: 71680-224 = 71456
        scf
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
        DRAW_MULTICOLOR_LINE 23
        dec iy
        jp loop                        ; 12 ticks

        INCLUDE "draw_off_rastr.asm"

/*********************** routines *************/

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

//JP_IX_CODE          equ #e9dd
JP_VIA_HL_CODE      equ #e9d9

jp_ix_record_size       equ 12
jp_write_data_offset    equ 8
data_page_count         equ 4
jp_ix_bank_size         equ (imageHeight/64 + 2) * jp_ix_record_size

write_initial_jp_ix_table
        ld a, 0
page_loop:
        bit 0, a
        jr nz, continue_page        
        ld sp, jpix_table + jp_write_data_offset
continue_page:
        ld b, 3

        ld c, a
        and a
        set_page_by_bank
        ld a, c

        ld de, JP_VIA_HL_CODE
.rep:   write_jp_ix_data_via_de
        write_jp_ix_data_via_de
        ld hl, jp_ix_record_size - 4
        add hl, sp
        ld sp, hl
        djnz .rep

        ld sp, jpix_table + jp_ix_bank_size + jp_write_data_offset

        inc a
        cp 8
        jr nz, page_loop

        // write initial color data to restore
        SET_PAGE 6

        ld hl, color_descriptor + 4                     ; 10
        ld sp, hl                                       ; 6
        pop hl                                          ; 10
        ld sp, hl
        pop hl
        ld (color_data_to_restore), hl                  ; 16

        ld sp, stack_top - 2
        ret

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
        ASSERT $ <= 30000
         ORG 30000
generated_code:
        INCBIN "resources/compressed_data.mt_and_rt_reach.descriptor"
multicolor_code
        INCBIN "resources/compressed_data.multicolor"

timings_data
        INCBIN "resources/compressed_data.timings"
timings_data_end
update_jpix_helper
        INCBIN "resources/compressed_data.update_jpix_helper"
/*
src_data
        INCBIN "resources/samanasuke.bin", 0, 6144
color_data:
        INCBIN "resources/samanasuke.bin", 6144, 768
*/	

jpix_table EQU 0xc000

        ORG 0xc000
        PAGE 0
        INCBIN "resources/compressed_data.jpix0"
        INCBIN "resources/compressed_data.main0"
        INCBIN "resources/compressed_data.reach_descriptor0"
        ORG 0xc000
        PAGE 1
        INCBIN "resources/compressed_data.jpix1"
        INCBIN "resources/compressed_data.main1"
        INCBIN "resources/compressed_data.reach_descriptor1"

        ORG 0xc000
        PAGE 3
        INCBIN "resources/compressed_data.jpix2"
        INCBIN "resources/compressed_data.main2"
        INCBIN "resources/compressed_data.reach_descriptor2"

        ORG 0xc000
        PAGE 4
        INCBIN "resources/compressed_data.jpix3"
        INCBIN "resources/compressed_data.main3"
        INCBIN "resources/compressed_data.reach_descriptor3"

        ORG 0xc000
        PAGE 6
color_code
        INCBIN "resources/compressed_data.color"
color_descriptor
        INCBIN "resources/compressed_data.color_descriptor"
off_rastr_descriptors
        INCBIN "resources/compressed_data.off_rastr.descriptors"
mc_rastr_descriptors
        INCBIN "resources/compressed_data.mc_rastr.descriptors"
mc_descriptors
        INCBIN "resources/compressed_data.mc_descriptors"

data_end:

imageHeight     equ (timings_data_end - timings_data) / 2

/*************** Ennd image data. ******************/


/*************** Commands to SJ asm ******************/

    SAVESNA "build/draw_image.sna", main
    savetap "build/draw_image.tap", main
