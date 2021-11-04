        MACRO DRAW_OFFSCREEN_LINES1 Iteration?, Step?
OFF_Iteration?_Step?_SP
                ld sp, 00
                IF (low($) + 6 < 256 && low($) -7 >= 0)
                        ld l, low($ + 6)
                ELSE                        
                        ld hl, $ + 7
                ENDIF
                exx
OFF_Iteration?_Step?_JP    
                jp 00 ; rastr for multicolor ( up to 8 lines)
                // total ticks: 21
        ENDM          

        MACRO DRAW_OFFSCREEN_LINES2 Iteration?, Step?
OFF_Iteration?_Step?_SP
                ld sp, 00
                IF (low($) + 6 < 256 && low($) -3 >= 0)
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
            ld h, high(label?)
            jp label?
        ENDM

        MACRO RESTORE_OFF_END labelName?, value16?
                ld hl, value16?
                ld (labelName?), hl
        ENDM

        MACRO UPDATE_SP l23?, l22?, l21?, l20?, l19?, l18?, l17?, l15?, l14?, l13?, l12?, l11?, l10?, l9?, l7?, l6?, l5?, l4?, l3?, l2?, l1?
                ld a, high(16384)

                ld (l23? + 2), a: inc a
                ld (l22? + 2), a: inc a
                ld (l21? + 2), a: inc a
                ld (l20? + 2), a: inc a
                ld (l19? + 2), a: inc a
                ld (l18? + 2), a: inc a
                ld (l17? + 2), a
                add 2
                ld (l15? + 2), a: inc a
                ld (l14? + 2), a: inc a
                ld (l13? + 2), a: inc a
                ld (l12? + 2), a: inc a
                ld (l11? + 2), a: inc a
                ld (l10? + 2), a: inc a
                ld (l9? + 2), a
                add 2
                ld (l7? + 2), a : inc a
                ld (l6? + 2), a : inc a
                ld (l5? + 2), a : inc a
                ld (l4? + 2), a : inc a
                ld (l3? + 2), a : inc a
                ld (l2? + 2), a : inc a
                ld (l1? + 2), a
        ENDM

                ALIGN 32

it0_start:      ld a, 0x54
                out (0xfd), a
                DRAW_OFFSCREEN_LINES1 0, 23
                DRAW_OFFSCREEN_LINES2 0, 15
                DRAW_OFFSCREEN_LINES2   0, 7

it7_start:      ld a, 0x54
                out (0xfd), a
                DRAW_OFFSCREEN_LINES1 0, 22
                DRAW_OFFSCREEN_LINES2 0, 14
                DRAW_OFFSCREEN_LINES2 0, 6

it6_start:      ld a, 0x53
                out (0xfd), a
                DRAW_OFFSCREEN_LINES1 0, 21
                DRAW_OFFSCREEN_LINES2 0, 13
                DRAW_OFFSCREEN_LINES2 0, 5

it5_start:      ld a, 0x53
                out (0xfd), a
                DRAW_OFFSCREEN_LINES1 0, 20
                DRAW_OFFSCREEN_LINES2 0, 12
                DRAW_OFFSCREEN_LINES2 0, 4

it4_start:      ld a, 0x51
                out (0xfd), a
                DRAW_OFFSCREEN_LINES1 0, 19
                DRAW_OFFSCREEN_LINES2 0, 11
                DRAW_OFFSCREEN_LINES2 0, 3

it3_start:      ld a, 0x51
                out (0xfd), a
                DRAW_OFFSCREEN_LINES1 0, 18
                DRAW_OFFSCREEN_LINES2 0, 10
                DRAW_OFFSCREEN_LINES2 0, 2

it2_start:      ld a, 0x50
                out (0xfd), a
                DRAW_OFFSCREEN_LINES1 0, 17
                DRAW_OFFSCREEN_LINES2 0, 9
                DRAW_OFFSCREEN_LINES2 0, 1

it1_start:      ld a, 0x50
                out (0xfd), a
                DRAW_OFFSCREEN_LINES1 0, 16
                DRAW_OFFSCREEN_LINES2 0, 8
                DRAW_OFFSCREEN_LINES2 0, 0

                // negative values (next 7 steps)

it0_end:        ld a, 0x54
                out (0xfd), a
                DRAW_OFFSCREEN_LINES1 1, 16
                DRAW_OFFSCREEN_LINES2 1, 8
                DRAW_OFFSCREEN_LINES2 1, 0

it7_end:        ld a, 0x54
                out (0xfd), a
                DRAW_OFFSCREEN_LINES1 2, 16
                DRAW_OFFSCREEN_LINES2 2, 8
                DRAW_OFFSCREEN_LINES2 2, 0

it6_end:        ld a, 0x53
                out (0xfd), a
                DRAW_OFFSCREEN_LINES1 3, 16
                DRAW_OFFSCREEN_LINES2 3, 8
                DRAW_OFFSCREEN_LINES2 3, 0

it5_end:        ld a, 0x53
                out (0xfd), a
                DRAW_OFFSCREEN_LINES1 4, 16
                DRAW_OFFSCREEN_LINES2 4, 8
                DRAW_OFFSCREEN_LINES2 4, 0

it4_end:        ld a, 0x51
                out (0xfd), a
                DRAW_OFFSCREEN_LINES1 5, 16
                DRAW_OFFSCREEN_LINES2 5, 8
                DRAW_OFFSCREEN_LINES2 5, 0

it3_end:        ld a, 0x51
                out (0xfd), a
                DRAW_OFFSCREEN_LINES1 6, 16
                DRAW_OFFSCREEN_LINES2 6, 8
                DRAW_OFFSCREEN_LINES2 6, 0

