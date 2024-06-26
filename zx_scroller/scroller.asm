        DEVICE zxspectrum128
        SLDOPT COMMENT WPMEM, LOGPOINT, ASSERTION ; More debug symbols.

        INCLUDE "delay.asm"

/**
  * Page locations:
  * 0,1, 3,4 - rastrData, multicolor data, JP_IX_TABLE
  * 2 - static page #8000
  * 5 - static page #4000
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
generated_code          equ 29900

code_after_moving       equ screen_end

timings_data            equ #c000
data_start              equ #bfc2

STACK_SIZE:             equ 12  ; in words
stack_bottom            equ data_start
stack_top               equ stack_bottom + STACK_SIZE * 2

                        ASSERT(stack_top < #c000)

                        INCLUDE "generated_code/labels.asm"

EXX_DE_JP_HL_CODE       EQU #eb + #e9 * 256

        MACRO DRAW_ONLY_RASTR_LINE N?
.prevHl         equ $
                IF (N? < 16)
                        IF (N? % 8 == 0)
                                ld a, #54
                                out (#fd), a
                        ELSEIF (N? % 8 == 2)
                                dec a
                                out (#fd), a
                        ELSEIF (N? % 8 == 4)
                                ld a, #51
                                out (#fd), a
                        ELSEIF (N? % 8 == 6)
                                dec a
                                out (#fd), a
                        ENDIF
                ELSE                        
                        IF (N? % 8 == 0 || N? % 8 == 7)
                                ld a, #54
                                out (#fd), a
                        ELSEIF (N? % 8 == 1)
                                dec a
                                out (#fd), a
                        ELSEIF (N? % 8 == 3)
                                ld a, #51
                                out (#fd), a
                        ELSEIF (N? % 8 == 5)
                                dec a
                                out (#fd), a
                        ENDIF
                ENDIF

                ld sp, 16384 + ((N? + 8) % 24) * 256 + 256       ; 10
.newHl          equ $ + 7
                IF N? == 0 || high(.newHl) != high(.prevHl)
                        ld hl, $ + 7
                ELSE
                        ld l, low($ + 6)
                ENDIF
                exx
RASTR0_N?       jp 00 ; rastr for multicolor ( up to 8 lines)       
        ENDM                

        MACRO DRAW_ONLY_RASTR_LINE_22
                ld sp, 16384 + ((22 + 8) % 24) * 256 + 256       ; 10
                exx
RASTR0_22       jp 00 ; rastr for multicolor ( up to 8 lines)       
        ENDM                

        MACRO update_colors_jpix
color_descriptor_ptr
                ld sp, color_descriptor + imageHeight /2 - 4 + 2                                                ; 10
                
                pop de                           ; data from prev MC drawing steps (last endAddress)            ; 20
                pop hl                           ; address to execute (current descriptor)                      ; 30
                ld (color_descriptor_ptr+1), sp                                                                 ; 50
                ld (start_draw_colors+1), hl                                                                    ; 66
                ld (start_draw_colors0+1), hl                                                                   ; 82
                pop hl                           ; end of exec address                                          ; 92

                ; new JPIX from HL to SP
                ld sp, hl                                       
                ex de,hl                         ; last endAddress to HL
                ; Old data to restore to DE, restore data from prev MC drawing steps in HL
color_data_to_restore EQU $+1        
                ld de, 0
                ld (hl),e
                inc hl
                ld (hl),d

                ld hl, JP_VIA_IX_CODE                           ; 10
                ex (sp), hl                                     ; 19
                ld (color_data_to_restore), hl
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

jpix_bank_size  EQU (imageHeight/64 + 2) * jp_ix_record_size

        MACRO DRAW_MULTICOLOR_LINE N?:
                ;ld sp, color_addr + N? * 32 + 16      ; 10
;MC_LINE_N?    jp 00                                   ; 10
                ; total ticks: 20 (30 with ret)
        ENDM                

    MACRO DRAW_RASTR_LINE N?:
RASTRJ_N?       ld hl, 0                                           ; 10
RASTRB_N?

                IF (N? == 17)
                        ld a, iyh               ; 54->54, or 54->53 (dec a)
                ELSEIF (N? == 18)
                        ld a, iyl               ; 53
                ELSEIF (N? == 19)
                        set 1, a                ; 53->53  or 53->51 (res 1, a)
                ELSEIF (N? == 20)
                        res 1, a                ; 51
                ELSEIF (N? == 21)
                        set 0, a                ; 51->51 or 51->50 (res 0, a)
                ELSEIF (N? == 22)
                        res 0, a                ; 50
                ELSEIF (N?  == 0)
page7_depend_5
                        ld a, 0x54              ; 54
                ELSEIF (N? % 8 == 0)
                        set 2, a
                ELSEIF (N? % 8 == 1)
                        nop                     ; 54. 
                        nop
                ELSEIF (N? % 8 == 2)
                        nop                     
                        dec a                   ; 53
                ELSEIF (N? %8 == 3)
                        nop                     ; 53
                        nop
                ELSEIF (N? %8 == 4)
                        dec a                   ; 51
                        dec a
                ELSEIF (N? %8 == 5)
                        nop                     ; 51
                        nop
                ELSEIF (N? %8 == 6)
                        dec a                   ; 50
                        nop
                ELSEIF (N? %8 == 7)
                        .2 nop                  ; 50
                ENDIF                        

                out (#fd), a

RASTRS_N?       ld sp, screen_addr + ((N? + 8) % 24) * 256 + 256       ; 10
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
                ; JP XX command+1 address (end addr)
                pop hl
                IF (N? == 23)
                        ld (hl), low(dec_and_loop)
                        inc hl
                        ld (hl), high(dec_and_loop)
                ELSE
                        ld (hl), low(RASTRJ_N?)
                        inc hl
                        ld (hl), high(RASTRJ_N?)
                ENDIF

                ; Second stack moving, fixed at line + 32
                pop hl
                ld de, color_addr + N? * 32 + 32
                ld (hl), e
                inc hl
                ld (hl), d

                ; First stack moving, the SP value related from line end
                pop hl
                add hl, de
                ex hl, de

                ; First stack moving, LD SP, XX first byte addr
                pop hl                          
                ld (hl), e
                inc hl
                ld (hl), d

                ; begin addr
                pop hl
                IF (N? == 0)
                        ld (stack_top), hl
                ELSE
                        ld (RASTRJ_prevN? + 1), hl
                ENDIF

                ; Line start stack
                inc l   ; it is always aligned here
                ld (hl), low(color_addr + N? * 32 + 16)
                inc l
                ld (hl), d
                ; total: 25


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
        ; 10+7+12=29

        MACRO SET_PAGE page_number
                LD A, #50 + page_number
                OUT (#fd), A
        ENDM

                org start
                JP move_code

BEGIN           DISP code_after_moving

        INCLUDE "draw_off_rastr.asm"

/** ----------- Routines -------------- */

