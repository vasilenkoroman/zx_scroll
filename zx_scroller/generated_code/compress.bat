cd /D "%~dp0"
del ram2
del ram2.zx0
copy /B mt_and_rt_reach_descriptor.z80+multicolor.z80 ram2
call zx0.exe ram2
del ram2
