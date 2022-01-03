        DEVICE zxspectrum128
        SLDOPT COMMENT WPMEM, LOGPOINT, ASSERTION //< More debug symbols.

/*
 * Page locations:
 * 0,1, 3,4 - rastrData, multicolor data, JP_IX_TABLE
 * 2 - static page 0x8000
 * 5 - static page 0x4000
 * Pages 2,5 include: reach descriptors, MC data
 * 7 - music data, second screen.
 * 6 - descriptors, color drawing.
 * short port page selection:
 *    LD A, #50 + screen_num*8 + page_number
 *    OUT (#fd), A
 **/

screen_addr:            equ 16384
color_addr:             equ 5800h
screen_end:             equ 5b00h
start:                  equ 6b00h
generated_code          equ 28700

draw_offrastr_offset    equ screen_end                     ; [0..15]
draw_offrastr_off_end   equ screen_end + 16

code_after_moving       equ draw_offrastr_off_end

color_data_to_restore   equ #bfc2
saved_bc_value          equ #bfc4

effects_by_run          equ #bfc6
effects_by_run_end      equ effects_by_run + 8

STACK_SIZE:             equ 12  ; in words
stack_bottom            equ effects_by_run_end
stack_top               equ stack_bottom + STACK_SIZE * 2

                        ASSERT(stack_top < #c000)


        INCLUDE "generated_code/labels.asm"

EXX_DE_JP_HL_CODE       EQU 0xeb + 0xe9 * 256

        MACRO DRAW_ONLY_RASTR_LINE N?

                IF (N? < 16)
                        IF (N? % 8 == 0)
                                ld a, iyh
                        ELSEIF (N? % 8 == 2)
                                ld a, iyl
                        ELSEIF (N? % 8 == 4)
                                ld a, ixl
                        ELSEIF (N? % 8 == 6)
                                ld a, ixh
                        ENDIF
                ELSE                        
                        IF (N? % 8 == 0 || N? % 8 == 7)
                                ld a, iyh
                        ELSEIF (N? % 8 == 1)
                                ld a, iyl
                        ELSEIF (N? % 8 == 3)
                                ld a, ixl
                        ELSEIF (N? % 8 == 5)
                                ld a, ixh
                        ENDIF
                ENDIF
                out (0xfd), a

                ld sp, 16384 + ((N? + 8) % 24) * 256 + 256       ; 10
                ld hl, $ + 7
                exx
RASTR0_N?       jp 00 ; rastr for multicolor ( up to 8 lines)       
        ENDM                

        MACRO update_colors_jpix
        //MACRO draw_colors
        ; bc - line number

        ld hl, bc
        srl h: rr l

        ld de, color_descriptor                         ; 10
        add hl, de                                      ; 11
        ld sp, hl                                       ; 6
        ; end of exec address
        pop de                                          ; 10
        ;  address to execute
        pop hl                                          ; 14
        ld (start_draw_colors+1), hl
        ld (start_draw_colors0+1), hl

        // restore data from prev MC drawing steps
        pop hl
        ld sp, hl
        inc sp
        inc sp
        ld hl, (color_data_to_restore)
        push hl

        ex de, hl

        ; write JP_IX to end exec address, 
        ; store original data to HL
        ; store end exec address to de
        ld sp, hl                                       ; 6
        ld de, JP_VIA_HL_CODE                           ; 10
        ex de, hl                                       ; 4
        ex (sp), hl                                     ; 19
        ld (color_data_to_restore), hl
        ENDM

        MACRO FILL_OFF_SP Step?, Iteration?
                ld de, color_addr
                add hl, de
                ld (OFF_Step?_Iteration?_SP + 1), hl
        ENDM

        MACRO CONT_OFF_SP Step?, Iteration?
                dec h
                ld (OFF_Step?_Iteration?_SP + 1), hl
        ENDM

        MACRO restore_jp_ix
                pop hl ; address
                pop de ; restore data
                ld (hl), e
                inc hl
                ld (hl), d
        ENDM
        MACRO write_jp_ix_data_via_de
                pop hl ; address
                ld (hl), e
                inc hl
                ld (hl), d
        ENDM
        MACRO write_jp_ix_data_via_bc
                pop hl ; address
                ld (hl), c
                inc hl
                ld (hl), b
        ENDM

jpix_bank_size          EQU (imageHeight/64 + 2) * jp_ix_record_size

    MACRO DRAW_MULTICOLOR_LINE N?:
                //ld sp, color_addr + N? * 32 + 16        ; 10

//MC_LINE_N?      jp 00                                   ; 10
                // total ticks: 20 (30 with ret)
    ENDM                

    MACRO DRAW_RASTR_LINE N?:
RASTRB_N?
                IF (N? % 8 < 2)
                        ld a, iyh
                ELSEIF (N? % 8 < 4)
                        ld a, iyl
                ELSEIF (N? % 8 < 6)
                        ld a, ixl
                ELSE 
                        ld a, ixh
                ENDIF
                out (0xfd), a

RASTRS_N?       ld sp, screen_addr + ((N? + 8) % 24) * 256 + 256       ; 10
RASTRJ_N?       ld hl, $ + 7                                           ; 10
                exx                                                    ; 4
RASTR_N?        jp 00 ; rastr for multicolor ( up to 8 lines)          ; 10        
    ENDM                

    MACRO DRAW_RASTR_LINE_S N?:
                ld sp, screen_addr + ((N? + 8) % 24) * 256 + 256       ; 10
RASTR_N?        jp 00 ; rastr for multicolor ( up to 8 lines)          ; 10        
    ENDM                

    MACRO DRAW_MULTICOLOR_AND_RASTR_LINE N?
                DRAW_MULTICOLOR_LINE  N?
                DRAW_RASTR_LINE N?
    ENDM                

        MACRO PREPARE_MC_DRAWING N?, prevN?
                // JP XX command+1 address (end addr)
                pop hl
                IF (N? == 23)
                        ld (hl), low(dec_and_loop)
                        inc hl
                        ld (hl), high(dec_and_loop)
                ELSE
                        ld (hl), low(RASTRB_N?)
                        inc hl
                        ld (hl), high(RASTRB_N?)
                ENDIF

                // Second stack moving, fixed at line + 32
                pop hl
                ld de, color_addr + N? * 32 + 32
                ld (hl), e
                inc hl
                ld (hl), d

                // First stack moving, the SP value related from line end
                pop hl
                add hl, de
                ex hl, de

                // First stack moving, LD SP, XX first byte addr
                pop hl                          
                ld (hl), e
                inc hl
                ld (hl), d

                // begin addr
                pop hl
                IF (N? == 0)
                        ld (first_mc_line + 1), hl
                ELSE
                        ld (RASTRJ_prevN? + 1), hl
                ENDIF

                // Line start stack
                inc l   // it is always aligned here
                ld (hl), low(color_addr + N? * 32 + 16)
                inc l
                ld (hl), d
                // total: 25


        ENDM               

        MACRO FILL_SP_DATA_MC_STEP iteration?
            pop de                              ; 10
            ld l, e                             ; 4
            ld (OFF_iteration?_0_SP + 1), hl    ; 16
            ld c, d                             ; 4
            ld (OFF_iteration?_8_SP + 1), bc    ; 20
        ENDM

        MACRO FILL_SP_DATA_MC_STEP2 iteration?
            pop de                              ; 10
            ld l, e                             ; 4
            ld (OFF_iteration?_16_SP + 1), hl   ; 16
        ENDM

        MACRO FILL_SP_DATA_NON_MC_STEP step1?, step2?
            pop de                              ; 10
            ld l, e                             ; 4
            ld (OFF_0_step1?_SP + 1), hl         ; 16
            dec h
            IF (step2? != -1)
                ld c, d                             ; 4
                ld (OFF_0_step2?_SP + 1), bc       ; 16
                dec b
            ENDIF                
        ENDM

        MACRO FILL_SP_DATA_ALL
            exx
            ld h, high(16384 + 6144) - 1        ; 7
            ld b, high(16384 + 4096) - 1        ; 7

            FILL_SP_DATA_MC_STEP 7
            FILL_SP_DATA_MC_STEP 6
            FILL_SP_DATA_MC_STEP 5
            FILL_SP_DATA_MC_STEP 4
            FILL_SP_DATA_MC_STEP 3
            FILL_SP_DATA_MC_STEP 2
            FILL_SP_DATA_MC_STEP 1
            
            FILL_SP_DATA_NON_MC_STEP 0, 8
            FILL_SP_DATA_NON_MC_STEP 1, 9
            FILL_SP_DATA_NON_MC_STEP 2, 10
            FILL_SP_DATA_NON_MC_STEP 3, 11
            FILL_SP_DATA_NON_MC_STEP 4, 12
            FILL_SP_DATA_NON_MC_STEP 5, 13
            FILL_SP_DATA_NON_MC_STEP 6, 14
            FILL_SP_DATA_NON_MC_STEP 7, 15
            exx                                 ; 4

            inc h                               ; 4
            ld sp, hl                           ; 6
            ld h, high(16384 + 2048) - 1        ; 7

            FILL_SP_DATA_MC_STEP2 7
            FILL_SP_DATA_MC_STEP2 6
            FILL_SP_DATA_MC_STEP2 5
            FILL_SP_DATA_MC_STEP2 4
            FILL_SP_DATA_MC_STEP2 3
            FILL_SP_DATA_MC_STEP2 2
            FILL_SP_DATA_MC_STEP2 1

            FILL_SP_DATA_NON_MC_STEP 16, -1
            FILL_SP_DATA_NON_MC_STEP 17, -1
            FILL_SP_DATA_NON_MC_STEP 18, -1
            FILL_SP_DATA_NON_MC_STEP 19, -1
            FILL_SP_DATA_NON_MC_STEP 20, -1
            FILL_SP_DATA_NON_MC_STEP 21, -1
            FILL_SP_DATA_NON_MC_STEP 22, -1
            FILL_SP_DATA_NON_MC_STEP 23, -1

        ENDM

        MACRO LONG_SET_PAGE page_number
                ld bc, #7ffd
                ld a, page_number
                out (c),a
        ENDM

        MACRO SET_PAGE page_number
                LD A, #50 + page_number
                OUT (#fd), A
        ENDM

        MACRO set_page_by_bank shadowBit
                ; a - bank number
                rra
                cp 2
                ccf
                if (shadowBit)
                        adc 0x58
                else
                        adc 0x50
                endif                        
                out (0xfd), a
        ENDM

/************** delay routine *************/
        MACRO DO_DELAY

//d1      equ     17 + 15+21+11+15+14
d1      equ     15+21+11+15+14 + 7 - 5
d2      equ     16
d3      equ     20+10+11+12

base
delay   xor     a               ; 4
        or      h               ; 4
        jr      nz,wait1        ; 12/7  20/15
        ld      a,l             ; 4
        sub     d1              ; 7
        ld      hl,high(base)*256 + d2      ; 10    21
wait2   sub     l               ; 4
        jr      nc,wait2        ; 12/7  16/11
        sub (256-16) - low(table)
        ld      l,a             ; 4
        ld      l,(hl)          ; 7
        jp      (hl)            ; 4     15

table
        db      low(t09), low(t10), low(t11), low(t12)
        db      low(t13), low(t14), low(t15), low(t16)
        db      low(t17), low(t18), low(t19), low(t20)
        db      low(t21), low(t22), low(t23), low(t24)
        db      low(t25), low(t26), low(t27)
table_end

wait1   ld      de,-d3          ; 10
        add     hl,de           ; 11
        jr      delay           ; 12    33

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
        ASSERT high(delay_end) == high(base)
        ENDM
/************** end delay routine *************/        

        org start

        JP move_code

BEGIN   DISP code_after_moving

        INCLUDE "draw_off_rastr.asm"
        INCLUDE "effects_by_run.asm"
main_entry_point

/*************** Main. ******************/
        ld sp, stack_top

        //ld  hl, packed_music
        IF (HAS_PLAYER == 1)
                ld hl, mus_intro
                call  init // player init
        ENDIF                

        call prepare_interruption_table
        call unpack_and_play_init_screen

1       halt
        jr 1b
after_play_intro
        // 224-47 = 177t longer than final ret in align int.
        // Calculate phase as ticks between: (alignInt.PrevHandler-after_play_intro-177) % 71680
        ld sp, stack_top
        ld hl, music_main
        call  init // player init

        call write_initial_jp_ix_table
        call create_jpix_helper
        ld ix, 0x5051
        ld iy, 0x5453


        ; Pentagon timings
first_timing_in_interrupt       equ 19 + 22 + 47
screen_ticks                    equ 71680
first_rastr_line_tick           equ  17920
screen_start_tick               equ  17988
ticks_per_line                  equ  224


mc_preambula_delay      equ 46
fixed_startup_delay     equ 28222-398 + 17017 - 145  +11+11-3 - 4  + 6
initial_delay           equ first_timing_in_interrupt + fixed_startup_delay +  mc_preambula_delay
INTERRUPT_PHASE         EQU 1   ; The value in range [0..3]. TODO: it need to property calculate it!
sync_tick               equ screen_ticks + screen_start_tick  - initial_delay +  FIRST_LINE_DELAY - INTERRUPT_PHASE
interrupt_phase EQU 2        

        DISPLAY	"sync_tick ", /D, sync_tick

        assert (sync_tick <= 65535 && sync_tick >= 4)
        call static_delay

        ld hl, after_player
        ld (stack_top), hl

max_scroll_offset equ imageHeight - 1

        ld a, 3                         ; 7 ticks
        out 0xfe,a                      ; 11 ticks

        // Prepare data
        xor a
        ld hl, COLOR_REG_AF2
        push hl
        ex af, af'
        pop af
        ex af, af'

        ld bc, 0h                       ; 10  ticks
        jp loop1
lower_limit_reached:
        ld bc,  max_scroll_offset  ; 14 ticks
        
        // Go to next timings page
        ld hl, load_timings_data+1
        ld a, (hl)
        add 4
        and #ef
        ld (hl), a
         // total: 10+7+7+7+7=38

         // Prepare next effect for next scroll run
        and #0f
        rra
        add low(effects_by_run)
        ld l,a
        ld h,high(effects_by_run)
        ld sp, hl
        pop hl
        jp hl
        // total: 7+4+7+4+7+6+10+4=49


dec_and_loop:
        ld bc, (saved_bc_value)
        dec bc
loop:  
jp_ix_line_delta_in_bank EQU 2 * 6*4
        // --------------------- update_jp_ix_table --------------------------------

        // ------------------------- update jpix table
        // bc - screen address to draw
        // between frames: 73248/71456
        ; a = bank number
        // set bits that match page number to 0
        ld a, ~6                        ; 7
        and c                           ; 4
        ld h, b                         ; 4
        rr h                            ; 8
        rra                             ; 4
        jr nc, no                       ; 7/12
        add 2                           ; 7
no:
        ld l, a                         ; 4
        ld de,  update_jpix_helper      ; 10
        add hl, de                      ; 11
        
        ld sp, hl
        pop hl
        ld sp, hl

next_step_first_bank
        SET_PAGE 0 //< Page number is updated in runtime

        ld hl, loop1
        exx
        ld bc, JP_VIA_HL_CODE
        .2 restore_jp_ix
        .2 write_jp_ix_data_via_bc
        .2 restore_jp_ix
        .2 write_jp_ix_data_via_bc
        .2 restore_jp_ix
        .2 write_jp_ix_data_via_bc
        // total: 562/560 (with switching page)
        // ------------------------- update jpix table end
        scf
        DRAW_RASTR_LINE_S 23

loop1:
        ld (saved_bc_value), bc
        SET_PAGE 6
        ld a, 7
        and c
        jp nz, mc_step_drawing

        /************************* no-mc step drawing *********************************************/

        update_colors_jpix        
        ld sp, color_addr + 768                         ; 10
        ld hl, $ + 7                                    ; 10
        exx                                             ; 4
start_draw_colors0:
        jp 00                                           ; 8

        // Update off rastr drawing for then next 8 steps

        sla c : rl b    // bc*2
        ld hl, off_rastr_descriptors
        add hl,bc       // * 2
        ld sp, hl

        // 2. Fill JP commands
        exx

        pop hl: ld(OFF_7_0_JP+1), hl
        pop hl: ld(OFF_6_0_JP+1), hl
        pop hl: ld(OFF_5_0_JP+1), hl
        pop hl: ld(OFF_4_0_JP+1), hl
        pop hl: ld(OFF_3_0_JP+1), hl
        pop hl: ld(OFF_2_0_JP+1), hl
        pop hl: ld(OFF_1_0_JP+1), hl
        
        pop hl: ld(OFF_0_0_JP+1), hl
        pop hl: ld(OFF_0_1_JP+1), hl
        pop hl: ld(OFF_0_2_JP+1), hl
        pop hl: ld(OFF_0_3_JP+1), hl
        pop hl: ld(OFF_0_4_JP+1), hl
        pop hl: ld(OFF_0_5_JP+1), hl
        pop hl: ld(OFF_0_6_JP+1), hl
        pop hl: ld(OFF_0_7_JP+1), hl


        ld hl, (64-15) * 2
        add hl, sp
        ld sp, hl

        pop hl: ld(OFF_7_8_JP+1), hl
        pop hl: ld(OFF_6_8_JP+1), hl
        pop hl: ld(OFF_5_8_JP+1), hl
        pop hl: ld(OFF_4_8_JP+1), hl
        pop hl: ld(OFF_3_8_JP+1), hl
        pop hl: ld(OFF_2_8_JP+1), hl
        pop hl: ld(OFF_1_8_JP+1), hl
        
        pop hl: ld(OFF_0_8_JP+1), hl
        pop hl: ld(OFF_0_9_JP+1), hl
        pop hl: ld(OFF_0_10_JP+1), hl
        pop hl: ld(OFF_0_11_JP+1), hl
        pop hl: ld(OFF_0_12_JP+1), hl
        pop hl: ld(OFF_0_13_JP+1), hl
        pop hl: ld(OFF_0_14_JP+1), hl
        pop hl: ld(OFF_0_15_JP+1), hl

        exx
        inc h
        ld sp, hl

        pop hl: ld(OFF_7_16_JP+1), hl
        pop hl: ld(OFF_6_16_JP+1), hl
        pop hl: ld(OFF_5_16_JP+1), hl
        pop hl: ld(OFF_4_16_JP+1), hl
        pop hl: ld(OFF_3_16_JP+1), hl
        pop hl: ld(OFF_2_16_JP+1), hl
        pop hl: ld(OFF_1_16_JP+1), hl
        
        pop hl: ld(OFF_0_16_JP+1), hl
        pop hl: ld(OFF_0_17_JP+1), hl
        pop hl: ld(OFF_0_18_JP+1), hl
        pop hl: ld(OFF_0_19_JP+1), hl
        pop hl: ld(OFF_0_20_JP+1), hl
        pop hl: ld(OFF_0_21_JP+1), hl
        pop hl: ld(OFF_0_22_JP+1), hl
        pop hl: ld(OFF_0_23_JP+1), hl

        // 3. prepare offscreen last lines

        ld hl, off_rastr_sp_delta
        add hl, bc
        ld sp, hl
        FILL_SP_DATA_ALL

        // 4. prepare offscreen exec data

        ld hl, EXX_DE_JP_HL_CODE
        ld (it0_end), hl
        ld (it7_end), hl
        ld (it6_end), hl
        ld (it5_end), hl
        ld (it4_end), hl
        ld (it3_end), hl
        ld (it2_end), hl

        // -------------------- MC rastr descriptors

        ld hl, mc_rastr_descriptors_bottom
        add hl,bc       // * 2
        ld sp, hl

        // Draw bottom 3-th of rastr during middle 3-th of colors
        pop hl: ld (RASTR0_15+1), hl: ld (RASTR_15+1), hl
        pop hl: ld (RASTR0_14+1), hl: ld (RASTR_14+1), hl
        pop hl: ld (RASTR0_13+1), hl: ld (RASTR_13+1), hl
        pop hl: ld (RASTR0_12+1), hl: ld (RASTR_12+1), hl
        pop hl: ld (RASTR0_11+1), hl: ld (RASTR_11+1), hl
        pop hl: ld (RASTR0_10+1), hl: ld (RASTR_10+1), hl
        pop hl: ld (RASTR0_9+1), hl: ld (RASTR_9+1), hl
        pop hl: ld (RASTR0_8+1), hl: //ld (RASTR_8+1), hl

        // Draw middle 3-th of rastr during top 3-th of colors

        ld hl, (64-8) * 2
        add hl, sp
        ld sp, hl

        pop hl: ld (RASTR0_7+1), hl: ld (RASTR_7+1), hl
        pop hl: ld (RASTR0_6+1), hl: ld (RASTR_6+1), hl
        pop hl: ld (RASTR0_5+1), hl: ld (RASTR_5+1), hl
        pop hl: ld (RASTR0_4+1), hl: ld (RASTR_4+1), hl
        pop hl: ld (RASTR0_3+1), hl: ld (RASTR_3+1), hl
        pop hl: ld (RASTR0_2+1), hl: ld (RASTR_2+1), hl
        pop hl: ld (RASTR0_1+1), hl: ld (RASTR_1+1), hl
        pop hl: ld (RASTR0_0+1), hl: //ld (RASTR_0+1), hl

        // Draw top 3-th of rastr during bottom 3-th of colors
        ; shift to 63 for MC rastr instead of 64 to move on next frame
        ld hl, mc_rastr_descriptors_next + 127 * 2
        add hl, bc
        ld sp, hl

        pop hl: ld (RASTR_23+1), hl
        pop hl: ld (RASTR0_22+1), hl: ld (RASTR_22+1), hl
        pop hl: ld (RASTR0_21+1), hl: ld (RASTR_21+1), hl
        pop hl: ld (RASTR0_20+1), hl: ld (RASTR_20+1), hl
        pop hl: ld (RASTR0_19+1), hl: ld (RASTR_19+1), hl
        pop hl: ld (RASTR0_18+1), hl: ld (RASTR_18+1), hl
        pop hl: ld (RASTR0_17+1), hl: ld (RASTR_17+1), hl
        pop hl: ld (RASTR0_16+1), hl: //ld (RASTR_16+1), hl

        ld hl, 0x18 + (finish_non_mc_drawing - start_mc_drawing - 2) * 256 // put jr command to code
        ld (start_mc_drawing), hl

        ld de, finish_off_drawing_0
        ld h, high(it0_start)
        jp it0_start
finish_off_drawing_0
        // MC part rastr
        ld (next_step_first_bank + 1), a
        scf
        DRAW_ONLY_RASTR_LINE 0
        DRAW_ONLY_RASTR_LINE 1
        DRAW_ONLY_RASTR_LINE 2
        DRAW_ONLY_RASTR_LINE 3
        DRAW_ONLY_RASTR_LINE 4
        DRAW_ONLY_RASTR_LINE 5
        DRAW_ONLY_RASTR_LINE 6
        DRAW_ONLY_RASTR_LINE 7
        
        DRAW_ONLY_RASTR_LINE 8
        DRAW_ONLY_RASTR_LINE 9
        DRAW_ONLY_RASTR_LINE 10
        DRAW_ONLY_RASTR_LINE 11
        DRAW_ONLY_RASTR_LINE 12
        DRAW_ONLY_RASTR_LINE 13
        DRAW_ONLY_RASTR_LINE 14
        DRAW_ONLY_RASTR_LINE 15

        DRAW_ONLY_RASTR_LINE 16
        DRAW_ONLY_RASTR_LINE 17
        DRAW_ONLY_RASTR_LINE 18
        DRAW_ONLY_RASTR_LINE 19
        DRAW_ONLY_RASTR_LINE 20
        DRAW_ONLY_RASTR_LINE 21
        DRAW_ONLY_RASTR_LINE 22


        jp bank_drawing_common2

//*************************************************************************************
mc_step_drawing:
        ld sp, color_addr + 768                         ; 10
        ld hl, $ + 7                                    ; 14
        exx
start_draw_colors:
        jp 00                                           ; 8

        sla c : rl b    // bc*2
        
        // Exec off rastr
        ld de, bank_drawing_common  // next jump
        ld h, high(draw_offrastr_offset)
        ld a, 15
        and c
        ld l, a
        ld sp, hl

        pop hl
        jp hl

finish_non_mc_drawing_cont:
        SET_PAGE 6
        // calculate address of the MC descriptors
        ld hl, mc_descriptors

        // prepare  multicolor drawing (for next 7 mc steps)
        // calculate bc/8 * 10
        ld bc, (saved_bc_value)
        ld de, bc
        add hl, bc

        srl d: rr e
        srl d: rr e
        add hl, de
        ld sp, hl

        PREPARE_MC_DRAWING 23, 22
        PREPARE_MC_DRAWING 22, 21
        PREPARE_MC_DRAWING 21, 20
        PREPARE_MC_DRAWING 20, 19
        PREPARE_MC_DRAWING 19, 18
        PREPARE_MC_DRAWING 18, 17
        PREPARE_MC_DRAWING 17, 16
        PREPARE_MC_DRAWING 16, 15
        PREPARE_MC_DRAWING 15, 14
        PREPARE_MC_DRAWING 14, 13
        PREPARE_MC_DRAWING 13, 12
        PREPARE_MC_DRAWING 12, 11
        PREPARE_MC_DRAWING 11, 10
        PREPARE_MC_DRAWING 10, 9
        PREPARE_MC_DRAWING 9, 8
        PREPARE_MC_DRAWING 8, 7
        PREPARE_MC_DRAWING 7, 6
        PREPARE_MC_DRAWING 6, 5
        PREPARE_MC_DRAWING 5, 4
        PREPARE_MC_DRAWING 4, 3
        PREPARE_MC_DRAWING 3, 2
        PREPARE_MC_DRAWING 2, 1
        PREPARE_MC_DRAWING 1, 0
        PREPARE_MC_DRAWING 0, -1

        ld hl, 0x37 + 0xc3*256 // restore data: scf, JP
        ld (start_mc_drawing), hl


        // update ports [50..54] for the next step 7

        LD a, 0X7D      // LD a, iyl (value 0x54 -> 0x53)
        ld (RASTRS_17 - 3), a

        LD a, 0X7C      // LD a, ixh (value 0x51 -> 0x50)
        ld (RASTRS_21 - 3), a

        LD a, 0XDD      // LD a, IX prefix (value 0x53 -> 0x51)
        ld (RASTRS_19 - 4), a

        dec bc
        ; compare to -1
        ld a, b
        inc a
        jp z, lower_limit_reached      ; 10 ticks
        jp loop                        ; 12 ticks

finish_non_mc_drawing:
        jp finish_non_mc_drawing_cont

bank_drawing_common:
        ld (next_step_first_bank + 1), a
bank_drawing_common2:
        ; delay

        ASSERT (timings_data % 32 == 0)
load_timings_data        
        ld hl, timings_data + 12
        add hl, bc
        ld sp, hl
        pop hl

        DO_DELAY

        IF (HAS_PLAYER == 1)
                ld sp, stack_top
                SET_PAGE 7
                jp play
        ENDIF                                

after_player
start_mc_drawing:
        ; timing here on first frame: 71680 * 2 + 17988 + 224*6 - (19 + 22) - 20 = 162631-6=162625=162625
        ; timing here on first frame: 162625+12 - 224 = 162413
        ; after non-mc frame: 144704, between regular lines: 71680-224 = 71456
        scf             // scf, jp is overrided to "jr finish_non_mc_drawing" for non-mc drawing step
first_mc_line: JP 00

        DRAW_MULTICOLOR_AND_RASTR_LINE 0
        DRAW_MULTICOLOR_AND_RASTR_LINE 1
        DRAW_MULTICOLOR_AND_RASTR_LINE 2
        DRAW_MULTICOLOR_AND_RASTR_LINE 3
        DRAW_MULTICOLOR_AND_RASTR_LINE 4
        DRAW_MULTICOLOR_AND_RASTR_LINE 5
        DRAW_MULTICOLOR_AND_RASTR_LINE 6
        DRAW_MULTICOLOR_AND_RASTR_LINE 7
        
        DRAW_MULTICOLOR_AND_RASTR_LINE 8
        DRAW_MULTICOLOR_AND_RASTR_LINE 9
        DRAW_MULTICOLOR_AND_RASTR_LINE 10
        DRAW_MULTICOLOR_AND_RASTR_LINE 11
        DRAW_MULTICOLOR_AND_RASTR_LINE 12
        DRAW_MULTICOLOR_AND_RASTR_LINE 13
        DRAW_MULTICOLOR_AND_RASTR_LINE 14
        DRAW_MULTICOLOR_AND_RASTR_LINE 15

        DRAW_MULTICOLOR_AND_RASTR_LINE 16
        DRAW_MULTICOLOR_AND_RASTR_LINE 17
        DRAW_MULTICOLOR_AND_RASTR_LINE 18
        DRAW_MULTICOLOR_AND_RASTR_LINE 19
        DRAW_MULTICOLOR_AND_RASTR_LINE 20
        DRAW_MULTICOLOR_AND_RASTR_LINE 21
        DRAW_MULTICOLOR_AND_RASTR_LINE 22
        DRAW_MULTICOLOR_LINE 23

/**     ----------------------------------------- routines -----------------------------------------
 *      All routines are called once before drawing. They can be moved to another page to free memory.
 */


create_jpix_helper
        ASSERT(imageHeight / 4 < 256)
        ld hl, update_jpix_helper
        ld iy, 0xc000
        ld ixl, imageHeight / 64
.loop2  ld a, 16
        push iy
        pop de
        ld bc, jpix_bank_size
.loop:  ld (hl), e
        inc hl
        ld (hl), d
        inc hl
        ex hl, de
        add hl, bc
        ex hl, de

        dec a
        jr nz, .loop
        ld bc, jp_ix_record_size
        add iy, bc
        dec ixl
        jr nz, .loop2

        ret

JP_VIA_HL_CODE          equ #e9d9
jpix_table              EQU 0xc000
jp_ix_record_size       equ 12
jp_write_data_offset    equ 8
data_page_count         equ 4
jp_ix_bank_size         equ (imageHeight/64 + 2) * jp_ix_record_size

write_initial_jp_ix_table
        xor a
page_loop:
        bit 0, a
        jr nz, continue_page        
        ld sp, jpix_table + jp_write_data_offset
continue_page:
        ld b, 3

        ld c, a
        and a
        set_page_by_bank 1
        ld a, c

        ld de, JP_VIA_HL_CODE
.rep:   write_jp_ix_data_via_de
        write_jp_ix_data_via_de
        ld hl, jp_ix_record_size - 4
        add hl, sp
        ld sp, hl
        djnz .rep

        ld sp, jpix_table + jp_ix_bank_size + jp_write_data_offset

        inc a
        cp 8
        jr nz, page_loop

        // write initial color data to restore
        SET_PAGE 6+8

        ld hl, color_descriptor + 4                     ; 10
        ld sp, hl                                       ; 6
        pop hl                                          ; 10
        ld sp, hl
        pop hl
        ld (color_data_to_restore), hl                  ; 16

        ld sp, stack_top - 2
        ret

static_delay
D0              EQU 17 + 10 + 10        ; call, initial LD, ret
D1              EQU     6 + 4 + 4 + 12     ; 26
DELAY_STEPS     EQU (sync_tick-D0) / D1
TICKS_REST      EQU  (sync_tick-D0) - DELAY_STEPS * D1 + 5
                LD hl, DELAY_STEPS      ; 10
.rep            dec hl                  ; 6
                ld a, h                 ; 4 
                or l                    ; 4
                jr nz, .rep             ; 7/12
                ASSERT(TICKS_REST >= 5 && TICKS_REST <= 30)
t1              EQU TICKS_REST                
                IF (t1 >= 26)
                        add hl, hl
t2                      EQU t1 - 11                        
                ELSE
t2                      EQU t1
                ENDIF

                IF (t2 >= 15)
                        add hl, hl
t3                      EQU t2 - 11
                ELSE
t3                      EQU t2
                ENDIF

                IF (t3 >= 10)
                        inc hl
t4                      EQU t3 - 6                        
                ELSE
t4                      EQU t3
                ENDIF

                IF (t4 == 9)
                        ret c
                        nop
                ELSEIF (t4 == 8)
                        ld hl, hl
                ELSEIF (t4 == 7)
                        ld l, (hl)
                ELSEIF (t4 == 6)
                        inc hl
                ELSEIF (t4 == 5)
                        ret c
                ELSEIF (t4 == 4)
                        nop
                ELSE
                        ASSERT 0
                ENDIF
        ret

        INCLUDE "zx0_standard.asm"
        //INCLUDE "include/l4_psg_player.asm"
        INCLUDE "include/fast_psg_player.asm"

        ASSERT $ < generated_code

        ENT     ; end of disp
move_code
        di
        LD HL, BEGIN
        LD DE, code_after_moving
        LD BC, move_code - BEGIN
        LDIR 
        JP main_entry_point
main_code_end
        //INCBIN "generated_code/mt_and_rt_reach_descriptor.z80"
        //INCBIN "generated_code/multicolor.z80"
        INCBIN "generated_code/ram2.zx0"
main_data_end
init_screen_i1        
        INCBIN "resources/screen2.scr.i1", 0, 6144/2
        INCLUDE "alignint.asm"
        INCLUDE "simple_scroller.asm"
page2_end

update_jpix_helper   EQU 0xc000 - 512
        ASSERT simple_scroller_end < update_jpix_helper
        ASSERT generated_code + RAM2_UNCOMPRESSED_SIZE < update_jpix_helper
        DISPLAY	"Packed Page 2 free ", /D, update_jpix_helper - simple_scroller_end
        DISPLAY	"Unpacked Page 2 free ", /D, update_jpix_helper - (generated_code + RAM2_UNCOMPRESSED_SIZE), " bytes"


        ORG 0xc000
        PAGE 0
        //INCBIN "generated_code/jpix0.dat"
        //INCBIN "generated_code/timings0.dat"
        //INCBIN "generated_code/main0.z80"
        //INCBIN "generated_code/reach_descriptor0.z80"
        INCBIN "generated_code/ram0.zx0"
ram0_end
        INCBIN "generated_code/first_screen.zx0"
page0_end
        DISPLAY	"Packed Page 0 free ", /D, 65536 - page0_end, " bytes"
        DISPLAY	"Unpacked Page 0 free ", /D, 16384 - RAM0_UNCOMPRESSED_SIZE, " bytes"


        ORG 0xc000
        PAGE 1
        //INCBIN "generated_code/jpix1.dat"
        //INCBIN "generated_code/timings1.dat"
        //INCBIN "generated_code/main1.z80"
        //INCBIN "generated_code/reach_descriptor1.z80"
        INCBIN "generated_code/ram1.zx0"
ram1_end
init_screen_i0
        INCBIN "resources/screen2.scr.i0"
page1_end
        DISPLAY	"Packed Page 1 free ", /D, 65536 - page1_end, " bytes"
        DISPLAY	"Unpacked Page 1 free ", /D, 16384 - RAM1_UNCOMPRESSED_SIZE, " bytes"

        ORG 0xc000
        PAGE 3
        INCBIN "generated_code/jpix2.dat"
        INCBIN "generated_code/timings2.dat"
        INCBIN "generated_code/main2.z80"
        INCBIN "generated_code/reach_descriptor2.z80"
page3_end        
        DISPLAY	"Page 3 free ", /D, 65536 - page3_end, " bytes"

        ORG 0xc000
        PAGE 4
        INCBIN "generated_code/jpix3.dat"
        INCBIN "generated_code/timings3.dat"
        INCBIN "generated_code/main3.z80"
        INCBIN "generated_code/reach_descriptor3.z80"
page4_end        
        DISPLAY	"Page 4 free ", /D, 65536 - page4_end, " bytes"

        ORG 0xc000
        PAGE 6
color_code
        INCBIN "generated_code/color.z80"
color_descriptor
        INCBIN "generated_code/color_descriptor.dat"
off_rastr_descriptors
        INCBIN "generated_code/off_rastr_descriptors.dat"
off_rastr_sp_delta
        INCBIN "generated_code/sp_delta_descriptors.dat"
mc_rastr_descriptors_bottom
        INCBIN "generated_code/mc_rastr_bottom_descriptors.dat"
mc_rastr_descriptors_top
        INCBIN "generated_code/mc_rastr_top_descriptors.dat"
mc_rastr_descriptors_next
        INCBIN "generated_code/mc_rastr_next_descriptors.dat"
mc_descriptors
        INCBIN "generated_code/mc_descriptors.dat"
mus_intro
        INCBIN "resources/intro.mus"

page6_end
        DISPLAY	"Page 6 free ", /D, 65536 - $, " bytes"

        ORG 0xc000
        PAGE 7
        INCBIN "resources/screen1.scr", 0, 6144
        BLOCK 768, #7
music_main
        INCBIN "resources/main.mus"
page7_end
        DISPLAY	"Page 7 free ", /D, 65536 - $, " bytes"

/*************** Ennd image data. ******************/


/*************** Commands to SJ asm ******************/

        SAVESNA "build/draw_image.sna", start
        savetap "build/draw_image.tap", start

        EMPTYTRD "build/scroller.trd" ;create empty TRD image

        SAVETRD "build/scroller.trd","main.C", start, page2_end - start

        PAGE 0
        SAVETRD "build/scroller.trd","ram0.C", $C000, page0_end - $C000
        PAGE 1
        SAVETRD "build/scroller.trd","ram1.C",$C000, page1_end - $C000
        PAGE 3
        SAVETRD "build/scroller.trd","ram3.C",$C000, page3_end - $C000
        PAGE 4
        SAVETRD "build/scroller.trd","ram4.C",$C000, page4_end - $C000
        PAGE 6
        SAVETRD "build/scroller.trd","ram6.C", $C000, page6_end - $C000
        PAGE 7
        SAVETRD "build/scroller.trd","ram7.C", $C000, page7_end - $C000

        // Just a fake address to load files into TRD
        ORG 0x8000
fix128k_script
        incbin "fix128k.C"
boot_start
        incbin "boot.B"
boot_end        
        SAVETRD "build/scroller.trd","fix128k.C", fix128k_script, boot_start - fix128k_script
        SAVETRD "build/scroller.trd","boot.B", boot_start, boot_end - boot_start, 1
