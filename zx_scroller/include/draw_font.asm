;   Render text in the middle of attributes. So, Y is fixed.
;   b   -   X position to render char
;   a   -   char code
print_ch
    //ld hl,font_data
    /*
    add low(font_data)
    ld l,a
    adc low(font_data)
*/