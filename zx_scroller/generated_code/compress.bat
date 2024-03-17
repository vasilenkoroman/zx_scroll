cd /D "%~dp0"
del ram2
del ram2.zx0
copy /B mt_and_rt_reach_descriptor.z80+multicolor.z80 ram2
call zx0.exe ram2
del ram2

del ram0
del ram0.zx0
copy /B timings0.dat+jpix0.dat+main0.z80+reach_descriptor0.z80 ram0
call zx0.exe ram0
del ram0

del ram1
del ram1.zx0
copy /B timings1.dat+jpix1.dat+main1.z80+reach_descriptor1.z80 ram1
call zx0.exe ram1
del ram1

del ram3
del ram3.zx0
copy /B timings2.dat+jpix2.dat+main2.z80+reach_descriptor2.z80 ram3
call zx0.exe ram3
del ram3

del first_screen.zx0
call zx0.exe first_screen.scr first_screen.zx0
