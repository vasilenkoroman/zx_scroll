        MACRO DRAW_OFFSCREEN_LINES Iteration?, Step?, prevStep?
OFF_Iteration?_Step?_SP
                ld sp, 00
                IF (low($) + 6 < 256 && high(prevStep?) == high($))
                        ld l, low($ + 6)
                ELSE                        
                        ld hl, $ + 7
                ENDIF
                exx
OFF_Iteration?_Step?_JP    
                jp 00 ; rastr for multicolor ( up to 8 lines)
                // total ticks: 21
        ENDM          

        MACRO START_OFF_DRAWING label?
            ld hl, label?        ; 10
            jp (hl)              ; 14
        ENDM

        MACRO RESTORE_OFF_END labelName?, value16?
                ld hl, value16?
                ld (labelName?), hl
        ENDM

        MACRO UPDATE_SP2 l23?, l22?, l21?, l20?, l19?, l18?, l17?,   r0?, r1?, r2?, r3?, r4?, r5?, r6?
                ld a, high(16384)

                ld (l23? + 2), a:                  inc a
                ld (l22? + 2), a: ld (r0? + 2), a: inc a
                ld (l21? + 2), a: ld (r1? + 2), a: inc a
                ld (l20? + 2), a: ld (r2? + 2), a: inc a
                ld (l19? + 2), a: ld (r3? + 2), a: inc a
                ld (l18? + 2), a: ld (r4? + 2), a: inc a
                ld (l17? + 2), a: ld (r5? + 2), a: inc a
                                  ld (r6? + 2), a: inc a
        ENDM

        MACRO UPDATE_SP1 l15?, l14?, l13?, l12?, l11?, l10?, l9?,  r15?, r14?, r13?, r12?, r11?, r10?, r9?, r8?

                ld (l15? + 2), a:                   inc a
                ld (l14? + 2), a: ld (r15? + 2), a: inc a
                ld (l13? + 2), a: ld (r14? + 2), a: inc a
                ld (l12? + 2), a: ld (r13? + 2), a: inc a
                ld (l11? + 2), a: ld (r12? + 2), a: inc a
                ld (l10? + 2), a: ld (r11? + 2), a: inc a
                ld (l9? + 2), a:  ld (r10? + 2), a: inc a
                                  ld (r9? + 2), a:  inc a
                                  ld (r8? + 2), a
        ENDM

        MACRO UPDATE_SP0 l7?, l6?, l5?, l4?, l3?, l2?, l1?,  r7?, r6?, r5?, r4?, r3?, r2?, r1?, r0?

                ld (l7? + 2), a :                  inc a
                ld (l6? + 2), a : ld (r7? + 2), a: inc a
                ld (l5? + 2), a : ld (r6? + 2), a: inc a
                ld (l4? + 2), a : ld (r5? + 2), a: inc a
                ld (l3? + 2), a : ld (r4? + 2), a: inc a
                ld (l2? + 2), a : ld (r3? + 2), a: inc a
                ld (l1? + 2), a : ld (r2? + 2), a: inc a
                                  ld (r1? + 2), a: inc a
                                  ld (r0? + 2), a
        ENDM

        MACRO update_rastr itr?, L1?, L2?, L3?

upd_rastr_itr?_0
                ld hl,0
                ld (L1?+1), hl
upd_rastr_itr?_1
                ld hl,0
                ld (L2?+1), hl
                 

                ld hl, mc_rastr_descriptors_next + 127 * 2      ; 10
                add hl, bc                                      ; 21
                ld sp, hl                                       ; 27

                pop hl: ld (RASTR_23+1), hl
                pop hl: ld (L3?+1), hl
                // 10+11+6+10+16+10+16=79

        ENDM

        MACRO   UPDATE_JPIX_HELPER value
                ld a, i                  ; 9
                IF (value < 0)
                        add value        ; 9/16
                ENDIF
                ld (jpix_h_pos+2), a     ; 22/29, dt=2/9

        ENDM

        MACRO SET_NEXT_STEP value
                ld ix, value
        ENDM

        MACRO SET_NEXT_STEP_SHORT value
                ld ixl, low(value)
        ENDM

it0_start:      ld a, 0x54
                out (0xfd), a
                DRAW_OFFSCREEN_LINES 0, 23,  0
