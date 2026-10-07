/* Userland SceNetCtl private exports recovered from the Vita Settings module. */
.syntax unified
.section .vitalink.fstubs.SceNetCtl,"ax",%progbits
.balign 4

.macro NETCTL_STUB symbol, nid
.global \symbol
.type \symbol, %function
\symbol:
    .word 0
    .word 0xC6083AE1
    .word \nid
    .word 0xE320F000
.size \symbol, .-\symbol
.endm

NETCTL_STUB dm_netctl_job_register, 0xAC326DB7
NETCTL_STUB dm_netctl_scan_start,   0x6A56FC3A
NETCTL_STUB dm_netctl_scan_wait,    0x300E8877
NETCTL_STUB dm_netctl_scan_count,   0x47980B9B
NETCTL_STUB dm_netctl_scan_get,     0x92EB6B1A
NETCTL_STUB dm_netctl_profile_submit, 0x17FB44C4
