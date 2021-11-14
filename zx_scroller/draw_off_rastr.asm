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
                DRAW_OFFSCREEN_LINES 0, 23,  it0_start
L01:            DRAW_OFFSCREEN_LINES 0, 15,  L01
L02:            DRAW_OFFSCREEN_LINES   0, 7, L02

it7_start:      //ld a, 0x54
                //out (0xfd), a
                DRAW_OFFSCREEN_LINES 0, 22, it7_start
L11:            DRAW_OFFSCREEN_LINES 0, 14, L11
L12:            DRAW_OFFSCREEN_LINES 0, 6,  L12

it6_start:      ld a, 0x53
                out (0xfd), a
                DRAW_OFFSCREEN_LINES 0, 21, it6_start
L21:            DRAW_OFFSCREEN_LINES 0, 13, L21
L22:            DRAW_OFFSCREEN_LINES 0, 5,  L22

it5_start:      //ld a, 0x53
                //out (0xfd), a
                DRAW_OFFSCREEN_LINES 0, 20, it5_start
L31:            DRAW_OFFSCREEN_LINES 0, 12, L31
L32:            DRAW_OFFSCREEN_LINES 0, 4, L32

it4_start:      ld a, 0x51
                out (0xfd), a
                DRAW_OFFSCREEN_LINES 0, 19, it4_start
L41:            DRAW_OFFSCREEN_LINES 0, 11, L41
L42:            DRAW_OFFSCREEN_LINES 0, 3,  L42

it3_start:      //ld a, 0x51
                //out (0xfd), a
                DRAW_OFFSCREEN_LINES 0, 18, it3_start
L51:            DRAW_OFFSCREEN_LINES 0, 10, L51
L52:            DRAW_OFFSCREEN_LINES 0, 2,  L52

it2_start:      ld a, 0x50
                out (0xfd), a
                DRAW_OFFSCREEN_LINES 0, 17, it2_start
L61:            DRAW_OFFSCREEN_LINES 0, 9,  L61
L62:            DRAW_OFFSCREEN_LINES 0, 1,  L62

it1_start:      //ld a, 0x50
                //out (0xfd), a
                DRAW_OFFSCREEN_LINES 0, 16, it1_start
L71:            DRAW_OFFSCREEN_LINES 0, 8,  L71
L72:            DRAW_OFFSCREEN_LINES 0, 0,  L72

                // negative values (next 7 steps)

L80:            ld a, 0x54
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

L100            ld a, 0x53
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

L120:           ld a, 0x51
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

L140:           ld a, 0x50
it2_end:        out (0xfd), a
                DRAW_OFFSCREEN_LINES 7, 16, L140
L141:           DRAW_OFFSCREEN_LINES 7, 8,  L141
L142:           DRAW_OFFSCREEN_LINES 7, 0,  L142

it1_end:        ex de,hl
                jp hl


draw_off_rastr_7
                UPDATE_SP OFF_0_22_SP, OFF_0_21_SP, OFF_0_20_SP, OFF_0_19_SP, OFF_0_18_SP, OFF_0_17_SP, OFF_0_16_SP, OFF_0_14_SP, OFF_0_13_SP, OFF_0_12_SP, OFF_0_11_SP, OFF_0_10_SP, OFF_0_9_SP,  OFF_0_8_SP, OFF_0_6_SP,  OFF_0_5_SP,  OFF_0_4_SP,  OFF_0_3_SP,  OFF_0_2_SP,  OFF_0_1_SP,  OFF_0_0_SP

                RESTORE_OFF_END it0_end, 0xd3 + 0xfd * 256
                ld a, 0x54
                out (0xfd), a
                START_OFF_DRAWING it7_start

draw_off_rastr_6
                UPDATE_SP OFF_0_21_SP, OFF_0_20_SP, OFF_0_19_SP, OFF_0_18_SP, OFF_0_17_SP, OFF_0_16_SP, OFF_1_16_SP, OFF_0_13_SP, OFF_0_12_SP, OFF_0_11_SP, OFF_0_10_SP, OFF_0_9_SP,  OFF_0_8_SP,  OFF_1_8_SP,  OFF_0_5_SP,  OFF_0_4_SP,  OFF_0_3_SP,  OFF_0_2_SP,  OFF_0_1_SP,  OFF_0_0_SP,  OFF_1_0_SP

                RESTORE_OFF_END it7_end, 0x31 * 256
                START_OFF_DRAWING it6_start

