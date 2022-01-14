cd %~dp0
encoding_convertor.exe Lapse.z80.asm ../generated_code/reduced_font.asm scroll_text.txt ../generated_code/encoded_text.dat
scr_deinterlace.exe screen2.scr ../generated_code/screen2.scr

del "../generated_code/final.scr.zx0"
"../generated_code/zx0.exe" final.scr "../generated_code/final.scr.zx0"

del "../generated_code/screen3.scr.zx0"
"../generated_code/zx0.exe" screen3.scr "../generated_code/screen3.scr.zx0"

del "../generated_code/scroller.scr.zx0"
del "../generated_code/scroller.scr"
del "../generated_code/scroller_attr.scr.zx0"
del "../generated_code/scroller_attr.scr"
file_truncate.exe scroller.scr "../generated_code/scroller.scr" 2048 2048
"../generated_code/zx0.exe" "../generated_code/scroller.scr" "../generated_code/scroller.scr.zx0"
file_truncate.exe scroller.scr "../generated_code/scroller_attr.scr" 6400 256
"../generated_code/zx0.exe" "../generated_code/scroller_attr.scr" "../generated_code/scroller_attr.scr.zx0"
