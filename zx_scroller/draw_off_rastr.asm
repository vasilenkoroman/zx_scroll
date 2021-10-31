    MACRO DRAW_ONLY_RASTR_LINE N?
                ld sp, 16384 + ((N? + 8) % 24) * 256 + 256       ; 10
                ld hl, $ + 7
                exx
RASTR0_N?       jp 00 ; rastr for multicolor ( up to 8 lines)       
    ENDM                


        MACRO DRAW_OFFSCREEN_LINES Step?, Iteration?
OFF_Step?_Iteration?_SP
                ld sp, 00        
                ld hl, $ + 7     ; 10
                exx
OFF_Step?_Iteration?_JP    
                jp 00 ; rastr for multicolor ( up to 8 lines)           ; 10
                // total ticks: 24
        ENDM          


draw_off_rastr_0
                ld a, 0x54
                out (0xfd), a
                scf
                DRAW_OFFSCREEN_LINES 0, 23
                DRAW_OFFSCREEN_LINES 0, 22
                DRAW_OFFSCREEN_LINES 0, 15
                DRAW_OFFSCREEN_LINES 0, 14
                DRAW_OFFSCREEN_LINES 0, 7
                DRAW_OFFSCREEN_LINES 0, 6

                ld a, 0x53
                out (0xfd), a

                DRAW_OFFSCREEN_LINES 0, 21
                DRAW_OFFSCREEN_LINES 0, 20
                DRAW_OFFSCREEN_LINES 0, 13
                DRAW_OFFSCREEN_LINES 0, 12
                DRAW_OFFSCREEN_LINES 0, 5
                DRAW_OFFSCREEN_LINES 0, 4

                ld a, 0x51
                out (0xfd), a

                DRAW_OFFSCREEN_LINES 0, 19
                DRAW_OFFSCREEN_LINES 0, 18
                DRAW_OFFSCREEN_LINES 0, 11
                DRAW_OFFSCREEN_LINES 0, 10
                DRAW_OFFSCREEN_LINES 0, 3
                DRAW_OFFSCREEN_LINES 0, 2

                ld a, 0x50
                out (0xfd), a

                DRAW_OFFSCREEN_LINES 0, 17
                DRAW_OFFSCREEN_LINES 0, 16
                DRAW_OFFSCREEN_LINES 0, 9
                DRAW_OFFSCREEN_LINES 0, 8
                DRAW_OFFSCREEN_LINES 0, 1
                DRAW_OFFSCREEN_LINES 0, 0

                // MC part rastr

                DRAW_ONLY_RASTR_LINE 0
                DRAW_ONLY_RASTR_LINE 1
                DRAW_ONLY_RASTR_LINE 2
                DRAW_ONLY_RASTR_LINE 3
                DRAW_ONLY_RASTR_LINE 4
                DRAW_ONLY_RASTR_LINE 5
                DRAW_ONLY_RASTR_LINE 6
                DRAW_ONLY_RASTR_LINE 7
                
                DRAW_ONLY_RASTR_LINE 8
                DRAW_ONLY_RASTR_LINE 9
                DRAW_ONLY_RASTR_LINE 10
                DRAW_ONLY_RASTR_LINE 11
                DRAW_ONLY_RASTR_LINE 12
                DRAW_ONLY_RASTR_LINE 13
                DRAW_ONLY_RASTR_LINE 14
                DRAW_ONLY_RASTR_LINE 15

                DRAW_ONLY_RASTR_LINE 16
                DRAW_ONLY_RASTR_LINE 17
                DRAW_ONLY_RASTR_LINE 18
                DRAW_ONLY_RASTR_LINE 19
                DRAW_ONLY_RASTR_LINE 20
                DRAW_ONLY_RASTR_LINE 21
                DRAW_ONLY_RASTR_LINE 22

                jp bank_drawing_common