draw_off_rastr_5
                UPDATE_SP OFF_0_20_SP, OFF_0_19_SP, OFF_0_18_SP, OFF_0_17_SP, OFF_0_16_SP, OFF_1_16_SP, OFF_2_16_SP, OFF_0_12_SP, OFF_0_11_SP, OFF_0_10_SP, OFF_0_9_SP,  OFF_0_8_SP,  OFF_1_8_SP, OFF_2_8_SP,  OFF_0_4_SP,  OFF_0_3_SP,  OFF_0_2_SP,  OFF_0_1_SP,  OFF_0_0_SP,  OFF_1_0_SP,  OFF_2_0_SP


                RESTORE_OFF_END it6_end, 0xd3 + 0xfd * 256
                ld a, 0x53
                out (0xfd), a
                START_OFF_DRAWING it5_start

draw_off_rastr_4
                UPDATE_SP OFF_0_19_SP, OFF_0_18_SP, OFF_0_17_SP, OFF_0_16_SP, OFF_1_16_SP, OFF_2_16_SP, OFF_3_16_SP, OFF_0_11_SP, OFF_0_10_SP, OFF_0_9_SP,  OFF_0_8_SP,  OFF_1_8_SP, OFF_2_8_SP,  OFF_3_8_SP,  OFF_0_3_SP,  OFF_0_2_SP,  OFF_0_1_SP,  OFF_0_0_SP,  OFF_1_0_SP,  OFF_2_0_SP,  OFF_3_0_SP

                RESTORE_OFF_END it5_end,  0x31 * 256
                START_OFF_DRAWING it4_start

draw_off_rastr_3
                UPDATE_SP OFF_0_18_SP, OFF_0_17_SP, OFF_0_16_SP, OFF_1_16_SP, OFF_2_16_SP, OFF_3_16_SP, OFF_4_16_SP, OFF_0_10_SP, OFF_0_9_SP,  OFF_0_8_SP,  OFF_1_8_SP, OFF_2_8_SP,  OFF_3_8_SP,  OFF_4_8_SP,  OFF_0_2_SP,  OFF_0_1_SP,  OFF_0_0_SP,  OFF_1_0_SP,  OFF_2_0_SP,  OFF_3_0_SP,  OFF_4_0_SP

                RESTORE_OFF_END it4_end, 0xd3 + 0xfd * 256
                ld a, 0x51
                out (0xfd), a
                START_OFF_DRAWING it3_start

draw_off_rastr_2
                UPDATE_SP OFF_0_17_SP, OFF_0_16_SP, OFF_1_16_SP, OFF_2_16_SP, OFF_3_16_SP, OFF_4_16_SP, OFF_5_16_SP, OFF_0_9_SP,  OFF_0_8_SP,  OFF_1_8_SP, OFF_2_8_SP,  OFF_3_8_SP,  OFF_4_8_SP,  OFF_5_8_SP, OFF_0_1_SP,  OFF_0_0_SP,  OFF_1_0_SP,  OFF_2_0_SP,  OFF_3_0_SP,  OFF_4_0_SP,  OFF_5_0_SP

                RESTORE_OFF_END it3_end, 0x31 * 256
                START_OFF_DRAWING it2_start

draw_off_rastr_1
                UPDATE_SP OFF_0_16_SP, OFF_1_16_SP, OFF_2_16_SP, OFF_3_16_SP, OFF_4_16_SP, OFF_5_16_SP, OFF_6_16_SP, OFF_0_8_SP,  OFF_1_8_SP, OFF_2_8_SP,  OFF_3_8_SP,  OFF_4_8_SP,  OFF_5_8_SP,  OFF_6_8_SP, OFF_0_0_SP,  OFF_1_0_SP,  OFF_2_0_SP,  OFF_3_0_SP,  OFF_4_0_SP,  OFF_5_0_SP,  OFF_6_0_SP

                RESTORE_OFF_END it2_end, 0xd3 + 0xfd * 256
                ld a, 0x50
                out (0xfd), a
                START_OFF_DRAWING it1_start
