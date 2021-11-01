        MACRO DRAW_OFFSCREEN_LINES Iteration?, Step?
OFF_Iteration?,Step?_SP
                ld sp, 00        
                ld hl, $ + 7     ; 10
                exx
OFF_Iteration?_Step?_JP    
                jp 00 ; rastr for multicolor ( up to 8 lines)           ; 10
                // total ticks: 24
        ENDM          

it7_upd:        ld hl, OFF_Iteration_0_6_SP + 1:    dec (hl)
                ld hl, OFF_Iteration_0_14_SP + 1:   dec (hl)
                ld hl, OFF_Iteration_0_22_SP + 1:   dec (hl)

it6_upd:        ld hl, OFF_Iteration_0_5_SP + 1:    dec (hl)
                ld hl, OFF_Iteration_0_13_SP + 1:   dec (hl)
                ld hl, OFF_Iteration_0_21_SP + 1:   dec (hl)

it5_upd:        ld hl, OFF_Iteration_0_4_SP + 1:    dec (hl)
                ld hl, OFF_Iteration_0_12_SP + 1:   dec (hl)
                ld hl, OFF_Iteration_0_20_SP + 1:   dec (hl)

it4_upd:        ld hl, OFF_Iteration_0_3_SP + 1:    dec (hl)
                ld hl, OFF_Iteration_0_11_SP + 1:   dec (hl)
                ld hl, OFF_Iteration_0_19_SP + 1:   dec (hl)

it3_upd:        ld hl, OFF_Iteration_0_2_SP + 1:    dec (hl)
                ld hl, OFF_Iteration_0_10_SP + 1:   dec (hl)
                ld hl, OFF_Iteration_0_18_SP + 1:   dec (hl)

it2_upd:        ld hl, OFF_Iteration_0_1_SP + 1:    dec (hl)
                ld hl, OFF_Iteration_0_9_SP + 1:    dec (hl)
                ld hl, OFF_Iteration_0_17_SP + 1:   dec (hl)

it1_upd:        ld hl, OFF_Iteration_0_0_SP + 1:    dec (hl)
                ld hl, OFF_Iteration_0_8_SP + 1:    dec (hl)
                ld hl, OFF_Iteration_0_16_SP + 1:   dec (hl)
                
it-1_upd:       ld hl, OFF_Iteration_-1_0_SP + 1:   dec (hl)
                ld hl, OFF_Iteration_-1_8_SP + 1:   dec (hl)
                ld hl, OFF_Iteration_-1_16_SP + 1:  dec (hl)

it-2_upd:       ld hl, OFF_Iteration_-2_0_SP + 1:   dec (hl)
                ld hl, OFF_Iteration_-2_8_SP + 1:   dec (hl)
                ld hl, OFF_Iteration_-2_16_SP + 1:  dec (hl)

it-3_upd:       ld hl, OFF_Iteration_-3_0_SP + 1:   dec (hl)
                ld hl, OFF_Iteration_-3_8_SP + 1:   dec (hl)
                ld hl, OFF_Iteration_-3_16_SP + 1:  dec (hl)

it-4_upd:       ld hl, OFF_Iteration_-4_0_SP + 1:   dec (hl)
                ld hl, OFF_Iteration_-4_8_SP + 1:   dec (hl)
                ld hl, OFF_Iteration_-4_16_SP + 1:  dec (hl)

it-5_upd:       ld hl, OFF_Iteration_-5_0_SP + 1:   dec (hl)
                ld hl, OFF_Iteration_-5_8_SP + 1:   dec (hl)
                ld hl, OFF_Iteration_-5_16_SP + 1:  dec (hl)

it-6_upd:       ld hl, OFF_Iteration_-6_0_SP + 1:   dec (hl)
                ld hl, OFF_Iteration_-6_8_SP + 1:   dec (hl)
                ld hl, OFF_Iteration_-6_16_SP + 1:  dec (hl)

                jp it1_start

it0_start:      ld a, 0x54
                out (0xfd), a
                DRAW_OFFSCREEN_LINES 0, 23
                DRAW_OFFSCREEN_LINES 0, 15
                DRAW_OFFSCREEN_LINES 0, 7

it7_start:      ld a, 0x54
                out (0xfd), a
                DRAW_OFFSCREEN_LINES 0, 22
                DRAW_OFFSCREEN_LINES 0, 14
                DRAW_OFFSCREEN_LINES 0, 6

it6_start:      ld a, 0x53
                out (0xfd), a
                DRAW_OFFSCREEN_LINES 0, 21
                DRAW_OFFSCREEN_LINES 0, 13
                DRAW_OFFSCREEN_LINES 0, 5

it5_start:      ld a, 0x53
                out (0xfd), a
                DRAW_OFFSCREEN_LINES 0, 20
                DRAW_OFFSCREEN_LINES 0, 12
                DRAW_OFFSCREEN_LINES 0, 4

it4_start:      ld a, 0x51
                out (0xfd), a
                DRAW_OFFSCREEN_LINES 0, 19
                DRAW_OFFSCREEN_LINES 0, 11
                DRAW_OFFSCREEN_LINES 0, 3

it3_start:      ld a, 0x51
                out (0xfd), a
                DRAW_OFFSCREEN_LINES 0, 18
                DRAW_OFFSCREEN_LINES 0, 10
                DRAW_OFFSCREEN_LINES 0, 2

