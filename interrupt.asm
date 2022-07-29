        section .text
        align 2
; DUART register offsets
DUART_IMR       equ     $0a
DUART_ISR       equ     $0a

DUART_MRA       equ     $0
DUART_SRA       equ     $2
DUART_CSRA      equ     $2
DUART_CRA       equ     $4
DUART_RBA       equ     $6

DUART_MRB       equ     $10
DUART_SRB       equ     $12
DUART_CSRB      equ     $12
DUART_CRB       equ     $14
DUART_RBB       equ     $16

; DUART interrupt vector address
VECADDR         equ     $45<<2


; C callable - void install_interrupt_dual(CHAR_DEVICE *device)
;
install_interrupt_dual::
        move.l  D0,-(A7)
        move.l  A0,-(A7)

        move.w  SR,D0                           ; Save SR
        and.w   #$F0FF,SR                       ; No interrupts for a bit

        move.l  12(A7),A0                       ; Get BASEADDR structure
        move.l  (A0),A0                         ; Get base address

        ; Ensure UART A and B are set up just like we like them...
        move.b  #$88,DUART_CSRA(A0)             ; 115K2
        move.b  #$10,DUART_CRA(A0)              ; Reset to MR1B
        move.b  #$13,DUART_MRA(A0)              ; Ensure No RTS, RxRDY, Char, No parity, 8 bits
        move.b  #$07,DUART_MRA(A0)              ; (Normal, No TX CTS/RTS, 1 stop bit)
        
        move.b  #$88,DUART_CSRB(A0)             ; 115K2
        move.b  #$10,DUART_CRB(A0)              ; Reset to MR1B
        move.b  #$13,DUART_MRB(A0)              ; Ensure No RTS, RxRDY, Char, No parity, 8 bits
        move.b  #$07,DUART_MRB(A0)              ; (Normal, No TX CTS/RTS, 1 stop bit)

        move.l  A0,BASEADDR                     ; Store BASEADDR base pointer
        move.l  VECADDR,CHAIN                   ; Store existing handler
        move.l  #HANDLER,VECADDR                ; And install new one

        move.b  #$2a,DUART_IMR(A0)              ; Enable RXRDY/RXFULL interrupt for channels A and B and keep counter going
        move.w  D0,SR                           ; Re-enable interrupts

        move.l  (A7)+,A0
        move.l  (A7)+,D0
        rts

; C callable - void install_interrupt_a(CHAR_DEVICE *device)
;
install_interrupt_a::
        move.l  D0,-(A7)
        move.l  A0,-(A7)

        move.w  SR,D0                           ; Save SR
        and.w   #$F0FF,SR                       ; No interrupts for a bit

        move.l  12(A7),A0                       ; Get BASEADDR structure
        move.l  (A0),A0                         ; Get base address

        ; Ensure UART A is set up just like we like it...
        move.b  #$88,DUART_CSRA(A0)             ; 115K2
        move.b  #$10,DUART_CRA(A0)              ; Reset to MR1B
        move.b  #$13,DUART_MRA(A0)              ; Ensure No RTS, RxRDY, Char, No parity, 8 bits
        move.b  #$07,DUART_MRA(A0)              ; (Normal, No TX CTS/RTS, 1 stop bit)

        move.l  A0,BASEADDR                     ; Store BASEADDR base pointer
        move.l  VECADDR,CHAIN                   ; Store existing handler
        move.l  #HANDLER,VECADDR                ; And install new one

        move.b  #$a,DUART_IMR(A0)               ; Enable RXRDY/RXFULL interrupt for channels A and B and keep counter going
        move.w  D0,SR                           ; Re-enable interrupts

        move.l  (A7)+,A0
        move.l  (A7)+,D0
        rts

; C callable - void install_interrupt_b(CHAR_DEVICE *device)
;
install_interrupt_b::
        move.l  D0,-(A7)
        move.l  A0,-(A7)

        move.w  SR,D0                           ; Save SR
        and.w   #$F0FF,SR                       ; No interrupts for a bit

        move.l  12(A7),A0                       ; Get BASEADDR structure
        move.l  (A0),A0                         ; Get base address

        ; Ensure UART B is set up just like we like it...
        move.b  #$88,DUART_CSRB(A0)             ; 115K2
        move.b  #$10,DUART_CRB(A0)              ; Reset to MR1B
        move.b  #$13,DUART_MRB(A0)              ; Ensure No RTS, RxRDY, Char, No parity, 8 bits
        move.b  #$07,DUART_MRB(A0)              ; (Normal, No TX CTS/RTS, 1 stop bit)

        move.l  A0,BASEADDR                     ; Store BASEADDR base pointer
        move.l  VECADDR,CHAIN                   ; Store existing handler
        move.l  #HANDLER,VECADDR                ; And install new one

        move.b  #$28,DUART_IMR(A0)              ; Enable RXRDY/RXFULL interrupt for channels A and B and keep counter going
        move.w  D0,SR                           ; Re-enable interrupts

        move.l  (A7)+,A0
        move.l  (A7)+,D0
        rts


