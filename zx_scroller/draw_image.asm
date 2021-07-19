        INCLUDE "wait_ticks.asm"

        DEVICE zxspectrum48
        SLDOPT COMMENT WPMEM, LOGPOINT, ASSERTION //< More debug symbols.

screen_addr:    equ 16384
screen_size:    equ 6144
screen_end:     equ screen_addr + screen_size
//generated_code: equ 0x5b00 ; screen_end + 768
generated_code: equ 32768 ; screen_end + 768
                ASSERT generated_code % 256 == 0

/*************** Image data. ******************/

    org generated_code + image_data_len

image_data:
        INCBIN "resources/thanos.bin", 0, 6144
        INCBIN "resources/thanos.bin", 0, 6144
        //INCBIN "resources/image8.bin", 0, 6144
image_end:
color_data:
        INCBIN "resources/thanos.bin", 6144, 768
data_end:

image_data_len: equ image_end - image_data
generate_code_len: equ image_data_len * 2
        ASSERT generate_code_len % 2048 == 0
generate_code_line_len: equ 64
generate_code_interlive8_len: equ generate_code_len / 8
generate_code_interlive8_len_h: equ generate_code_interlive8_len / 256

/*************** Ennd image data. ******************/

JP_HL_CODE      equ 0e9h
LD_BC_XXXX_CODE equ 01h
LD_DE_XXXX_CODE equ 11h

        MACRO write_byte data
                ld (hl), data
                inc hl
        ENDM

/********************* mirror data ***********************/

interleave_data_block:
        // hl - destination address for generated code.
        // ix - source data

        ld b, 128
        
.rep:   ld de, (ix)
        inc ix
        inc ix
        write_byte e
        write_byte d

        djnz .rep
        ret

prepare_image_data:
        ld hl, image_data
        ld ix, image_data + image_data_len
        ld bc, image_data_len / 2
        
        ; mirror pixels in data
.mirror_loop
        dec ix
        ld d, (ix)
        ld e, (hl)
        ld (ix), e
        ld (hl), d
        inc hl

        dec bc
        ld a, b
        or c
        jr nz, .mirror_loop

        ; interliave data in the same way as for generated code
        ld hl, image_data
        ld ix, generated_code
        ld a, 8 ; banks count

        ld hl, generated_code
        ld ix, image_data
        ld b, image_data_len / (32 * 64) ; Data length in lines/64.

.interleave_loop
        ; generate 64 lines
        push ix
        push bc
.line_64_loop:
        push bc
        call interleave_data_block ; 8 lines (256 bytes)
        pop bc

        ld de,  2048 - (8 * 32)
        add ix, de
        djnz .line_64_loop ; draw 8 * N lines (N is whole data size in 3-th part of the screen)

        pop bc
        pop ix
        ld de, 8 * 32
        add ix, de ; next screen line  in the source image (skip 8 lines in bytes)
        dec a
        jr nz, .interleave_loop
        
        ; copy interleaved data back to the source buffer
        ld hl, generated_code
        ld de, image_data
        ld bc, image_data_len
        ldir

        ret

/** Generate code for while image.
 * It assumes the source image buffer has several images in ZX video format.
 * The destination code contains lines interleaved by 8. It remove interleaving by 64 lines 
 * (third part of screen interleaving).
 */
generate_data:
        ld hl, generated_code
        ld ix, image_data
        ld bc, image_data_len / 4

.rep:   ld de, (ix)
        inc ix
        inc ix

        write_byte LD_BC_XXXX_CODE  //< "ld bc, xxxx" instruction code.
        write_byte d
        write_byte e

        ld de, (ix)
        inc ix
        inc ix

        write_byte LD_DE_XXXX_CODE  //< "ld bc, xxxx" instruction code.
        write_byte d
        write_byte e

        write_byte 0c5h //< "push bc" instruction code.
        write_byte 0d5h //< "push de" instruction code.

        dec bc
        ld a, b
        or c
        jr nz, .rep
        ret

/*************** Draw 8 lines of image.  ******************/
        MACRO draw_8_lines
                // hl - descriptor
                // sp - destinatin screen address to draw

                // write return address to code
                ld (hl), d ;JP_HL_CODE          ; 7 ticks

                exx                             ; 4 ticks
                ld hl, $ + 5 ; return addr      ; 10 ticks
                jp ix ; draw                    ; 8 ticks
                exx                             ; 4 ticks
                // restore code back
                ld (hl), c; LD_BC_XXXX_CODE     ; 7 ticks

                // itself total 40 ticks per 8 lines, gen code = 21 * 16*8 + 4 = 2692
        ENDM