it2_start:      ld a, 0x50
                out (0xfd), a
                DRAW_OFFSCREEN_LINES 0, 17
                DRAW_OFFSCREEN_LINES 0, 9
                DRAW_OFFSCREEN_LINES 0, 1

it1_start:      ld a, 0x50
                out (0xfd), a
                DRAW_OFFSCREEN_LINES 0, 16
                DRAW_OFFSCREEN_LINES 0, 8
                DRAW_OFFSCREEN_LINES 0, 0

                // negative values (next 7 steps)

it0_end:        ld a, 0x54
                out (0xfd), a
                DRAW_OFFSCREEN_LINES -1, 16
                DRAW_OFFSCREEN_LINES -1, 8
                DRAW_OFFSCREEN_LINES -1, 0

it7_end:        ld a, 0x54
                out (0xfd), a
                DRAW_OFFSCREEN_LINES -2, 16
                DRAW_OFFSCREEN_LINES -2, 8
                DRAW_OFFSCREEN_LINES -2, 0

it6_end:        ld a, 0x53
                out (0xfd), a
                DRAW_OFFSCREEN_LINES -3, 16
                DRAW_OFFSCREEN_LINES -3, 8
                DRAW_OFFSCREEN_LINES -3, 0

it5_end:        ld a, 0x53
                out (0xfd), a
                DRAW_OFFSCREEN_LINES -4, 16
                DRAW_OFFSCREEN_LINES -4, 8
                DRAW_OFFSCREEN_LINES -4, 0

it4_end:        ld a, 0x51
                out (0xfd), a
                DRAW_OFFSCREEN_LINES -5, 16
                DRAW_OFFSCREEN_LINES -5, 8
                DRAW_OFFSCREEN_LINES -5, 0

it3_end:        ld a, 0x51
                out (0xfd), a
                DRAW_OFFSCREEN_LINES -6, 16
                DRAW_OFFSCREEN_LINES -6, 8
                DRAW_OFFSCREEN_LINES -6, 0

it2_end:        ld a, 0x50
                out (0xfd), a
                DRAW_OFFSCREEN_LINES -7, 16
                DRAW_OFFSCREEN_LINES -7, 8
                DRAW_OFFSCREEN_LINES -7, 0

it1_end:        jp bank_drawing_common


draw_off_rastr_7
                pop hl: ld (OFF_-1_0), hl
                add hl, de: ld sp, hl
                pop hl: ld (OFF_-1_8), hl
                add hl, de: ld sp, hl
                pop hl: ld (OFF_-1_16), hl

                ld de, $ + 6
                jp it7_upd
                ld de, $ + 6
                jp it7_start

                RESTORE_UPD_END it-2_upd, 7
                RESTORE_OFF_END it7_end, 7
                jp bank_drawing_common

draw_off_rastr_6
                pop hl: ld (OFF_-2_0), hl
                add hl, de: ld sp, hl
                pop hl: ld (OFF_-2_8), hl
                add hl, de: ld sp, hl
                pop hl: ld (OFF_-2_16), hl

                ld de, $ + 6
                jp it6_upd
                ld de, $ + 6
                jp it6_start

                RESTORE_UPD_END it-3_upd, 6
                RESTORE_OFF_END it6_end, 6
                jp bank_drawing_common

draw_off_rastr_5
                pop hl: ld (OFF_-3_0), hl
                add hl, de: ld sp, hl
                pop hl: ld (OFF_-3_8), hl
                add hl, de: ld sp, hl
                pop hl: ld (OFF_-3_16), hl

                ld de, $ + 6
                jp it5_upd
                ld de, $ + 6
                jp it5_start

                RESTORE_UPD_END it-4_upd, 5
                RESTORE_OFF_END it5_end, 5
                jp bank_drawing_common

draw_off_rastr_4
                pop hl: ld (OFF_-4_0), hl
                add hl, de: ld sp, hl
                pop hl: ld (OFF_-4_8), hl
                add hl, de: ld sp, hl
                pop hl: ld (OFF_-4_16), hl

                ld de, $ + 6
                jp it4_upd
                ld de, $ + 6
                jp it4_start

                RESTORE_UPD_END it-5_upd, 4
                RESTORE_OFF_END it4_end, 4
                jp bank_drawing_common

draw_off_rastr_3
                pop hl: ld (OFF_-5_0), hl
                add hl, de: ld sp, hl
                pop hl: ld (OFF_-5_8), hl
                add hl, de: ld sp, hl
                pop hl: ld (OFF_-5_16), hl

                ld de, $ + 6
                jp it3_upd
                ld de, $ + 6
                jp it3_start

                RESTORE_UPD_END it-6_upd, 3
                RESTORE_OFF_END it3_end, 3
                jp bank_drawing_common

draw_off_rastr_2
                pop hl: ld (OFF_-6_0), hl
                add hl, de: ld sp, hl
                pop hl: ld (OFF_-6_8), hl
                add hl, de: ld sp, hl
                pop hl: ld (OFF_-6_16), hl

                ld de, $ + 6
                jp it2_upd
                ld de, $ + 6
                jp it2_start

                RESTORE_UPD_END it-7_upd, 2
                RESTORE_OFF_END it2_end, 2
                jp bank_drawing_common

draw_off_rastr_1
                pop hl: ld (OFF_-7_0), hl
                add hl, de: ld sp, hl
                pop hl: ld (OFF_-7_8), hl
                add hl, de: ld sp, hl
                pop hl: ld (OFF_-7_16), hl

                ld de, $ + 6
                jp it1_upd
                ld de, bank_drawing_common
                jp it1_start
