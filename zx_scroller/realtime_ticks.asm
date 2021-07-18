        MACRO delay   
        sub     36 + 9 + 19                                 ; 7         
        ld      h, high($)                                  ; 7         14
        jr      nc,wait2                                    ; 12/7      21
        ld      l,a                                         ; 4         25
        ld      l,(hl)                                      ; 7         32
        jp      (hl)                                        ; 4         36
wait2   sub 19                                              ; 4         30
        jr      nc,wait2                                    ; 12/7      37
        ld      l,a                                         ; 4         41
        ld      l,(hl)                                      ; 7         48
        jp      (hl)                                        ; 4         52
        
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
        ENDM


/*************** Data segment ******************/

        IF (low($) <= 237)
                org high($) * 256 + 237
        ELSE
                assert(0)
        ENDIF        

data_segment_start:
table
        ASSERT(low(table) == 237)
        db      low(t09), low(t10), low(t11), low(t12)
        db      low(t13), low(t14), low(t15), low(t16)
        db      low(t17), low(t18), low(t19), low(t20)
        db      low(t21), low(t22), low(t23), low(t24)
        db      low(t25), low(t26), low(t27)

