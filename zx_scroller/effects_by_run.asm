effect0
    ld a, #18                   ; JR xx
    ld (before_odd_step),a
    jp loop

effect1
    ld a, #30                   ; JR NC
    ld (before_odd_step),a
    jp loop

effect2
    ld a, #30                   ; JR NC
    ld (before_odd_step),a
    jp loop

effect3
    ld a, #18                   ; JR xx
    ld (before_odd_step),a
    jp loop