draw_4_lines_and_rt_colors:
                // hl - descriptor
                // sp - destinatin screen address to draw
                // iy - color address to draw

                ; bc - address to execute
                ld c, (hl)                                      ; 7
                inc l                                           ; 4
                ld b, (hl)                                      ; 7
                inc l                                           ; 4

                ; exec address end
                ld e, (hl)                                      ; 7
                inc l                                           ; 4
                ld d, (hl)                                      ; 7
                inc hl                                          ; 10
                ex de, hl       ; save descriptor to de         ; 4

                ; update generated code line
                ld (hl), JP_HL_CODE                             ; 11
                ld ix, bc                                       ; 16
                exx                                             ; 4
                ld hl, $ + 5    ; return address                ; 10
                ; free registers to use: bc, de, bc'.   hl -return addr, hl' - exec end addr to restore, de' - descriptor
                jp ix                                           ; 8
                exx                                             ; 10

                ; restore data
                ld (hl), LD_BC_XXXX_CODE                        ; 11
                ex de, hl                                       ; 4

                // total ticks: 126

                // draw RT colors (1 line)

                ld d, (hl)                                      ; 7
                inc l                                           ; 4
                ld e, (hl)                                      ; 7
                inc hl                                          ; 10

                ; save descriptor
                ld (stack_bottom), hl                           ; 16
                ; save stack
                ld (stack_bottom + 2), sp                       ; 20
                ; set stack to color attributes
                ld sp, iy                                       ; 10

                ex de, hl                                       ; 4
                ld ix, $ + 4    ; return address                ; 14
                jp hl                                           ; 4
                ; restore descriptor
                ld hl, (stack_bottom)                           ; 16
                ; restore stack
                ld sp, (stack_bottom + 2)                       ; 20

                // total ticks:118 for colors, total: 118 + 126 = 244


                jp iy      ; ret                                               ; 10 ticks


draw_64_lines_2:
                // sp - descriptor
                // de - destinatin screen address to draw

                ; bc - address to execute
                pop ix                                          ; 10

                ; hl - end execute address
                pop hl                                          ; 10

                ; save descriptor
                ld (stack_bottom + 2), sp                       ; 20

                ld (hl), JP_HL_CODE                             ; 11

                ex de, hl ; save end address in de              ; 4
                ld sp, hl                                       ; 6

                ld hl, $ + 6    ; return address                ; 10
                ; free registers to use: bc, de, bc'
                jp ix                                           ; 8

                ; restore data
                ex de, hl                                       ; 4
                ld (hl), LD_BC_XXXX_CODE                        ; 11

                ld hl, 0                                        ; 10
                add hl, sp                                      ; 11
                ex de, hl                                       ; 4
                ld sp, (stack_bottom + 2)                       ; 20
                // total ticks: 139

                jp iy      ; ret                                               ; 10 ticks


draw_image:
        // bc - line number

        ; offset = (line % 8) * generate_code_interlive8_len + line/8 * generate_code_line_len = 
        ; (line % 8) * generate_code_interlive8_len + line * 8

        ld (stack_bottom), sp                   ; 20 ticks

        ; e = 8 - (line % 8)
        ld a, c
        and 7
        ld d, a
        ld a, 8
        sub d
        ld e, a

generated_code_since_line_128 equ generated_code + 128/8 * generate_code_line_len

        ; ix = generate code + (line % 8) * bankOffset
        ld h, code_offsets / 256
        ld a, d
        add code_offsets % 256
        ld l, a ; hl = code_offsets + line % 8
        ld a, (hl) ; code_offsets[line % 8]
        add a, generated_code_since_line_128 / 256 + 2 ; shift HL to 512 bytes (code ptr end)
        ld h, a
        ld l, generated_code_since_line_128 % 256 ; ix - code to execute address

        ; cb = roundToFloor(lineNnumber,8) * 8
        ld a, c
        and 0xf8
        rla
        rl b
        rla
        rl b
        rla
        rl b
        ld c, a

        add hl, bc ; generated code to execute

        ld b, e ; line number
        ld c, LD_BC_XXXX_CODE ; constant
        ld de, JP_HL_CODE * 256 + 2 ; constants

        ld sp, 16384 + 1024 * 2
        ld iy, $ + 7
        jp draw_64_lines

        ld sp, -(64 / 8 * generate_code_line_len + generate_code_line_len)
        add hl, sp
        ld sp, 16384 + 1024 * 4
        ld iy, $ + 7
        jp draw_64_lines

        ld sp, -(64 / 8 * generate_code_line_len + generate_code_line_len)
        add hl, sp
        ld sp, 16384 + 1024 * 6
        ld iy, $ + 7
        jp draw_64_lines

        ld sp, (stack_bottom)                   ; 20 ticks
        ret

