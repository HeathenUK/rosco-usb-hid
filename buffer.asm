        section .text
        align 2
; ASM callable - pass character in D0
;
; Expects to be called in interrupt context, not re-entrant.
;
buffer_char::
        move.l  D1,-(A7)                        ; Stash regs
        move.l  A0,-(A7)
        lea.l   BUF,A0                          ; Point A0 to the internal buffer
        move.w  BUF_W_PTR,D1                    ; Get the current write pointer into D1
        move.b  D0,(A0,D1)                      ; Buffer the character
        addi.w  #1,D1                           ; Increment write pointer...
        andi.w  #$3ff,D1                        ; ... keep it within range
        move.w  D1,BUF_W_PTR                    ; ... and store it back in mem
        move.l  (A7)+,A0                        ; Restore regs
        move.l  (A7)+,D1
        rts                                     ; Fin.


; C-callable - pass a 1K buffer on the stack. Returns actual count.
;
unbuffer::
        movem.l D1-D2/A0-A1,-(A7)
        move.w  SR,D2                           ; Save SR
        and.w   #$F0FF,SR                       ; No interrupts for a bit

        lea.l   BUF,A0                          ; Load internal buffer into A0
        move.l  20(A7),A1                       ; Load out buffer into A1
        clr.l   D0                              ; Zero return value

        move.w  BUF_R_PTR,D1                    ; D1 is read pointer to internal buffer

.loop
        cmp.w   BUF_W_PTR,D1                    ; Are pointers equal?
        beq.s   .done                           ; Leave now if so...

        move.b  (A0,D1),(A1)+                   ; Copy byte into out buffer
        addi.w  #1,D0                           ; Increment return value (count)
        addi.w  #1,D1                           ; Increment read pointer...
        andi.w  #$3FF,D1                        ; ... and wrap if >1023
        bra.s   .loop                           ; Let's go again!

.done:
        move.w  D1,BUF_R_PTR                    ; Store updated read pointer
        move.w  D2,SR                           ; Re-enable interrupts
        movem.l (A7)+,D1-D2/A0-A1
        rts

        section .bss
        align 2
BUF         ds.b        1024                    ; 1K might be a bit much, but YMMV
BUF_R_PTR   dc.w        0                       ; Read pointer
BUF_W_PTR   dc.w        0                       ; Write pointer
