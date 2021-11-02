        MACRO UPDATE_SP l23?, l22?, l21?, l20?, l19?, l18?, l17?, l15?, l14?, l13?, l12?, l11?, l10?, l9?, l7?, l6?, l5?, l4?, l3?, l2?, l1?
                ld a, high(16384)

                ld (l23? + 1), a: inc a
                ld (l22? + 1), a: inc a
                ld (l21? + 1), a: inc a
                ld (l20? + 1), a: inc a
                ld (l19? + 1), a: inc a
                ld (l18? + 1), a: inc a
                ld (l17? + 1), a
                add 2
                ld (l15? + 1), a: inc a
                ld (l14? + 1), a: inc a
                ld (l13? + 1), a: inc a
                ld (l12? + 1), a: inc a
                ld (l11? + 1), a: inc a
                ld (l10? + 1), a: inc a
                ld (l9? + 1), a
                add 2
                ld (l7? + 1), a : inc a
                ld (l6? + 1), a : inc a
                ld (l5? + 1), a : inc a
                ld (l4? + 1), a : inc a
                ld (l3? + 1), a : inc a
                ld (l2? + 1), a : inc a
                ld (l1? + 1), a
        ENDM

                ALIGN 32

it0_start:      ld a, 0x54
                out (0xfd), a
                DRAW_OFFSCREEN_LINES 0, 23
                DRAW_OFFSCREEN_LINES_S 0, 15
                DRAW_OFFSCREEN_LINES_S 0, 7

it7_start:      ld a, 0x54
                out (0xfd), a
                DRAW_OFFSCREEN_LINES 0, 22
                DRAW_OFFSCREEN_LINES_S 0, 14
                DRAW_OFFSCREEN_LINES_S 0, 6

it6_start:      ld a, 0x53
                out (0xfd), a
                DRAW_OFFSCREEN_LINES 0, 21
                DRAW_OFFSCREEN_LINES_S 0, 13
                DRAW_OFFSCREEN_LINES_S 0, 5

it5_start:      ld a, 0x53
                out (0xfd), a
                DRAW_OFFSCREEN_LINES 0, 20
                DRAW_OFFSCREEN_LINES_S 0, 12
                DRAW_OFFSCREEN_LINES_S 0, 4

it4_start:      ld a, 0x51
                out (0xfd), a
                DRAW_OFFSCREEN_LINES 0, 19
                DRAW_OFFSCREEN_LINES_S 0, 11
                DRAW_OFFSCREEN_LINES_S 0, 3

it3_start:      ld a, 0x51
                out (0xfd), a
                DRAW_OFFSCREEN_LINES 0, 18
                DRAW_OFFSCREEN_LINES_S 0, 10
                DRAW_OFFSCREEN_LINES_S 0, 2

it2_start:      ld a, 0x50
                out (0xfd), a
                DRAW_OFFSCREEN_LINES 0, 17
                DRAW_OFFSCREEN_LINES_S 0, 9
                DRAW_OFFSCREEN_LINES_S 0, 1

it1_start:      ld a, 0x50
                out (0xfd), a
                DRAW_OFFSCREEN_LINES 0, 16
                DRAW_OFFSCREEN_LINES_S 0, 8
                DRAW_OFFSCREEN_LINES_S 0, 0

                // negative values (next 7 steps)

it0_end:        ld a, 0x54
                out (0xfd), a
                DRAW_OFFSCREEN_LINES 1, 16
                DRAW_OFFSCREEN_LINES 1, 8
                DRAW_OFFSCREEN_LINES 1, 0

it7_end:        ld a, 0x54
                out (0xfd), a
                DRAW_OFFSCREEN_LINES 2, 16
                DRAW_OFFSCREEN_LINES 2, 8
                DRAW_OFFSCREEN_LINES 2, 0

it6_end:        ld a, 0x53
                out (0xfd), a
                DRAW_OFFSCREEN_LINES 3, 16
                DRAW_OFFSCREEN_LINES 3, 8
                DRAW_OFFSCREEN_LINES 3, 0

it5_end:        ld a, 0x53
                out (0xfd), a
                DRAW_OFFSCREEN_LINES 4, 16
                DRAW_OFFSCREEN_LINES 4, 8
                DRAW_OFFSCREEN_LINES 4, 0

it4_end:        ld a, 0x51
                out (0xfd), a
                DRAW_OFFSCREEN_LINES 5, 16
                DRAW_OFFSCREEN_LINES 5, 8
                DRAW_OFFSCREEN_LINES 5, 0

it3_end:        ld a, 0x51
                out (0xfd), a
                DRAW_OFFSCREEN_LINES 6, 16
                DRAW_OFFSCREEN_LINES 6, 8
                DRAW_OFFSCREEN_LINES 6, 0

it2_end:        ld a, 0x50
                out (0xfd), a
                DRAW_OFFSCREEN_LINES 7, 16
                DRAW_OFFSCREEN_LINES 7, 8
                DRAW_OFFSCREEN_LINES 7, 0

it1_end:        jp bank_drawing_common


