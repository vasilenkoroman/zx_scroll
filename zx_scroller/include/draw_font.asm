buffer_data     equ #C000 + 6144 - 512
buffer_data2    equ #C000 + 6144 - 256

;   Render text in the middle of attributes
;   a   -   char code
print_ch

            // de - font data ptr
            // hl' - buffer ptr
            // bc' - const 32
            // de' - const 7,~7

            // Calculate symbol address to draw
            add a
            add a
            add a
            ld e, a
            adc high(font_data)
            sub e
            ld d, a

            exx
            ld bc,32
            ld de, 7 * 256 + low(~7)

buffer_pos  ld a, 0

            ld h, high(buffer_data)
            ld l, a
            srl l
            jr nc, buffer0
            inc h
buffer0     exx

            ld bc,8*256 + 3

            and 7
            jr z, render_single_point

draw_char
            ld a, (de) // load font byte
            rlca
            ld (de),a   // update font data for the next step
            rlca
            and c
            ld l, a
            ld h,high(font_data)    ; hl - helper table with paper/inc values
            ld a,(hl)   // color attribute value to draw
            exx
            ld (hl),a   // save data in buffer
            add hl, bc
            exx
            inc de
            djnz draw_char
            // total draw_char: 7+4+4+4+7+7+4+7+11+4+6+13=78
            ret

render_single_point
draw_char2
            ld a, (de) // load font byte
            inc de
            rla
            exx
            ld a,(hl)
            jr c, has_inc
            and e
            jp 1f
has_inc    
            or d
            ret c       ; 5 ticks. Align branch length
1           ld (hl),a
            add hl, bc
            exx
            djnz draw_char2
            // total draw_char: 7+6+4+4+7+7+4+10+7+11+4+13=84
            ret


copy_buffer
            ld a, (buffer_pos+1)
            ld h, high(buffer_data)
            ld l, a
            srl l
            jr nc, 1f
            inc h           ; switch to buffer 2
1           
            // update position
            inc a
            and #3f
            ld (buffer_pos+1),a

            inc l           ; a - head, l - tail
            res 5, l

            ld de, #d900        ; middle of the colors at page 7
            ld bc, 9*256


            ld a,32
            sub l
            add a
            add copy_buffer_bytes - buffer_left-2
            ld (buffer_left+1),a

            ld a,l
            add a
            add copy_buffer_bytes - buffer_right-2
            ld (buffer_right+1),a

copy_buffer_line    
    ld iy, $+6
buffer_right
    jr copy_buffer_bytes

    ld a, l
    sub 32
    ld l, a

    ld iy, $+6
buffer_left
    jr copy_buffer_bytes
    ld a, l
    add 32
    ld l, a
    djnz copy_buffer_line
    ret

copy_buffer_bytes
    .32 ldi
    jp iy


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

test_render_text

    // prepare data

    ld hl,#C000 + 6144 - 512
    ld de,#C001 + 6144 - 512
    ld bc, 768 + 512
    ld (hl),0
    ldir

    ld hl,#C000 + 2048
    ld de,#C001 + 2048
    ld bc, 2048
    ld (hl), #0f
    ldir
    halt

    // start drwing
1    
    ld a, 1 ; 'A'
    call print_ch
    call copy_buffer
    halt
    halt
    halt
    halt
    jr 1b

    ret