L01:            DRAW_OFFSCREEN_LINES 0, 15,  L01
L02:            DRAW_OFFSCREEN_LINES   0, 7, L02

it7_start:      //ld a, 0x54
                //out (0xfd), a
                DRAW_OFFSCREEN_LINES 0, 22, it7_start
L11:            DRAW_OFFSCREEN_LINES 0, 14, L11
L12:            DRAW_OFFSCREEN_LINES 0, 6,  L12

                //ld a, 0x53                                           
                dec a                                           ; 54->53
it6_start:      out (0xfd), a                                           
                DRAW_OFFSCREEN_LINES 0, 21, it6_start
L21:            DRAW_OFFSCREEN_LINES 0, 13, L21
L22:            DRAW_OFFSCREEN_LINES 0, 5,  L22

it5_start:      //ld a, 0x53
                //out (0xfd), a
                DRAW_OFFSCREEN_LINES 0, 20, it5_start
L31:            DRAW_OFFSCREEN_LINES 0, 12, L31
L32:            DRAW_OFFSCREEN_LINES 0, 4, L32

                //ld a, 0x51
                add  -2                                         ; 53->51
it4_start:      out (0xfd), a
                DRAW_OFFSCREEN_LINES 0, 19, it4_start
L41:            DRAW_OFFSCREEN_LINES 0, 11, L41
L42:            DRAW_OFFSCREEN_LINES 0, 3,  L42

it3_start:      //ld a, 0x51
                //out (0xfd), a
                DRAW_OFFSCREEN_LINES 0, 18, it3_start
L51:            DRAW_OFFSCREEN_LINES 0, 10, L51
L52:            DRAW_OFFSCREEN_LINES 0, 2,  L52

                dec a                                           ; 51->50
it2_start:      out (0xfd), a
                DRAW_OFFSCREEN_LINES 0, 17, it2_start
L61:            DRAW_OFFSCREEN_LINES 0, 9,  L61
L62:            DRAW_OFFSCREEN_LINES 0, 1,  L62

it1_start:      //ld a, 0x50
                //out (0xfd), a
                DRAW_OFFSCREEN_LINES 0, 16, it1_start
L71:            DRAW_OFFSCREEN_LINES 0, 8,  L71
L72:            DRAW_OFFSCREEN_LINES 0, 0,  L72

                // negative values (next 7 steps)

L80:            sub -4                                          ; 50->54
it0_end:        out (0xfd), a
                DRAW_OFFSCREEN_LINES 1, 16, L80
L81:            DRAW_OFFSCREEN_LINES 1, 8,  L81
L82:            DRAW_OFFSCREEN_LINES 1, 0,  L82

it7_end:        //ld a, 0x54
                //out (0xfd), a
                nop
                DRAW_OFFSCREEN_LINES 2, 16, it7_end
L91:            DRAW_OFFSCREEN_LINES 2, 8,  L91
L92:            DRAW_OFFSCREEN_LINES 2, 0,  L92

L100            dec a                                           ; 54->53
it6_end:        out (0xfd), a
                DRAW_OFFSCREEN_LINES 3, 16, L100
L101:           DRAW_OFFSCREEN_LINES 3, 8,  L101
L102:           DRAW_OFFSCREEN_LINES 3, 0,  L102

it5_end:        //ld a, 0x53
                //out (0xfd), a
                nop
                DRAW_OFFSCREEN_LINES 4, 16, it5_end
L111:           DRAW_OFFSCREEN_LINES 4, 8,  L111
L112:           DRAW_OFFSCREEN_LINES 4, 0,  L112

L120:           add -2                                          ; 53->51
it4_end:        out (0xfd), a
                DRAW_OFFSCREEN_LINES 5, 16, L120
L121:           DRAW_OFFSCREEN_LINES 5, 8,  L121
L122:           DRAW_OFFSCREEN_LINES 5, 0,  L122

it3_end:        //ld a, 0x51
                //out (0xfd), a
                nop
                DRAW_OFFSCREEN_LINES 6, 16, it3_end
L131:           DRAW_OFFSCREEN_LINES 6, 8,  L131
L132:           DRAW_OFFSCREEN_LINES 6, 0,  L132

L140:           dec a                                           ; 51->50
it2_end:        out (0xfd), a
                DRAW_OFFSCREEN_LINES 7, 16, L140