draw_off_rastr_7
                UPDATE_SP OFF_0_22_SP, OFF_0_21_SP, OFF_0_20_SP, OFF_0_19_SP, OFF_0_18_SP, OFF_0_17_SP, OFF_0_16_SP, OFF_0_14_SP, OFF_0_13_SP, OFF_0_12_SP, OFF_0_11_SP, OFF_0_10_SP, OFF_0_9_SP,  OFF_0_8_SP, OFF_0_6_SP,  OFF_0_5_SP,  OFF_0_4_SP,  OFF_0_3_SP,  OFF_0_2_SP,  OFF_0_1_SP,  OFF_0_0_SP

                RESTORE_OFF_END it0_end, 0x3e + 0x54 * 256
                jp it7_start

draw_off_rastr_6
                UPDATE_SP OFF_0_21_SP, OFF_0_20_SP, OFF_0_19_SP, OFF_0_18_SP, OFF_0_17_SP, OFF_0_16_SP, OFF_1_16_SP, OFF_0_13_SP, OFF_0_12_SP, OFF_0_11_SP, OFF_0_10_SP, OFF_0_9_SP,  OFF_0_8_SP,  OFF_1_8_SP,  OFF_0_5_SP,  OFF_0_4_SP,  OFF_0_3_SP,  OFF_0_2_SP,  OFF_0_1_SP,  OFF_0_0_SP,  OFF_1_0_SP

                RESTORE_OFF_END it7_end, 0x3e + 0x54 * 256
                jp it6_start

draw_off_rastr_5
                UPDATE_SP OFF_0_20_SP, OFF_0_19_SP, OFF_0_18_SP, OFF_0_17_SP, OFF_0_16_SP, OFF_1_16_SP, OFF_2_16_SP, OFF_0_12_SP, OFF_0_11_SP, OFF_0_10_SP, OFF_0_9_SP,  OFF_0_8_SP,  OFF_1_16_SP, OFF_2_8_SP,  OFF_0_4_SP,  OFF_0_3_SP,  OFF_0_2_SP,  OFF_0_1_SP,  OFF_0_0_SP,  OFF_1_0_SP,  OFF_2_0_SP


                RESTORE_OFF_END it6_end, 0x3e + 0x53 * 256
                jp it5_start

draw_off_rastr_4
                UPDATE_SP OFF_0_19_SP, OFF_0_18_SP, OFF_0_17_SP, OFF_0_16_SP, OFF_1_16_SP, OFF_2_16_SP, OFF_3_16_SP, OFF_0_11_SP, OFF_0_10_SP, OFF_0_9_SP,  OFF_0_8_SP,  OFF_1_16_SP, OFF_2_8_SP,  OFF_3_8_SP,  OFF_0_3_SP,  OFF_0_2_SP,  OFF_0_1_SP,  OFF_0_0_SP,  OFF_1_0_SP,  OFF_2_0_SP,  OFF_3_0_SP

                RESTORE_OFF_END it5_end, 0x3e + 0x53 * 256
                jp it4_start

draw_off_rastr_3
                UPDATE_SP OFF_0_18_SP, OFF_0_17_SP, OFF_0_16_SP, OFF_1_16_SP, OFF_2_16_SP, OFF_3_16_SP, OFF_4_16_SP, OFF_0_10_SP, OFF_0_9_SP,  OFF_0_8_SP,  OFF_1_16_SP, OFF_2_8_SP,  OFF_3_8_SP,  OFF_4_8_SP,  OFF_0_2_SP,  OFF_0_1_SP,  OFF_0_0_SP,  OFF_1_0_SP,  OFF_2_0_SP,  OFF_3_0_SP,  OFF_4_0_SP

                RESTORE_OFF_END it4_end, 0x3e + 0x51 * 256
                jp it3_start

draw_off_rastr_2
                UPDATE_SP OFF_0_17_SP, OFF_0_16_SP, OFF_1_16_SP, OFF_2_16_SP, OFF_3_16_SP, OFF_4_16_SP, OFF_5_16_SP, OFF_0_9_SP,  OFF_0_8_SP,  OFF_1_16_SP, OFF_2_8_SP,  OFF_3_8_SP,  OFF_4_8_SP,  OFF_5_8_SP, OFF_0_1_SP,  OFF_0_0_SP,  OFF_1_0_SP,  OFF_2_0_SP,  OFF_3_0_SP,  OFF_4_0_SP,  OFF_5_0_SP

                RESTORE_OFF_END it3_end, 0x3e + 0x51 * 256
                jp it2_start

draw_off_rastr_1
                UPDATE_SP OFF_0_16_SP, OFF_1_16_SP, OFF_2_16_SP, OFF_3_16_SP, OFF_4_16_SP, OFF_5_16_SP, OFF_6_16_SP, OFF_0_8_SP,  OFF_1_16_SP, OFF_2_8_SP,  OFF_3_8_SP,  OFF_4_8_SP,  OFF_5_8_SP,  OFF_6_8_SP, OFF_0_0_SP,  OFF_1_0_SP,  OFF_2_0_SP,  OFF_3_0_SP,  OFF_4_0_SP,  OFF_5_0_SP,  OFF_6_0_SP

                RESTORE_OFF_END it2_end, 0x3e + 0x50 * 256
                jp it2_start
