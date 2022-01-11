cd %~dp0
encoding_convertor.exe Lapse.z80.asm ../generated_code/reduced_font.asm scroll_text.txt ../generated_code/encoded_text.dat
scr_deinterlace.exe screen2.scr ../generated_code/screen2.scr

del "../generated_code/final.scr.zx0"
"../generated_code/zx0.exe" final.scr "../generated_code/final.scr.zx0"
