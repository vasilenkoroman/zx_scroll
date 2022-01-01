cd /D "%~dp0"
del ram2
del ram2.zx0
copy /B mt_and_rt_reach_descriptor.z80+multicolor.z80 ram2
call zx0.exe ram2
del ram2

del ram0
del ram0.zx0
copy /B jpix0.dat+timings0.dat+main0.z80+reach_descriptor0.z80 ram0
call zx0.exe ram0
del ram0

del first_screen.zx0
call zx0.exe first_screen.scr first_screen.zx0
