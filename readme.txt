Vertical per pixel full screen scroller for ZX spectrum. 
Designed for Pentagon 128/256/512/1024.
Build number 3.10.



 Pouet: https://www.pouet.net/prod.php?which=90767
 Demozoo: https://demozoo.org/productions/304794/
 Download: https://emulate.su/rmda/scroller.zip
 ZXArt: https://zxart.ee/eng/software/demoscene/demo/fast-demo/scroller5/ 
 Source: https://github.com/vasilenkoroman/zx_scroll



How it works:
the engine compares two screens and updates changed bytes only both for raster and color bytes.
This pre-calculation is performed on PC and generator produce Z80 code for it. The generated code
contains such commands like LD/PUSH and INC/DEC for registers. Generator keeps current state of Z80
registers for the each step. So, when it need to write next 16 bit value, it tries to deduce it
from the current registers state first: direct using of existing 16-bit register or updating only 8
bit of 16-bit register or use 8-bit increment only e.t.c.

Generator looks up to 40 millions combination for the each line to choose that register is better to
use for N-th step. Sometimes it is better to keep 'BC' for example, and use 'DE' for N-th step,
because BC value can be reused at step N+x. It just doing brute force for it. Such registers state
is shared between lines across whole frames. 
The images itself isn't multicolor, this term related to scroller itself because it is able to move image
with per-pixel accuracy. To achieve that the engine draw colors twice: for upper and lower part of
each cell. The second draw is occurred just in time when the Ray is about to draw N-th color line. 
The most difficult part is the task manager: it need to switch between drawing colors and raster without
losing time on waits but second color drawing should be synchronized with Screen Ray.
So, it need to save register values to switch to another task and it need to do as fast as possible.
Actually, this task manager is pre-calculated at PC as well, inside Z80 code generator.

In addition, there are a lot of minor optimization tricks: using push AF, writing random values to th
raster bytes for hidden attributes (in case of same inc/paper), using LD (HL), reg8 in addition to
PUSH command and many others.



Change log:
3.7
	initial release
3.10	
	Add .tap version.
	Add one more empty line on page7 during text effect.
	Align int phase fixed 2->1.
3.11
	Fix crash on standard Spectrum 128k