draw_off_rastr_1
                ld a, 0x50
                out (0xfd), a
                scf
                DRAW_OFFSCREEN_LINES 1, 23
                DRAW_OFFSCREEN_LINES 1, 16
                DRAW_OFFSCREEN_LINES 1, 15
                DRAW_OFFSCREEN_LINES 1, 8
                DRAW_OFFSCREEN_LINES 1, 7
                DRAW_OFFSCREEN_LINES 1, 0

                ld a, 0x54
                out (0xfd), a

                DRAW_OFFSCREEN_LINES 1, 22
                DRAW_OFFSCREEN_LINES 1, 21
                DRAW_OFFSCREEN_LINES 1, 14
                DRAW_OFFSCREEN_LINES 1, 13
                DRAW_OFFSCREEN_LINES 1, 6
                DRAW_OFFSCREEN_LINES 1, 5

                ld a, 0x53
                out (0xfd), a

                DRAW_OFFSCREEN_LINES 1, 20
                DRAW_OFFSCREEN_LINES 1, 19
                DRAW_OFFSCREEN_LINES 1, 12
                DRAW_OFFSCREEN_LINES 1, 11
                DRAW_OFFSCREEN_LINES 1, 4
                DRAW_OFFSCREEN_LINES 1, 3

                ld a, 0x51
                out (0xfd), a

                DRAW_OFFSCREEN_LINES 1, 18
                DRAW_OFFSCREEN_LINES 1, 17
                DRAW_OFFSCREEN_LINES 1, 10
                DRAW_OFFSCREEN_LINES 1, 9
                DRAW_OFFSCREEN_LINES 1, 2
                DRAW_OFFSCREEN_LINES 1, 1

                jp bank_drawing_common

draw_off_rastr_2
                ld a, 0x50
                out (0xfd), a
                scf
                DRAW_OFFSCREEN_LINES 2, 23
                DRAW_OFFSCREEN_LINES 2, 22
                DRAW_OFFSCREEN_LINES 2, 15
                DRAW_OFFSCREEN_LINES 2, 14
                DRAW_OFFSCREEN_LINES 2, 7
                DRAW_OFFSCREEN_LINES 2, 6

                ld a, 0x54
                out (0xfd), a

                DRAW_OFFSCREEN_LINES 2, 21
                DRAW_OFFSCREEN_LINES 2, 20
                DRAW_OFFSCREEN_LINES 2, 13
                DRAW_OFFSCREEN_LINES 2, 12
                DRAW_OFFSCREEN_LINES 2, 5
                DRAW_OFFSCREEN_LINES 2, 4

                ld a, 0x53
                out (0xfd), a

                DRAW_OFFSCREEN_LINES 2, 19
                DRAW_OFFSCREEN_LINES 2, 18
                DRAW_OFFSCREEN_LINES 2, 11
                DRAW_OFFSCREEN_LINES 2, 10
                DRAW_OFFSCREEN_LINES 2, 3
                DRAW_OFFSCREEN_LINES 2, 2

                ld a, 0x51
                out (0xfd), a

                DRAW_OFFSCREEN_LINES 2, 17
                DRAW_OFFSCREEN_LINES 2, 16
                DRAW_OFFSCREEN_LINES 2, 9
                DRAW_OFFSCREEN_LINES 2, 8
                DRAW_OFFSCREEN_LINES 2, 1
                DRAW_OFFSCREEN_LINES 2, 0

                jp bank_drawing_common