it2_end:        ld a, 0x50
                out (0xfd), a
                DRAW_OFFSCREEN_LINES1 7, 16
                DRAW_OFFSCREEN_LINES2 7, 8
                DRAW_OFFSCREEN_LINES2 7, 0

it1_end:        jp bank_drawing_common


draw_off_rastr_7
                UPDATE_SP OFF_0_22_SP, OFF_0_21_SP, OFF_0_20_SP, OFF_0_19_SP, OFF_0_18_SP, OFF_0_17_SP, OFF_0_16_SP, OFF_0_14_SP, OFF_0_13_SP, OFF_0_12_SP, OFF_0_11_SP, OFF_0_10_SP, OFF_0_9_SP,  OFF_0_8_SP, OFF_0_6_SP,  OFF_0_5_SP,  OFF_0_4_SP,  OFF_0_3_SP,  OFF_0_2_SP,  OFF_0_1_SP,  OFF_0_0_SP

                RESTORE_OFF_END it0_end, 0x3e + 0x54 * 256
                START_OFF_DRAWING it7_start

draw_off_rastr_6
                UPDATE_SP OFF_0_21_SP, OFF_0_20_SP, OFF_0_19_SP, OFF_0_18_SP, OFF_0_17_SP, OFF_0_16_SP, OFF_1_16_SP, OFF_0_13_SP, OFF_0_12_SP, OFF_0_11_SP, OFF_0_10_SP, OFF_0_9_SP,  OFF_0_8_SP,  OFF_1_8_SP,  OFF_0_5_SP,  OFF_0_4_SP,  OFF_0_3_SP,  OFF_0_2_SP,  OFF_0_1_SP,  OFF_0_0_SP,  OFF_1_0_SP

                RESTORE_OFF_END it7_end, 0x3e + 0x54 * 256
                START_OFF_DRAWING it6_start

draw_off_rastr_5
                UPDATE_SP OFF_0_20_SP, OFF_0_19_SP, OFF_0_18_SP, OFF_0_17_SP, OFF_0_16_SP, OFF_1_16_SP, OFF_2_16_SP, OFF_0_12_SP, OFF_0_11_SP, OFF_0_10_SP, OFF_0_9_SP,  OFF_0_8_SP,  OFF_1_8_SP, OFF_2_8_SP,  OFF_0_4_SP,  OFF_0_3_SP,  OFF_0_2_SP,  OFF_0_1_SP,  OFF_0_0_SP,  OFF_1_0_SP,  OFF_2_0_SP


                RESTORE_OFF_END it6_end, 0x3e + 0x53 * 256
                START_OFF_DRAWING it5_start

draw_off_rastr_4
                UPDATE_SP OFF_0_19_SP, OFF_0_18_SP, OFF_0_17_SP, OFF_0_16_SP, OFF_1_16_SP, OFF_2_16_SP, OFF_3_16_SP, OFF_0_11_SP, OFF_0_10_SP, OFF_0_9_SP,  OFF_0_8_SP,  OFF_1_16_SP, OFF_2_8_SP,  OFF_3_8_SP,  OFF_0_3_SP,  OFF_0_2_SP,  OFF_0_1_SP,  OFF_0_0_SP,  OFF_1_0_SP,  OFF_2_0_SP,  OFF_3_0_SP

                RESTORE_OFF_END it5_end, 0x3e + 0x53 * 256
                START_OFF_DRAWING it4_start

draw_off_rastr_3
                UPDATE_SP OFF_0_18_SP, OFF_0_17_SP, OFF_0_16_SP, OFF_1_16_SP, OFF_2_16_SP, OFF_3_16_SP, OFF_4_16_SP, OFF_0_10_SP, OFF_0_9_SP,  OFF_0_8_SP,  OFF_1_16_SP, OFF_2_8_SP,  OFF_3_8_SP,  OFF_4_8_SP,  OFF_0_2_SP,  OFF_0_1_SP,  OFF_0_0_SP,  OFF_1_0_SP,  OFF_2_0_SP,  OFF_3_0_SP,  OFF_4_0_SP

                RESTORE_OFF_END it4_end, 0x3e + 0x51 * 256
                START_OFF_DRAWING it3_start

draw_off_rastr_2
                UPDATE_SP OFF_0_17_SP, OFF_0_16_SP, OFF_1_16_SP, OFF_2_16_SP, OFF_3_16_SP, OFF_4_16_SP, OFF_5_16_SP, OFF_0_9_SP,  OFF_0_8_SP,  OFF_1_16_SP, OFF_2_8_SP,  OFF_3_8_SP,  OFF_4_8_SP,  OFF_5_8_SP, OFF_0_1_SP,  OFF_0_0_SP,  OFF_1_0_SP,  OFF_2_0_SP,  OFF_3_0_SP,  OFF_4_0_SP,  OFF_5_0_SP

                RESTORE_OFF_END it3_end, 0x3e + 0x51 * 256
                START_OFF_DRAWING it2_start

draw_off_rastr_1
                UPDATE_SP OFF_0_16_SP, OFF_1_16_SP, OFF_2_16_SP, OFF_3_16_SP, OFF_4_16_SP, OFF_5_16_SP, OFF_6_16_SP, OFF_0_8_SP,  OFF_1_16_SP, OFF_2_8_SP,  OFF_3_8_SP,  OFF_4_8_SP,  OFF_5_8_SP,  OFF_6_8_SP, OFF_0_0_SP,  OFF_1_0_SP,  OFF_2_0_SP,  OFF_3_0_SP,  OFF_4_0_SP,  OFF_5_0_SP,  OFF_6_0_SP

                RESTORE_OFF_END it2_end, 0x3e + 0x50 * 256
                START_OFF_DRAWING it1_start
