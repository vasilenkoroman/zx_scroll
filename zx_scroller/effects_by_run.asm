    // Effects by run (perform on step 0)

effect0
    ld a, #3e                   ; make JR xx to LD A, XX
    ld (check_for_page7_effect),a
    jp loop

effect1
    ld a, #38                   ; JR C
    ld (check_for_page7_effect),a
    jp loop

effect2
    ld a, #38                   ; JR C
    ld (check_for_page7_effect),a
    jp loop

effect3
    ld a, #3e                   ; make JR xx to LD A, XX
    ld (check_for_page7_effect),a
    jp loop

    // Effects inside run
effect_step

    jp after_draw_colors                            