draw_off_rastr_3
                ld a, 0x51
                out (0xfd), a
                scf
                DRAW_OFFSCREEN_LINES 3, 23
                DRAW_OFFSCREEN_LINES 3, 16
                DRAW_OFFSCREEN_LINES 3, 15
                DRAW_OFFSCREEN_LINES 3, 8
                DRAW_OFFSCREEN_LINES 3, 7
                DRAW_OFFSCREEN_LINES 3, 0

                ld a, 0x50
                out (0xfd), a

                DRAW_OFFSCREEN_LINES 3, 22
                DRAW_OFFSCREEN_LINES 3, 21
                DRAW_OFFSCREEN_LINES 3, 14
                DRAW_OFFSCREEN_LINES 3, 13
                DRAW_OFFSCREEN_LINES 3, 6
                DRAW_OFFSCREEN_LINES 3, 5

                ld a, 0x54
                out (0xfd), a

                DRAW_OFFSCREEN_LINES 3, 20
                DRAW_OFFSCREEN_LINES 3, 19
                DRAW_OFFSCREEN_LINES 3, 12
                DRAW_OFFSCREEN_LINES 3, 11
                DRAW_OFFSCREEN_LINES 3, 4
                DRAW_OFFSCREEN_LINES 3, 3

                ld a, 0x53
                out (0xfd), a

                DRAW_OFFSCREEN_LINES 3, 18
                DRAW_OFFSCREEN_LINES 3, 17
                DRAW_OFFSCREEN_LINES 3, 10
                DRAW_OFFSCREEN_LINES 3, 9
                DRAW_OFFSCREEN_LINES 3, 2
                DRAW_OFFSCREEN_LINES 3, 1

                jp bank_drawing_common

draw_off_rastr_4
                ld a, 0x51
                out (0xfd), a
                scf
                DRAW_OFFSCREEN_LINES 4, 23
                DRAW_OFFSCREEN_LINES 4, 22
                DRAW_OFFSCREEN_LINES 4, 15
                DRAW_OFFSCREEN_LINES 4, 14
                DRAW_OFFSCREEN_LINES 4, 7
                DRAW_OFFSCREEN_LINES 4, 6

                ld a, 0x50
                out (0xfd), a

                DRAW_OFFSCREEN_LINES 4, 21
                DRAW_OFFSCREEN_LINES 4, 20
                DRAW_OFFSCREEN_LINES 4, 13
                DRAW_OFFSCREEN_LINES 4, 12
                DRAW_OFFSCREEN_LINES 4, 5
                DRAW_OFFSCREEN_LINES 4, 4

                ld a, 0x54
                out (0xfd), a

                DRAW_OFFSCREEN_LINES 4, 19
                DRAW_OFFSCREEN_LINES 4, 18
                DRAW_OFFSCREEN_LINES 4, 11
                DRAW_OFFSCREEN_LINES 4, 10
                DRAW_OFFSCREEN_LINES 4, 3
                DRAW_OFFSCREEN_LINES 4, 2

                ld a, 0x53
                out (0xfd), a

                DRAW_OFFSCREEN_LINES 4, 17
                DRAW_OFFSCREEN_LINES 4, 16
                DRAW_OFFSCREEN_LINES 4, 9
                DRAW_OFFSCREEN_LINES 4, 8
                DRAW_OFFSCREEN_LINES 4, 1
                DRAW_OFFSCREEN_LINES 4, 0

                jp bank_drawing_common

draw_off_rastr_5
                ld a, 0x53
                out (0xfd), a
                scf
                DRAW_OFFSCREEN_LINES 5, 23
                DRAW_OFFSCREEN_LINES 5, 16
                DRAW_OFFSCREEN_LINES 5, 15
                DRAW_OFFSCREEN_LINES 5, 8
                DRAW_OFFSCREEN_LINES 5, 7
                DRAW_OFFSCREEN_LINES 5, 0

                ld a, 0x51
                out (0xfd), a

                DRAW_OFFSCREEN_LINES 5, 22
                DRAW_OFFSCREEN_LINES 5, 21
                DRAW_OFFSCREEN_LINES 5, 14
                DRAW_OFFSCREEN_LINES 5, 13
                DRAW_OFFSCREEN_LINES 5, 6
                DRAW_OFFSCREEN_LINES 5, 5

                ld a, 0x50
                out (0xfd), a

                DRAW_OFFSCREEN_LINES 5, 20
                DRAW_OFFSCREEN_LINES 5, 19
                DRAW_OFFSCREEN_LINES 5, 12
                DRAW_OFFSCREEN_LINES 5, 11
                DRAW_OFFSCREEN_LINES 5, 4
                DRAW_OFFSCREEN_LINES 5, 3

                ld a, 0x54
                out (0xfd), a

                DRAW_OFFSCREEN_LINES 5, 18
                DRAW_OFFSCREEN_LINES 5, 17
                DRAW_OFFSCREEN_LINES 5, 10
                DRAW_OFFSCREEN_LINES 5, 9
                DRAW_OFFSCREEN_LINES 5, 2
                DRAW_OFFSCREEN_LINES 5, 1

                jp bank_drawing_common

