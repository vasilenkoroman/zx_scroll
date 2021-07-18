        MACRO wait_ticks N
                IF N >= 32

.loop_counter equ (N - 10 - 12) / 24
                        ld hl, .loop_counter            ; 10 ticks
                        IF .loop_counter > 0
.loop:
                                dec hl                  ; 6 ticks
                                ld a, h                 ; 4 ticks
                                or l                    ; 4 ticks
                                JP nz, .loop            ; 10 ticks
                        ENDIF

.ticks_rest equ N - 10 - .loop_counter * 24
                        IF .ticks_rest >= 32
                                nop
                        ENDIF
                        IF .ticks_rest >= 28
                                nop
                        ENDIF
                        IF .ticks_rest >= 24
                                nop
                        ENDIF
                        IF .ticks_rest >= 20
                                nop
                        ENDIF
                        IF .ticks_rest >= 16
                                nop
                        ENDIF
                        IF .ticks_rest >= 12
                                nop
                        ENDIF
.ticks_rest2 equ 8 + (.ticks_rest % 4)
                        IF .ticks_rest2 == 8
                                nop
                                nop
                        ELSEIF .ticks_rest2 == 9
                                ld a, i
                        ELSEIF .ticks_rest2 == 10
                                dec hl
                                nop
                        ELSEIF .ticks_rest2 == 11
                                add hl, hl
                        ENDIF
                ELSE
                        ASSERT 0 ; // ticks < 32 not implemented now
                ENDIF
        ENDM