JP_VIA_HL_CODE          equ #e9d9
JP_VIA_IX_CODE          equ #e9dd
jp_ix_record_size       equ 12
jp_write_data_offset    equ 8
data_page_count         equ 4
jp_ix_bank_size         equ (imageHeight/64 + 2) * jp_ix_record_size


/** ----------- Routines end -------------- */

main_entry_point

/*************** Main. ******************/

                ld sp, stack_top

                call copy_page7_screen_and_prepare_interruption_table
                call unpack_and_play_init_screen

        IF (HAS_PLAYER == 1)
1               halt
                jr 1b
        ELSE
                ld hl, after_interrupt_no_player
                ld   (#BFBF+1), hl
                ei
                halt
after_interrupt_no_player
                pop af
        ENDIF                
after_play_intro

                ld a, 7                         ; 7 ticks
                out #fe,a                      ; 11 ticks

        IF (HAS_PLAYER == 1)        
                ld sp, stack_top
                ld hl, music_main
                call  init ; player init

                ld hl, show_final_screen
                ld(endtrack+1), hl
        ENDIF

                call write_initial_jp_ix_table
                call create_jpix_helper
                ld iy, #5453


        ; Pentagon timings
screen_ticks                    equ 71680
screen_start_tick               equ  17988


mc_preambula_delay      equ 46
fixed_startup_delay     equ 32084 + 9 + 13 - 14 + 1
        IF HAS_PLAYER == 1
pl_delay                equ 29432 + 406 + (83186-71680)
        ELSE
pl_delay                equ -202 -171
        ENDIF

                        ; TODO: Now alignInt is called before image unpack.
                        ; so, INTERRUPT_PHASE depend on a image and needs to be updated
                        ; when image list is changed. To avoid this it is needed
                        ; to call alignInt  after image unpack.
                IF HAS_PLAYER == 1
                        ; Player time between after_play_intro - player_callback = 202t.
                        ; 202 - 47 = 155t longer than final ret in align int.
                        ; Calculate phase as ticks between: (alignInt.PrevHandler-after_play_intro-155) % 4
INTERRUPT_PHASE         EQU 2   ; The value in range [0..3].
                ELSE
                        ; 10 - 47 = -37t longer than final ret in align int.
                        ; Calculate phase as ticks between: (alignInt.PrevHandler-after_play_intro+37) % 4
INTERRUPT_PHASE         EQU 1   ; The value in range [0..3].
                ENDIF

create_jpix_delay       equ 1058 * (imageHeight/64)
initial_delay           equ fixed_startup_delay + pl_delay + create_jpix_delay + mc_preambula_delay
sync_tick               equ screen_ticks + screen_start_tick  - initial_delay +  FIRST_LINE_DELAY - INTERRUPT_PHASE

                assert (sync_tick <= 65535 && sync_tick >= 4)
                call static_delay

        IF (HAS_PLAYER == 1)
                LONG_SET_PAGE 7
                LD HL, gigascreen_logo
                LD DE, #c800
                CALL  dzx0_standard
                LD HL, gigascreen_logo_attr
                LD DE, #D900
                CALL  dzx0_standard

                ; Clear  page 7
                ld sp,#c000+6144
                ld de, #aaaa
                ld hl, #5555

                ld c,2
fill_start
                ld a,4
fill_rep
                ld b,32
1               .4 push de
                djnz 1b
                ld b,32
1               .4 push hl
                djnz 1b
                dec a
                jr nz, fill_rep
                dec c
                ld sp,#c000+2048
                jr nz, fill_start

                ld sp,#c000+2048
                ld de, #0000
                ld b,16
1               push de
                djnz 1b

        ENDIF                

                ld hl, start_mc_drawing
                ld (stack_top), hl

max_scroll_offset equ imageHeight - 1

                ; Prepare data
                xor a
                //  Fixed const in AF' is currently unused for colors. It uses af/bc/de/hl instead
                /*
                ld sp,stack_top
                ld hl, COLOR_REG_AF2
                push hl
                ex af, af'
                pop af
                ex af, af'
                */

                ld b,a: ld c,a
                ld a, 5                         ; 7 ticks
                out #fe,a                      ; 11 ticks

                jp non_mc_draw_step
lower_limit_reached:
                ld bc,  max_scroll_offset*2     ; 10 ticks
                ld (saved_bc_value), bc


                ld hl, color_descriptor + 2
                ld (color_descriptor_ptr+1), hl

                ld hl, mc_descriptors + imageHeight/8*10 - 10
                ld (mc_descriptors_ptr+1), hl
                
                ; Update jpix_helper data
                ld hl, update_jpix_helper + imageHeight/2 - 2
                ld (jpix_h_pos+2), hl

                ld a, l
                ld i, a   ; 20            

                ; 10+16=26

        IF (HAS_PLAYER == 1)         
                ; Go to next timings page
         ; Prepare next effect for next scroll run

                ld hl, load_timings_data+1                              ; 10
                ld a, (hl)                                              ; 17
                add 4                                                   ; 24
                and 0x0f                                                ; 31
                ld (hl), a                                              ; 38

/*
run_number      ld a, -4                                                ; 7
                add 4                                                   ; 14
                ld (load_timings_data+1),a                              ; 27
                ld (run_number+1), a                                    ; 40
                or a     ; set flag P for run 1 and 2                   ; 44
*/                
                ld hl, start_draw_colors                                ; 58
                jp PO, effects_run                                      ; 68
normal_run:     
                ld (hl), 0xc3           ; make JP xx                    ; 78
                jp main_loop                                            ; 
effects_run                
                ld (hl), 0xca           ; make JP z, xx
        ENDIF
                jp main_loop                

        IF (HAS_PLAYER == 1)
finish_page7_drawing_cont
                ld hl, start_mc_drawing
                ld (stack_top), hl

                ld bc,-4
                ld hl, pg7_0
                jp hl

                jp page7_drawing_end
                nop
                ; total: 10+16+10+10+4+10=60

                add hl, bc: jp RASTRB_22
                add hl, bc: jp RASTRB_21
                add hl, bc: jp RASTRB_20
                add hl, bc: jp RASTRB_19
                add hl, bc: jp RASTRB_18
                add hl, bc: jp RASTRB_17
                add hl, bc: jp RASTRB_16
                add hl, bc: jp RASTRB_15
                add hl, bc: jp RASTRB_14
                add hl, bc: jp RASTRB_13
                add hl, bc: jp RASTRB_12
                add hl, bc: jp RASTRB_11
                add hl, bc: jp RASTRB_10
                add hl, bc: jp RASTRB_9
                add hl, bc: jp RASTRB_8
                add hl, bc: jp RASTRB_7
                add hl, bc: jp RASTRB_6
                add hl, bc: jp RASTRB_5
                add hl, bc: jp RASTRB_4
                add hl, bc: jp RASTRB_3
                add hl, bc: jp RASTRB_2
                add hl, bc: jp RASTRB_1
pg7_0           add hl, bc: jp RASTRB_0
page7_drawing_end
                ;ld ix, #5051                   ; before: 14t

                ld a, 0x54
                ld (page7_depend_4+1), a
                ld (page7_depend_5+1), a
                dec a
                ld (page7_depend_3+1), a
                ld a, 0x51
                ld (page7_depend_1+1), a
                dec a
                ld (page7_depend_2+1), a          ; after: 40t, dt=26t

                ;ld iy, #5453
                ld iyl, #53             ; 11
                ld a, iyh               ; 19
                and ~8                  ; 26
                ld iyh, a               ; 34

                ld a,#57
                ld (player_pg+1),a
        ENDIF

dec_and_loop:
saved_bc_value  EQU $ +1
                ld bc, 0        ;saved_bc_value here
                dec c
                dec c
main_loop:  
jp_ix_line_delta_in_bank EQU 2 * 6*4

before_update_jpix
                ld hl, mc_drawing_step
                exx

                exa             ;next_step_first_bank
                out (#fd),a
                ; --------------------- update_jp_ix_table --------------------------------
jpix_h_pos      
                ld sp, (update_jpix_helper + imageHeight/2 - 2)
                ld bc, JP_VIA_HL_CODE
                .2 restore_jp_ix
                .2 write_jp_ix_data_via_bc
                .2 restore_jp_ix
                .2 write_jp_ix_data_via_bc
                .2 restore_jp_ix
                .2 write_jp_ix_data_via_bc
                ; total: 465 (with switching page)
                ; ------------------------- update jpix table end
                DRAW_RASTR_LINE_S 23
mc_drawing_step:
                SET_PAGE 6
                ld a, c
                ld (saved_bc_value), a
        IF (HAS_PLAYER == 1)
                and 2
        ENDIF

                ld sp, color_addr + 768                         
                ld de, bank_drawing_common
                exx                                             ; 10+10+4=24
start_draw_colors:
                jp 00                                           ; 10

;*************************************************************************************
        IF (HAS_PLAYER == 1)
page7_effect
                exx
                ld sp, stack_top

                ld hl,finish_page7_drawing_cont
                ex (sp),hl
                ld (finish_page7_drawing_cont+1), hl
                ;10+10+19+16=55

                LD A, #50 + 7+8
                ld (player_pg+1),a

                .2 dec sp

                push bc
                push de
                push iy
                LONG_SET_PAGE 7+8

ef_x            call effect_step
                pop de

                ;ld iy, #5453 + #0808
                set 3, e
                set 3, d
		ld iy, de


                pop de
                pop bc

                SET_PAGE 6+8


                ld a, 0x54 + 8
                ld (page7_depend_5+1), a
                ld (page7_depend_4+1), a
                dec a
                ld (page7_depend_3+1), a
                ld a, 0x51 + 8
                ld (page7_depend_1+1), a
                dec a
                ld (page7_depend_2+1), a          ; after: 37t, dt=23t

                exx
                jp ix	; draw offrastr draw_off_rastr_7..draw_off_rastr_1
        ENDIF

        /************************* no-mc step drawing *********************************************/
non_mc_draw_step
                SET_PAGE 6
                ld a, c
                ld (saved_bc_value), a

                ASSERT high(mc_drawing_step) == high(non_mc_draw_step)
                ld a, low(mc_drawing_step)
                ld (before_update_jpix+1),a    ; update drawing to mc branch

                update_colors_jpix        

                ld sp, color_addr + 768                         ; 10
                ld ix, finish_draw_colors0                      ; 10
                exx                                             ; 4
start_draw_colors0:
                jp 00                                           ; 8
finish_draw_colors0        
                exx
                ; Update off rastr drawing for then next 7 steps
                SET_NEXT_STEP draw_off_rastr_7  ; update mc offrastr jump addr
                ld hl, off_rastr_descriptors
                add hl,bc       ; * 2
                ld sp, hl

                ; 2. Fill JP commands
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

                ; 3. prepare offscreen last lines

                ld hl, off_rastr_sp_delta
                add hl, bc
                ld sp, hl
                FILL_SP_DATA_ALL

                ; 4. prepare offscreen exec data

                ld hl, EXX_DE_JP_HL_CODE
                ld (it0_end), hl
                ld (it7_end), hl
                ld (it6_end), hl
                ld (it5_end), hl
                ld (it4_end), hl
                ld (it3_end), hl
                ld (it2_end), hl

                ; -------------------- MC rastr descriptors

                ld hl, mc_rastr_descriptors_bottom
                add hl,bc       ; * 2
                ld sp, hl

                ; Draw bottom 3-th of rastr during middle 3-th of colors
                pop hl: ld (RASTR0_15+1), hl: ld (RASTR_15+1), hl
                pop hl: ld (RASTR0_14+1), hl: ld (RASTR_14+1), hl
                pop hl: ld (RASTR0_13+1), hl: ld (RASTR_13+1), hl
                pop hl: ld (RASTR0_12+1), hl: ld (RASTR_12+1), hl
                pop hl: ld (RASTR0_11+1), hl: ld (RASTR_11+1), hl
                pop hl: ld (RASTR0_10+1), hl: ld (RASTR_10+1), hl
                pop hl: ld (RASTR0_9+1), hl: ld (RASTR_9+1), hl
                pop hl: ld (RASTR0_8+1), hl: ;ld (RASTR_8+1), hl

                ; Draw middle 3-th of rastr during top 3-th of colors

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
                pop hl: ld (RASTR0_0+1), hl: ;ld (RASTR_0+1), hl

                ; Draw top 3-th of rastr during bottom 3-th of colors
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
                pop hl: ld (RASTR0_16+1), hl: ;ld (RASTR_16+1), hl

                ; mc_rastr_descriptors_top

                ld hl, mc_rastr_descriptors_top-7*2
                add hl, bc
                ld sp, hl
                pop hl: ld (upd_rastr_1_0+1), hl
                pop hl: ld (upd_rastr_2_0+1), hl
                pop hl: ld (upd_rastr_3_0+1), hl
                pop hl: ld (upd_rastr_4_0+1), hl
                pop hl: ld (upd_rastr_5_0+1), hl
                pop hl: ld (upd_rastr_6_0+1), hl
                pop hl: ld (upd_rastr_7_0+1), hl

                ld hl, mc_rastr_descriptors_top-7*2 + 64*2
                add hl, bc
                ld sp, hl
                pop hl: ld (upd_rastr_1_1+1), hl
                pop hl: ld (upd_rastr_2_1+1), hl
                pop hl: ld (upd_rastr_3_1+1), hl
                pop hl: ld (upd_rastr_4_1+1), hl
                pop hl: ld (upd_rastr_5_1+1), hl
                pop hl: ld (upd_rastr_6_1+1), hl
                pop hl: ld (upd_rastr_7_1+1), hl


                ld hl, finish_non_mc_drawing_cont
                ld (stack_top), hl

                ld de, finish_off_drawing_0
                jp it0_start
finish_off_drawing_0
                ; non-MC part rastr
                exa
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

                ld hl, bank_drawing_common2
                DRAW_ONLY_RASTR_LINE_22

finish_non_mc_drawing_cont:
                SET_PAGE 6

                ld bc, (saved_bc_value)

mc_descriptors_ptr
                ld sp, mc_descriptors           ; 10
                ld hl, -10                      ; 20
                add hl, sp                      ; 31
                ld (mc_descriptors_ptr+1), hl   ; 47

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

                ; update ports [50..54] for the next step 7
                ;LD a, #3D               ; dec a (value #54 -> #53)
                ;ld (RASTRS_17 - 3), a
                dec iyh
                LD a, #8F               ; res 1, a (value #53 -> #51)
                ld (RASTRS_19 - 3), a
                LD A, #87               ; res 0, a (value #51 -> #50)
                ld (RASTRS_21 - 3), a

                dec bc
                dec c
                ; compare to -1
                ld a, b
                inc a
                jp z, lower_limit_reached

                ld sp, jpix_h_pos+2
                pop hl
                dec hl
                dec l
                push hl

                ld a, l                 ; 4
                ld i, a                 ; 20
                
                ; 10+10+6+4+11=41

                ld (saved_bc_value), bc
                jp main_loop                        ; 12 ticks


start_mc_drawing:
                ; timing here on first frame: 71680 * 2 + 17988 + 224*6 - (19 + 10) - 20 = 162643-6=162637 (-phase)
                ; timing here on first frame: 162637 - 202 = 162435 + 71680=234115 (-phase)
                ; after non-mc frame: 144704, between regular lines: 71680-224 = 71456. double frames=142912

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

        IF (HAS_PLAYER == 1)
show_final_screen
                SET_PAGE 4
                ld (set_player_page+1),a

                ld sp,stack_top
                ld hl,final_mus
                call  init ; player init
                call play

                SET_PAGE 7
                ld ixl,a                

                ld hl, end_mus_finished
                ld(endtrack+1), hl

                ; restore interruption vector
                LD   hl, #BE00
                LD   de, #BE01
                LD   bc, #0100
                LD   (hl), #BF
                LD   a, h
                LDIR
                ld a, 0xbe
                ld i, a
                ei

                ; Unpack final screen to the page 7
                LD DE, #c000
                LD HL, final_screen
                CALL  dzx0_standard
                
                halt
                call copy_7_to_5_animated

1               halt
                jr 1b           ; finish play music

end_mus_finished
                call stop
                di
                halt        
        ENDIF   ; has_player

copy_5_to_7_animated
                ld hl, #4000
                ld (cp1+1), hl
                ld a,#fa                ; set 7,d
                ld (upd_de_anim+1),a
                ld (upd_de_anim2+1),a
                jr copy_page_animated

copy_7_to_5_animated
                ld hl, #c000
                ld (cp1+1), hl
                ld a,#ba                ; res 7,d
                ld (upd_de_anim+1),a
                ld (upd_de_anim2+1),a

copy_page_animated
                ; Copy page 7 to Page 5 with animation
                ld c,32
next_column
                exx
                ld bc, 32
cp1             ld hl, #c000
                ld de, hl
upd_de_anim     res 7,d

                ld a,l
                and 7
                out (#fe),a
                exx
                ld b,192 + 24
copy_column     exx
                ld a,(hl)
                ld (de),a
                add hl,bc
                ld de,hl
upd_de_anim2    res 7,d
                exx
                djnz copy_column
                halt

                ld hl, cp1+1
                inc (hl)
                dec c
                jr nz, next_column
                ret

/**     ----------------------------------------- routines -----------------------------------------
 *      All routines are called once before drawing. They can be moved to another page to free memory.
 */

write_initial_jp_ix_table
                xor a
page_loop:
                ld c, a
                srl a
                jr c, continue_page        
                ld sp, jpix_table + jp_write_data_offset
continue_page:
                ld b, 3

                cp 2
                ccf
                adc #58
                out (#fd), a

                ld a, c

                ld de, JP_VIA_HL_CODE
.rep:           write_jp_ix_data_via_de
                write_jp_ix_data_via_de
                ld hl, jp_ix_record_size - 4
                add hl, sp
                ld sp, hl
                djnz .rep

                ld sp, jpix_table + jp_ix_bank_size + jp_write_data_offset

                inc a
                cp 8
                jr nz, page_loop

                ; write initial color data to restore
                SET_PAGE 6+8

                ld hl, color_descriptor + imageHeight / 2 - 2   ; 10
                ld sp, hl                                       ; 6
                pop hl                                          ; 10
                ld sp, hl
                pop hl
                ld (color_data_to_restore), hl                  ; 16

                ld sp, stack_top - 2
                ret

create_jpix_helper
                ASSERT(imageHeight / 4 < 256)
                ld hl, update_jpix_helper
                ld iy, jpix_table
                ld ixl, imageHeight / 64
.loop2          ld a, 16
                push iy
                pop de
                ld bc, jpix_bank_size
.loop:          ld (hl), e
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
        IF (HAS_PLAYER == 1)
                INCLUDE "draw_font.asm"
                INCLUDE "effects_by_run.asm"
        ENDIF                
encoded_text
                INCBIN "generated_code/encoded_text.dat"
final_screen        
                INCBIN "generated_code/final.scr.zx0"
gigascreen_logo        
                INCBIN "generated_code/scroller.scr.zx0"
gigascreen_logo_attr
                INCBIN "generated_code/scroller_attr.scr.zx0"

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
                ;INCBIN "generated_code/mt_and_rt_reach_descriptor.z80"
                ;INCBIN "generated_code/multicolor.z80"
                INCBIN "generated_code/ram2.zx0"
main_data_end
init_screen_i1
                INCBIN "generated_code/screen2.scr.i1", 0, 6144/2
                INCLUDE "alignint.asm"
encoded_text_end        
                INCLUDE "startup_routines.asm"

static_data_page2 EQU #c000 - 1024-512
                ORG static_data_page2

buffer_data     defs 256, 0
buffer_data2    defs 256, 0

font_data   
                INCLUDE "generated_code/reduced_font.asm"
font_data_end
                ASSERT font_data_end - font_data <= 512

update_jpix_helper   EQU #c000 - 512
jpix_helper_end EQU  update_jpix_helper  + imageHeight/2
                ASSERT data_start >= jpix_helper_end
                ASSERT $ <= update_jpix_helper
                ASSERT simple_scroller_end < static_data_page2
                ASSERT generated_code + RAM2_UNCOMPRESSED_SIZE < static_data_page2
                DISPLAY	"Packed Page 2 free ", /D, static_data_page2 - simple_scroller_end
                DISPLAY	"Unpacked Page 2 free ", /D, static_data_page2 - (generated_code + RAM2_UNCOMPRESSED_SIZE), " bytes"

                ORG #c000
                PAGE 0
                ;INCBIN "generated_code/timings0.dat"
                ;INCBIN "generated_code/jpix0.dat"
                ;INCBIN "generated_code/main0.z80"
                ;INCBIN "generated_code/reach_descriptor0.z80"
                INCBIN "generated_code/ram0.zx0"
ram0_end
first_multicolor_delta
                INCBIN "generated_code/first_screen.zx0"
page0_end
                DISPLAY	"Packed Page 0 free ", /D, 65536 - page0_end, " bytes"
                DISPLAY	"Unpacked Page 0 free ", /D, 16384 - RAM0_UNCOMPRESSED_SIZE, " bytes"

                ORG #c000
                PAGE 1
                ;INCBIN "generated_code/timings1.dat"
                ;INCBIN "generated_code/jpix1.dat"
                ;INCBIN "generated_code/main1.z80"
                ;INCBIN "generated_code/reach_descriptor1.z80"
                INCBIN "generated_code/ram1.zx0"
ram1_end
init_screen_i0
                INCBIN "generated_code/screen2.scr.i0"
page1_end
                DISPLAY	"Packed Page 1 free ", /D, 65536 - page1_end, " bytes"
                DISPLAY	"Unpacked Page 1 free ", /D, 16384 - RAM1_UNCOMPRESSED_SIZE, " bytes"

                ORG #c000
                PAGE 3
                ;INCBIN "generated_code/timings2.dat"
                ;INCBIN "generated_code/jpix2.dat"
                ;INCBIN "generated_code/main2.z80"
                ;INCBIN "generated_code/reach_descriptor2.z80"
                INCBIN "generated_code/ram3.zx0"
ram3_end
screen3
                INCBIN "generated_code/screen3.scr.zx0"
page3_end        
                DISPLAY	"Packed Page 3 free ", /D, 65536 - page3_end, " bytes"
                DISPLAY	"Unpacked Page 3 free ", /D, 16384 - RAM3_UNCOMPRESSED_SIZE, " bytes"

                ORG #c000
                PAGE 4
                INCBIN "generated_code/timings3.dat"
jpix_table      
                INCBIN "generated_code/jpix3.dat"
                INCBIN "generated_code/main3.z80"
                INCBIN "generated_code/reach_descriptor3.z80"
final_mus        
                INCBIN "resources/final.mus"
page4_end        
                DISPLAY	"Page 4 free ", /D, 65536 - page4_end, " bytes"

                ORG #c000
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
                INCBIN "generated_code/mc_rastr_top_descriptors.dat", (imageHeight-7)*2, 7*2
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

                ORG #c000
                PAGE 7
                INCBIN "resources/screen1.scr", 0, 6144
                BLOCK 768, #7
music_main
                INCBIN "resources/main.mus"
page7_end
                DISPLAY	"Page 7 free ", /D, 65536 - $, " bytes"

/*************** Commands to SJ asm ******************/

                SAVESNA "build/scroller.sna", start

                EMPTYTRD "build/scroller.trd" ;create empty TRD image

                SAVETRD "build/scroller.trd","main.C", start, $C000 - start

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

                ; Just a fake address to load files into TRD
                ORG #4000
fix128k_script
                incbin "fix128k.C"
boot_start
                incbin "boot.B"
boot_end        
                SAVETRD "build/scroller.trd","fix128k.C", fix128k_script, boot_start - fix128k_script
                SAVETRD "build/scroller.trd","boot.B", boot_start, boot_end - boot_start, 1

                // Save tap
tap_boot_start
                incbin "scroller.B"
tap_boot_end        
                EMPTYTAP "build/scroller.tap" ;create empty TAP image
                SAVETAP "build/scroller.tap", BASIC, "scroller", tap_boot_start, tap_boot_end - tap_boot_start, 1
                SAVETAP "build/scroller.tap", CODE, "fix128k", fix128k_script, boot_start - fix128k_script
                PAGE 0
                SAVETAP "build/scroller.tap", CODE, "ram0", $C000, page0_end - $C000
                PAGE 1
                SAVETAP "build/scroller.tap", CODE, "ram1",$C000, page1_end - $C000
                PAGE 3
                SAVETAP "build/scroller.tap", CODE, "ram3",$C000, page3_end - $C000
                PAGE 4
                SAVETAP "build/scroller.tap", CODE, "ram4",$C000, page4_end - $C000
                PAGE 6
                SAVETAP "build/scroller.tap", CODE, "ram6", $C000, page6_end - $C000
                PAGE 7
                SAVETAP "build/scroller.tap", CODE, "ram7", $C000, page7_end - $C000
                SAVETAP "build/scroller.tap", CODE, "main", start, $C000 - start
