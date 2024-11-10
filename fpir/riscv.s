
        .option norvc
        .section .text.entry
        .global _entry
_entry:
        csrr a1, mhartid
        bnez a1, spin            #all but the first hart spin

        ## TODO this isn't actually set up right in the linkerscript
        ## for more than one hart
        csrr a1, mhartid
        li a0, 0x3000           #2 page stack + guard page
        mul a1, a1, a0          #offset by hart id
        .extern _stacks_end
        la a2, _stacks_end      # this is the top byte for hart 0
        sub sp, a2, a1

        la a0, handler
        csrw mepc, a0

        .extern forsp_main
        la a1, BM_TEXT_PTR
        la a2, BM_TEXT
        sd a2, (a1)
        call forsp_main
spin:
        wfi
        j spin

handler:
        .extern ringbuf
        la a1, ringbuf
        add a0, a0, 0x30
        li t0, 0x41
        li t1, 0x4e
        li t2, 0x54
        sb t0, (a1)
        sb t1, 1(a1)
        sb t2, 2(a1)
        sb a0, 3(a1)
        j spin
