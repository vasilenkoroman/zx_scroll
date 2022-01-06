buffer_data equ font_data - 256
buffer_data2 equ font_data - 512


;   Render text in the middle of attributes
;   a   -   char code
print_ch

    ld hl, font_data
    ld bc, buffer_data

    // de - font data ptr
    // hl - helper table
    // hl' - buffer ptr
    // bc' - const 32

    // Calculate symbol address to draw
    add a
    add a
    add a
    ld e, a
    adc h
    sub e
    ld d, a
    ld bc,8*256+3

draw_char
    ld a, (de) // load font byte
    and c
    ld l, a
    rrca
    ld (de),a   // update font data for the next step
    ld a,(hl)   // color attribute value to draw
    exx
    ld (hl),a   // save data in buffer
    add hl, bc
    exx
    inc de
    djnz draw_char
    // total draw_char: 7+4+4+4+7+7+4+7+11+4+6+13=78
    ret


copy_buffer
    ;   a - cyclic buffer offset [0..31]
    
    ld h, high(buffer_data)
    ld l, a
    ld de, #5900
    ld bc, 8*256+32

    add a
    add copy_buffer_bytes - buffer_right
    ld (buffer_right+1),a

    ld a,c
    sub l
    add a
    add copy_buffer_bytes - buffer_left
    ld (buffer_left+1),a

copy_buffer_line    
    ld ix, $+6
buffer_right
    jr copy_buffer_bytes

    ld a, l
    sub c
    ld l, a

    ld ix, $+6
buffer_left
    jr copy_buffer_bytes
    ld a, l
    add c
    ld l, a
    djnz copy_buffer_line
    ret

copy_buffer_bytes
    .32 ldi
    jp ix


/*
    exx
    ld b, 8
copy_buffer_line
    exx
buffer_right
    ld bc, 0
    ldir
    ld a, l
    sub 32
    ld l, a
buffer_left    
    ld bc,0
    ldir
    exx
    djnz copy_buffer_line
*/
