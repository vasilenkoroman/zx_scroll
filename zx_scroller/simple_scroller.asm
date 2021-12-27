    MACRO down_hl
down_hl     inc h
            ld a,h
            and 7
            jr nz, next ; It is slow, but we don't hurry here. Used for 25fps scrool only.
            ld a,l
            sub -32
            ld l,a
            sbc a
            and #f8
            add h
            ld h,a
next        ; 27/56            
    ENDM

copy_image
            ; hl - source bitmap (full screen width)
            ; b - screen line number to draw

            // calculate screen address
            ld a,b
            and 7
            or #40
            ld d,a
            ld a,b
            rra: rra: rra
            and #18
            or d
            cp #58
            ret z
            ld d,a

            ld a,b
            rla: rla
            and #e0
            ld e, a

next_line
            ld bc, 32
            push hl
line_loop
            ldi
            ldi
            jr nz, line_loop
            pop hl

            down_hl
            jr next_line