L141:           DRAW_OFFSCREEN_LINES 7, 8,  L141
L142:           DRAW_OFFSCREEN_LINES 7, 0,  L142

it1_end:        ;ex de,hl
                ;jp hl

bank_drawing_common:
                exa                                 ; save current value for the next_step_first_bank
bank_drawing_common2:        
load_timings_data
                ld hl, timings_data + 12
                add hl, bc
                ld sp, hl
                pop hl

                ld sp, stack_top                    ; jump to multicolor drawing after player or imidiatly
        IF (HAS_PLAYER == 1)
player_pg       SET_PAGE 7
        ENDIF

                delayFrom59

        IF (HAS_PLAYER == 1)
                ;jp play
                INCLUDE "fast_psg_player.asm"
        ELSE                
                scf
                ret
        ENDIF                                


draw_off_rastr_7
                DISPLAY "draw_off_rastr_7= ", $
                exx
                ASSERT high(draw_off_rastr_7) == high(draw_off_rastr_6)
                SET_NEXT_STEP_SHORT draw_off_rastr_6
                UPDATE_JPIX_HELPER -2
                
                update_rastr 7, RASTR_8, RASTR_0, RASTR_16

                UPDATE_SP2 OFF_0_22_SP, OFF_0_21_SP, OFF_0_20_SP, OFF_0_19_SP, OFF_0_18_SP, OFF_0_17_SP, OFF_0_16_SP,   RASTRS_17, RASTRS_18, RASTRS_19, RASTRS_20, RASTRS_21, RASTRS_22, RASTRS_16
                UPDATE_SP1 OFF_0_14_SP, OFF_0_13_SP, OFF_0_12_SP, OFF_0_11_SP, OFF_0_10_SP, OFF_0_9_SP,  OFF_0_8_SP,    RASTRS_1,  RASTRS_2,  RASTRS_3,  RASTRS_4,  RASTRS_5,  RASTRS_6,  RASTRS_7,  RASTRS_0
                UPDATE_SP0 OFF_0_6_SP,  OFF_0_5_SP,  OFF_0_4_SP,  OFF_0_3_SP,  OFF_0_2_SP,  OFF_0_1_SP,  OFF_0_0_SP,    RASTRS_9,  RASTRS_10, RASTRS_11, RASTRS_12, RASTRS_13, RASTRS_14, RASTRS_15, RASTRS_8

                RESTORE_OFF_END it0_end, 0xd3 + 0xfd * 256
page7_depend_4  ld a, 0x54
                out (0xfd), a
                START_OFF_DRAWING it7_start
draw_off_rastr_6
                DISPLAY "draw_off_rastr_6= ", $
                exx
                SET_NEXT_STEP draw_off_rastr_5
                UPDATE_JPIX_HELPER 2
                update_rastr 6, RASTR_9, RASTR_1, RASTR_17

                inc iyh          ; (0x53->0x54)

                UPDATE_SP2 OFF_0_21_SP, OFF_0_20_SP, OFF_0_19_SP, OFF_0_18_SP, OFF_0_17_SP, OFF_0_16_SP, OFF_1_16_SP,   RASTRS_18, RASTRS_19, RASTRS_20, RASTRS_21, RASTRS_22, RASTRS_16, RASTRS_17
                UPDATE_SP1 OFF_0_13_SP, OFF_0_12_SP, OFF_0_11_SP, OFF_0_10_SP, OFF_0_9_SP,  OFF_0_8_SP,  OFF_1_8_SP,    RASTRS_2,  RASTRS_3,  RASTRS_4,  RASTRS_5,  RASTRS_6,  RASTRS_7,  RASTRS_0,  RASTRS_1
                UPDATE_SP0 OFF_0_5_SP,  OFF_0_4_SP,  OFF_0_3_SP,  OFF_0_2_SP,  OFF_0_1_SP,  OFF_0_0_SP,  OFF_1_0_SP,    RASTRS_10, RASTRS_11, RASTRS_12, RASTRS_13, RASTRS_14, RASTRS_15, RASTRS_8, RASTRS_9

                RESTORE_OFF_END it7_end, 0x31 * 256
                ld a, 0x53
                START_OFF_DRAWING it6_start