; C callable - void remove_interrupt(void)
;
remove_interrupt::
        movem.l D0/A0-A1,-(A7)

        move.w  SR,D0                           ; Save SR
        and.w   #$F0FF,SR                       ; No interrupts for a bit

        move.l  CHAIN,VECADDR                   ; Restore original handler
        move.l  BASEADDR,A0                     ; Get BASEADDR structure
        move.b  #$08,DUART_IMR(A0)              ; Just keep counter going

        move.w  D0,SR                           ; Re-enable interrupts
        movem.l (A7)+,D0/A0-A1
        rts


; The interrupt handler (also chains to the original handler)
HANDLER:
        movem.l D0-D1/A0-A1,-(A7)
        move.l  BASEADDR,A0                     ; Load BASEADDR pointer

.loop:
        move.b  DUART_ISR(A0),D0
        btst    #1,D0                           ; Check if ready A bit is set
        beq.s   .chkb                           ; If not, also check if ready B is set (chkb)
        move.l  #buffer_char_a,A1               ; But if it is, use buffer_char_a

.cont:
        move.b  DUART_SRA,D0                    ; Check if error bits are set for channel A
        and.b   #$F0,D0
        bne.s   .error_a
        move.b  DUART_SRB,D0                    ; Check if error bits are set for channel B
        and.b   #$F0,D0
        bne.s   .error_b

        btst    #1,D0                           ; Check if ready A bit is set
        beq.s   .chkb2                          ; If not, also check if ready B is set (chkb2)
        move.b  DUART_RBA(A0),D0                ; Grab character from A receive buffer
        jsr     (A1)                            ; Call buffer_char_a
        bra.s   .loop

.chkb2: 
        btst    #5,D0                           ; Check if ready A bit is set
        beq.s   .chkb2                          ; If not, also check if ready B is set (chkb2)
        move.b  DUART_RBB(A0),D0                ; Grab character from B receive buffer
        jsr     (A1)                            ; Call buffer_char_b
        bra.s   .loop                           ; And continue testing...

.chkb:  
        btst    #5,D0                           ; Check if ready B bit is set
        beq.s   .chain                          ; Just bail now if not (probably a timer tick)
        move.l  #buffer_char_b,A1               ; But if set, use buffer_char_b
        bra.s   .cont

.error_a
        move.b  D0,D1                           ; Every day I'm shufflin' (registers)
        btst    #4,D1                           ; Overrun error?
        beq.s   .notoverrun        
        move.b  #$40,DUART_CRB(A0)              ; Reset overrun error status

.notoverrun_a
        ifd ERROR_REPORTING

        btst    #5,D1                           ; Parity error?
        beq.s   .notparity

        move.b  #'P',D0
        jsr     (A1)                            ; Call buffer_char
        move.b  #'?',D0
        jsr     (A1)                            ; Call buffer_char

.notparity_a
        btst    #6,D1                           ; Frame error?
        beq.s   .notframe

        move.b  #'F',D0
        jsr     (A1)                            ; Call buffer_char
        move.b  #'?',D0
        jsr     (A1)                            ; Call buffer_char

.notframe_a
        btst    #7,D1                           ; Break?
        beq.s   .notbreak

        move.b  #'B',D0
        jsr     (A1)                            ; Call buffer_char
        move.b  #'?',D0
        jsr     (A1)                            ; Call buffer_char

.notbreak_a
        endif ; ERROR_REPORTING

        move.b  DUART_RBB(A0),D0                ; Get faulty character...
        jsr     (A1)                            ; ... and buffer it anyway (YOLO)

        bra.s   .loop

.chain
        movem.l (A7)+,D0-D1/A0-A1               ; Restore regs...
        move.l  CHAIN,-(A7)                     ; And "return to" the original ISR
        rts


        section .bss
        align 2
BASEADDR    dc.l        0                       ; DUART base address from CHAR_DEVICE struct     
CHAIN       dc.l        0                       ; Chained ISR (timer tick probably)
