; SPDX-License-Identifier: BSD-2-Clause
; SPDX-FileCopyrightText: 2026 Stefan Reinauer
;
; Read-only hardware probes. Leave the machine's bus timeout policy alone.
; Recover through the original Supervisor frame, not the CPU-specific
; bus/address-error frame (68000, 010, 020/030, 040 and 060 all differ).

        INCDIR  "include:"
        INCLUDE "exec/execbase.i"
        INCLUDE "lvo/exec_lib.i"

        SECTION Code,CODE
        MACHINE 68000

        XDEF    _berr_probe_byte
        XDEF    _berr_probe_long

BusErrorVector      EQU     $8
AddressErrorVector  EQU     $C

; C arguments: address, destination. Only publish a successful read,
; after restoring the exception vectors and leaving supervisor mode.
_berr_probe_byte:
        moveq   #0,d0
        bra.s   probe
_berr_probe_long:
        moveq   #1,d0
probe:
        movem.l d2-d3/a2-a6,-(sp)
        move.l  d0,d2
        move.l  4+28(sp),a4
        move.l  8+28(sp),a2
        lea     probe_super(pc),a5
        move.l  $4.w,a6
        jsr     _LVOSupervisor(a6)
        tst.l   d0
        bne.s   probe_return
        tst.l   d2
        beq.s   probe_byte_result
        move.l  d1,(a2)
        bra.s   probe_return
probe_byte_result:
        move.b  d1,(a2)
probe_return:
        movem.l (sp)+,d2-d3/a2-a6
        rts

probe_super:
        ori.w   #$0700,sr           ; RTE restores the previous mask
        move.w  AttnFlags(a6),d3
        suba.l  a0,a0
        btst    #AFB_68010,d3
        beq.s   probe_vectors
        MACHINE 68010
        movec   vbr,a0
        MACHINE 68000
probe_vectors:
        move.l  BusErrorVector(a0),-(sp)
        move.l  AddressErrorVector(a0),-(sp)
        move.l  sp,a3              ; checkpoint above any fault frame
        nop                       ; complete earlier bus cycles first
        lea     berr_handler(pc),a1
        move.l  a1,BusErrorVector(a0)
        lea     probe_fault(pc),a1
        move.l  a1,AddressErrorVector(a0)
        nop                       ; finish vector writes before reading
        tst.l   d2
        beq.s   probe_byte
        move.l  (a4),d1
        bra.s   probe_read_done
probe_byte:
        move.b  (a4),d1
probe_read_done:
        nop                       ; drain the read before unhooking
        moveq   #0,d0
        bra.s   probe_done

berr_handler:
        ; A 060 branch prediction error alone must retry, not report
        ; absent hardware. Flush its branch cache before either path.
        btst    #AFB_68060,d3
        beq.s   probe_fault
        btst    #2,15(sp)          ; format 4 FSLW: BPE
        beq.s   probe_fault
        move.l  d0,-(sp)
        MACHINE 68060
        movec   cacr,d0
        or.l    #$00400000,d0      ; CABC
        movec   d0,cacr
        MACHINE 68000
        move.l  16(sp),d0
        and.l   #$00007ff1,d0      ; real access errors or SEE
        bne.s   probe_fault       ; checkpoint also discards saved d0
        move.l  (sp)+,d0
        rte

probe_fault:
        move.l  a3,sp             ; discard the entire fault frame
        moveq   #-1,d0
probe_done:
        move.l  (sp)+,AddressErrorVector(a0)
        move.l  (sp)+,BusErrorVector(a0)
        rte                       ; original Supervisor frame and SR

        END