draw_off_rastr_5
                DISPLAY "draw_off_rastr_5= ", $
                exx
                SET_NEXT_STEP draw_off_rastr_4
                UPDATE_JPIX_HELPER -2
                update_rastr 5, RASTR_10, RASTR_2, RASTR_18

                UPDATE_SP2 OFF_0_20_SP, OFF_0_19_SP, OFF_0_18_SP, OFF_0_17_SP, OFF_0_16_SP, OFF_1_16_SP, OFF_2_16_SP,   RASTRS_19, RASTRS_20, RASTRS_21, RASTRS_22, RASTRS_16, RASTRS_17, RASTRS_18
                UPDATE_SP1 OFF_0_12_SP, OFF_0_11_SP, OFF_0_10_SP, OFF_0_9_SP,  OFF_0_8_SP,  OFF_1_8_SP,  OFF_2_8_SP,    RASTRS_3,  RASTRS_4,  RASTRS_5,  RASTRS_6,  RASTRS_7,  RASTRS_0,  RASTRS_1,  RASTRS_2
                UPDATE_SP0 OFF_0_4_SP,  OFF_0_3_SP,  OFF_0_2_SP,  OFF_0_1_SP,  OFF_0_0_SP,  OFF_1_0_SP,  OFF_2_0_SP,    RASTRS_11, RASTRS_12, RASTRS_13, RASTRS_14, RASTRS_15, RASTRS_8,  RASTRS_9,  RASTRS_10


                RESTORE_OFF_END it6_end, 0xd3 + 0xfd * 256
page7_depend_3  ld a, 0x53
                out (0xfd), a
                START_OFF_DRAWING it5_start

draw_off_rastr_4
                DISPLAY "draw_off_rastr_4= ", $
                exx
                SET_NEXT_STEP draw_off_rastr_3
                UPDATE_JPIX_HELPER 2
                update_rastr 4, RASTR_11, RASTR_3, RASTR_19

                ld A, 0xCF      // set 1, a (0x51 -> 0x53)
                ld (RASTRS_19 - 3), A

                UPDATE_SP2 OFF_0_19_SP, OFF_0_18_SP, OFF_0_17_SP, OFF_0_16_SP, OFF_1_16_SP, OFF_2_16_SP, OFF_3_16_SP,   RASTRS_20, RASTRS_21, RASTRS_22, RASTRS_16, RASTRS_17, RASTRS_18, RASTRS_19
                UPDATE_SP1 OFF_0_11_SP, OFF_0_10_SP, OFF_0_9_SP,  OFF_0_8_SP,  OFF_1_8_SP,  OFF_2_8_SP,  OFF_3_8_SP,    RASTRS_4,  RASTRS_5,  RASTRS_6,  RASTRS_7,  RASTRS_0,  RASTRS_1,  RASTRS_2,  RASTRS_3
                UPDATE_SP0 OFF_0_3_SP,  OFF_0_2_SP,  OFF_0_1_SP,  OFF_0_0_SP,  OFF_1_0_SP,  OFF_2_0_SP,  OFF_3_0_SP,    RASTRS_12, RASTRS_13, RASTRS_14, RASTRS_15, RASTRS_8,  RASTRS_9,  RASTRS_10, RASTRS_11

                RESTORE_OFF_END it5_end,  0x31 * 256
                ld a, 0x51
                START_OFF_DRAWING it4_start

draw_off_rastr_3
                DISPLAY "draw_off_rastr_3= ", $
                exx
                SET_NEXT_STEP draw_off_rastr_2
                UPDATE_JPIX_HELPER -2
                update_rastr 3, RASTR_12, RASTR_4, RASTR_20

                UPDATE_SP2 OFF_0_18_SP, OFF_0_17_SP, OFF_0_16_SP, OFF_1_16_SP, OFF_2_16_SP, OFF_3_16_SP, OFF_4_16_SP,   RASTRS_21, RASTRS_22, RASTRS_16, RASTRS_17, RASTRS_18, RASTRS_19, RASTRS_20
                UPDATE_SP1 OFF_0_10_SP, OFF_0_9_SP,  OFF_0_8_SP,  OFF_1_8_SP, OFF_2_8_SP,   OFF_3_8_SP,  OFF_4_8_SP,    RASTRS_5,  RASTRS_6,  RASTRS_7,  RASTRS_0,  RASTRS_1,  RASTRS_2,  RASTRS_3,  RASTRS_4
                UPDATE_SP0 OFF_0_2_SP,  OFF_0_1_SP,  OFF_0_0_SP,  OFF_1_0_SP,  OFF_2_0_SP,  OFF_3_0_SP,  OFF_4_0_SP,    RASTRS_13, RASTRS_14, RASTRS_15, RASTRS_8,  RASTRS_9,  RASTRS_10, RASTRS_11, RASTRS_12

                RESTORE_OFF_END it4_end, 0xd3 + 0xfd * 256
