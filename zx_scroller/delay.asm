        MODULE	Delay
        
        MACRO delayFrom71
                ; HL: ticks to wait. min value: 71
                ; macro size 74 bytes
delay           xor     a               ; 4
                or      h               ; 4     8
                jr      nz, wait1       ; 7/12  15/20
                ld      a,l             ; 4     19

offset          EQU (256-table_size) - low(table)
                ld      hl, high(table)*256 + offset                ; 7         26
                sub     62 + 9 + table_size                         ; 10        36      
                jr      nc,wait2                                    ; 12/7      43
                sub     l                                           ; 7         47
                ld      l,a                                         ; 4         51
                ld      l,(hl)                                      ; 7         58
                jp      (hl)                                        ; 4         62
                ; min 62 + t09 = 71t
wait2           sub 19                                              ; 7         
                jr      nc,wait2                                    ; 12        19      
                sub     l
                ld      l,a                                         
                ld      l,(hl)                                      
                jp      (hl)                                        

wait1           ld      de, -(20 + 33)          ; 10
                add     hl,de                   ; 11    21
                jr      delay                   ; 12    33

table
        db      low(t09), low(t10), low(t11), low(t12)
        db      low(t13), low(t14), low(t15), low(t16)
        db      low(t17), low(t18), low(t19), low(t20)
        db      low(t21), low(t22), low(t23), low(t24)
        db      low(t25), low(t26), low(t27)
table_end
table_size      equ table_end - table

t24             nop
t20             nop
t16             nop
t12             jr delay_end                    ; 12

t27             nop
t23             nop
t19             nop
t15             nop
t11             ld l, low(delay_end)
                jp (hl)                         ; 11

t26             nop
t22             nop
t18             nop
t14             nop
t10             jp delay_end                    ; 10

t25             nop
t21             nop
t17             nop
t13             nop
t09             ld a, r                         ;9
delay_end
                ASSERT high(delay_end) == high(table)
        ENDM

        MACRO delayFrom70
                ; HL: ticks to wait. min value: 70
                ; macro size 91 bytes
                ld a, low(table_end)            ; 7     
                ld de, -(70 + table_size)       ; 10    17
                add hl, de                      ; 11    28
                jr c, wait                      ; 7     35

                ld h, high(table)               ; 7     42
                add     l                       ; 4     46                   
                ld      l,a                     ; 4     50                   
                ld      l,(hl)                  ; 7     57                                   
                jp      (hl)                    ; 4     61              

wait            ld  e, -30                      ; 7    
                add hl, de                      ; 11    18
                jr c, wait                      ; 12    30

                ld h, high(table)               ; 7     
                add     l                       ; 4                        
                ld      l,a                     ; 4                        
                ld      l,(hl)                  ; 7                                        
                jp      (hl)                    ; 4                   
table
        db      low(t09), low(t10), low(t11), low(t12)
        db      low(t13), low(t14), low(t15), low(t16)
        db      low(t17), low(t18), low(t19), low(t20)
        db      low(t21), low(t22), low(t23), low(t24)
        db      low(t25), low(t26), low(t27), low(t28)
        db      low(t29), low(t30), low(t31), low(t32)
        db      low(t33), low(t34), low(t35), low(t36)
        db      low(t37), low(t38)

table_end
table_size      equ table_end - table

t36             nop
t32             nop
t28             nop
t24             nop
t20             nop
t16             nop
t12             jr delay_end                    ; 12

t35             nop
t31             nop
t27             nop
t23             nop
t19             nop
t15             nop
t11             ld l, low(delay_end)
                jp (hl)                         ; 11

t38             nop
t34             nop
t30             nop
t26             nop
t22             nop
t18             nop
t14             nop
t10             jp delay_end                    ; 10

t37             nop
t33             nop
t29             nop
t25             nop
t21             nop
t17             nop
t13             nop
t09             ld a, r                         ;9
delay_end

                ASSERT high(delay_end) == high(table)
        ENDM

        MACRO delayFrom59
                ; HL: ticks to wait. min value: 59
                ; macro size 87 + align bytes
delay_start                
table_size      equ 30
                ld de, -(59 + table_size)       ; 10    
                add hl, de                      ; 11    21
                jr c, wait                      ; 7     28

                ld h, high(table)               ; 7     35
                ld l,(hl)                       ; 7     42                                   
                inc h                           ; 4     46
                jp (hl)                         ; 4     50              

wait            ld  e, -30                      ; 7    
                add hl, de                      ; 11    18
                jr c, wait                      ; 12    30

                ld h, high(table)               ; 7     
                ld l,(hl)                       ; 7       
                inc h                           ; 4                                 
                jp  (hl)                        ; 4                   

        IF (low($) <= 256 - table_size)
                DISPLAY "losed bytes on delay align: ", /D, 256 - table_size - low($) 
                defs 256 - table_size - low($), 0
        ELSE
                DISPLAY "not enough bytes at this 256 block: ", /D, low($) - (256 - table_size)
                assert(0)
        ENDIF        

table
        db      low(t09), low(t10), low(t11), low(t12)
        db      low(t13), low(t14), low(t15), low(t16)
        db      low(t17), low(t18), low(t19), low(t20)
        db      low(t21), low(t22), low(t23), low(t24)
        db      low(t25), low(t26), low(t27), low(t28)
        db      low(t29), low(t30), low(t31), low(t32)
        db      low(t33), low(t34), low(t35), low(t36)
        db      low(t37), low(t38)

t36             nop
t32             nop
t28             nop
t24             nop
t20             nop
t16             nop
t12             jr delay_end                    ; 12

t35             nop
t31             nop
t27             nop
t23             nop
t19             nop
t15             nop
t11             ld l, low(delay_end)
                jp (hl)                         ; 11

t38             nop
t34             nop
t30             nop
t26             nop
t22             nop
t18             nop
t14             nop
t10             jp delay_end                    ; 10

t37             nop
t33             nop
t29             nop
t25             nop
t21             nop
t17             nop
t13             nop
t09             ld a, r                         ;9
delay_end
                DISPLAY "Delay macro size: ", /D, delay_end - delay_start 
        ENDM

        ENDMODULE