copy_colors:
        ld hl, color_data
        ld de, 16384 + 6144
        ld bc, 768
        ldir
        ret

prepare_interruption_table:
        ld A, 18h       ; JR instruction code
        ld  (65535), a

        ld   a, 0c3h    ; JP instruction code
        ld   (65524), a
        ld hl, after_interrupt
        ld   (65525), hl

        LD   hl, #FE00
        LD   de, #FE01
        LD   bc, #0100
        LD   (hl), #FF
        LD   a, h
        LDIR
        ld i, a
        im 2
        ret

/*
        MACRO delay   
        sub     43 + 9                                      ; 7
        ld      hl, high(table)  * 256 + 16                 ; 10        17
wait2   sub     l                                           ; 4         21
        jr      nc,wait2                                    ; 12/7      28
        ld      l,a                                         ; 4         32
        ld      l,(hl)                                      ; 7         39
        jp      (hl)                                        ; 4         43
*/        


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

/*************** Main. ******************/
main:
        ld bc, (hl)

        di
        ld sp, stack_top

        ld a, 112   ; 48 is min
        delay

        ; Change border color
        ld a, 1
        out 0xfe,a

        call copy_colors
	
	call prepare_image_data
        call generate_data

        call prepare_interruption_table
        ei
        halt
after_interrupt:        
        ; Pentagon timings
first_timing_in_interrupt equ 21

        di                              ; 4 ticks
        ; remove interrupt data from stack
        pop af                          ; 10 ticks

ticks_after_interrupt equ first_timing_in_interrupt + 12 + 10 + 4 + 10 ; jr + jp + di + pop

screen_ticks  equ 71680
first_rastr_line_tick equ  17920
ticks_per_line equ  224

sync_tick equ first_rastr_line_tick + ticks_per_line * 64
ticks_to_wait equ sync_tick - ticks_after_interrupt

        wait_ticks ticks_to_wait - 10 - 10 - 7

max_scroll_offset equ image_data_len / 32 - 192

        ld bc, max_scroll_offset        ; 10  ticks
        ld de, -1                       ; 10 ticks
.loop:  
        ld a, 1                         ; 7 ticks
        out 0xfe,a                      ; 11 ticks

        push bc                         ; 11 ticks
        push de                         ; 11 ticks
        call draw_image                 ; 67103 ticks
        halt
        pop de                          ; 10 ticks
        pop bc                          ; 10 ticks

        ; do increment
        ld hl, bc
        add hl, de
        ld bc, hl

        ; compare to 0
        ld a, h
        or l
        jp z, .zero_reached

        ; compare to N
        push bc                         ; 11 ticks
        ld bc, -max_scroll_offset       ; 10 ticks
        add hl, bc                      ; 11 ticks
        pop bc                          ; 10 ticks
        
        jp c, .upper_limit_reached      ; 10 ticks
        jp .common_branch               ; 10 ticks
.zero_reached:
        ld de, 1                        ; 10 ticks
        wait_ticks 42
        jp .common_branch               ; 10 ticks
.upper_limit_reached:
        ld de, -1                        ; 10 ticks
.common_branch:

        ld a, 2                         ; 7 ticks
        out 0xfe,a                      ; 11 ticks

.total_ticks_per_loop: equ 67300
        //wait_ticks (screen_ticks - .total_ticks_per_loop)

        jr .loop                        ; 12 ticks
 
        ret

/*************** Data segment ******************/
data_segment_start:
table
        db      low(t09), low(t10), low(t11), low(t12)
        db      low(t13), low(t14), low(t15), low(t16)
        db      low(t17), low(t18), low(t19), low(t20)
        db      low(t21), low(t22), low(t23), low(t24)
        db      low(t25), low(t26), low(t27)

STACK_SIZE: equ 16  ; in words
stack_bottom:
        defs STACK_SIZE * 2, 0
stack_top:

code_offsets:
        db 0
        db generate_code_interlive8_len / 256
        db generate_code_interlive8_len * 2 / 256
        db generate_code_interlive8_len * 3 / 256
        db generate_code_interlive8_len * 4 / 256
        db generate_code_interlive8_len * 5 / 256
        db generate_code_interlive8_len * 6 / 256
        db generate_code_interlive8_len * 7 / 256
code_offsets_end:         
        ; it need to update code. Currently it don't increment low part of the register.
        ASSERT code_offsets / 256 == code_offsets_end / 256
data_segment_end:

/*************** Commands to SJ asm ******************/

    SAVESNA "build/draw_image.sna", main