page7_depend_1  ld a, 0x51
                out (0xfd), a
                START_OFF_DRAWING it3_start

draw_off_rastr_2
                DISPLAY "draw_off_rastr_2= ", $
                exx
                ASSERT high(draw_off_rastr_2) == high(draw_off_rastr_1)
                SET_NEXT_STEP_SHORT draw_off_rastr_1
                UPDATE_JPIX_HELPER 2
                update_rastr 2, RASTR_13, RASTR_5, RASTR_21

                ld A, 0xC7      // set 0, a (0x50 -> 0x51)
                ld (RASTRS_21 - 3), A

                UPDATE_SP2 OFF_0_17_SP, OFF_0_16_SP, OFF_1_16_SP, OFF_2_16_SP, OFF_3_16_SP, OFF_4_16_SP, OFF_5_16_SP,   RASTRS_22, RASTRS_16, RASTRS_17, RASTRS_18, RASTRS_19, RASTRS_20, RASTRS_21
                UPDATE_SP1 OFF_0_9_SP,  OFF_0_8_SP,  OFF_1_8_SP,  OFF_2_8_SP,  OFF_3_8_SP,  OFF_4_8_SP,  OFF_5_8_SP,    RASTRS_6,  RASTRS_7,  RASTRS_0,  RASTRS_1,  RASTRS_2,  RASTRS_3,  RASTRS_4, RASTRS_5
                UPDATE_SP0 OFF_0_1_SP,  OFF_0_0_SP,  OFF_1_0_SP,  OFF_2_0_SP,  OFF_3_0_SP,  OFF_4_0_SP,  OFF_5_0_SP,    RASTRS_14, RASTRS_15, RASTRS_8,  RASTRS_9,  RASTRS_10, RASTRS_11, RASTRS_12, RASTRS_13

                RESTORE_OFF_END it3_end, 0x31 * 256
                ld a, 0x50
                START_OFF_DRAWING it2_start

draw_off_rastr_1
                ;SET_NEXT_STEP draw_off_rastr_7
                DISPLAY "draw_off_rastr_1= ", $
                exx
                ld a, low(non_mc_draw_step)
                ld (before_update_jpix+1), a

                UPDATE_JPIX_HELPER -2
                update_rastr 1, RASTR_14, RASTR_6, RASTR_22

                UPDATE_SP2 OFF_0_16_SP, OFF_1_16_SP, OFF_2_16_SP, OFF_3_16_SP, OFF_4_16_SP, OFF_5_16_SP, OFF_6_16_SP,   RASTRS_16, RASTRS_17, RASTRS_18, RASTRS_19, RASTRS_20, RASTRS_21, RASTRS_22
                UPDATE_SP1 OFF_0_8_SP,  OFF_1_8_SP,  OFF_2_8_SP,  OFF_3_8_SP,  OFF_4_8_SP,  OFF_5_8_SP,  OFF_6_8_SP,    RASTRS_7,  RASTRS_0,  RASTRS_1,  RASTRS_2,  RASTRS_3,  RASTRS_4, RASTRS_5, RASTRS_6
                UPDATE_SP0 OFF_0_0_SP,  OFF_1_0_SP,  OFF_2_0_SP,  OFF_3_0_SP,  OFF_4_0_SP,  OFF_5_0_SP,  OFF_6_0_SP,    RASTRS_15, RASTRS_8,  RASTRS_9,  RASTRS_10, RASTRS_11, RASTRS_12, RASTRS_13, RASTRS_14

                RESTORE_OFF_END it2_end, 0xd3 + 0xfd * 256
page7_depend_2  ld a, 0x50
                out (0xfd), a
                START_OFF_DRAWING it1_start