draw_off_rastr_6
                ld a, 0x53
                out (0xfd), a
                scf
                DRAW_OFFSCREEN_LINES 6, 23
                DRAW_OFFSCREEN_LINES 6, 22
                DRAW_OFFSCREEN_LINES 6, 15
                DRAW_OFFSCREEN_LINES 6, 14
                DRAW_OFFSCREEN_LINES 6, 7
                DRAW_OFFSCREEN_LINES 6, 6

                ld a, 0x51
                out (0xfd), a

                DRAW_OFFSCREEN_LINES 6, 21
                DRAW_OFFSCREEN_LINES 6, 20
                DRAW_OFFSCREEN_LINES 6, 13
                DRAW_OFFSCREEN_LINES 6, 12
                DRAW_OFFSCREEN_LINES 6, 5
                DRAW_OFFSCREEN_LINES 6, 4

                ld a, 0x50
                out (0xfd), a

                DRAW_OFFSCREEN_LINES 6, 19
                DRAW_OFFSCREEN_LINES 6, 18
                DRAW_OFFSCREEN_LINES 6, 11
                DRAW_OFFSCREEN_LINES 6, 10
                DRAW_OFFSCREEN_LINES 6, 3
                DRAW_OFFSCREEN_LINES 6, 2

                ld a, 0x54
                out (0xfd), a

                DRAW_OFFSCREEN_LINES 6, 17
                DRAW_OFFSCREEN_LINES 6, 16
                DRAW_OFFSCREEN_LINES 6, 9
                DRAW_OFFSCREEN_LINES 6, 8
                DRAW_OFFSCREEN_LINES 6, 1
                DRAW_OFFSCREEN_LINES 6, 0

                jp bank_drawing_common

draw_off_rastr_7
                ld a, 0x54
                out (0xfd), a
                scf
                DRAW_OFFSCREEN_LINES 7, 23
                DRAW_OFFSCREEN_LINES 7, 16
                DRAW_OFFSCREEN_LINES 7, 15
                DRAW_OFFSCREEN_LINES 7, 8
                DRAW_OFFSCREEN_LINES 7, 7
                DRAW_OFFSCREEN_LINES 7, 0

                ld a, 0x53
                out (0xfd), a

                DRAW_OFFSCREEN_LINES 7, 22
                DRAW_OFFSCREEN_LINES 7, 21
                DRAW_OFFSCREEN_LINES 7, 14
                DRAW_OFFSCREEN_LINES 7, 13
                DRAW_OFFSCREEN_LINES 7, 6
                DRAW_OFFSCREEN_LINES 7, 5

                ld a, 0x51
                out (0xfd), a

                DRAW_OFFSCREEN_LINES 7, 20
                DRAW_OFFSCREEN_LINES 7, 19
                DRAW_OFFSCREEN_LINES 7, 12
                DRAW_OFFSCREEN_LINES 7, 11
                DRAW_OFFSCREEN_LINES 7, 4
                DRAW_OFFSCREEN_LINES 7, 3

                ld a, 0x50
                out (0xfd), a

                DRAW_OFFSCREEN_LINES 7, 18
                DRAW_OFFSCREEN_LINES 7, 17
                DRAW_OFFSCREEN_LINES 7, 10
                DRAW_OFFSCREEN_LINES 7, 9
                DRAW_OFFSCREEN_LINES 7, 2
                DRAW_OFFSCREEN_LINES 7, 1

                jp bank_drawing_common
