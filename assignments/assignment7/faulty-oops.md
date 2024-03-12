# OOPS COMPLETE LOGS

```
Unable to handle kernel NULL pointer dereference at virtual address 0000000000000000
Mem abort info:
  ESR = 0x96000045
  EC = 0x25: DABT (current EL), IL = 32 bits
  SET = 0, FnV = 0
  EA = 0, S1PTW = 0
  FSC = 0x05: level 1 translation fault
Data abort info:
  ISV = 0, ISS = 0x00000045
  CM = 0, WnR = 1
user pgtable: 4k pages, 39-bit VAs, pgdp=0000000042034000
[0000000000000000] pgd=0000000000000000, p4d=0000000000000000, pud=0000000000000000
Internal error: Oops: 96000045 [#1] SMP
Modules linked in: hello(O) faulty(O) scull(O)
CPU: 0 PID: 162 Comm: sh Tainted: G           O      5.15.18 #1
Hardware name: linux,dummy-virt (DT)
pstate: 80000005 (Nzcv daif -PAN -UAO -TCO -DIT -SSBS BTYPE=--)
pc : faulty_write+0x14/0x20 [faulty]
lr : vfs_write+0xa8/0x2b0
sp : ffffffc008d23d80
x29: ffffffc008d23d80 x28: ffffff80020e4c80 x27: 0000000000000000
x26: 0000000000000000 x25: 0000000000000000 x24: 0000000000000000
x23: 0000000040001000 x22: 0000000000000012 x21: 0000005565612a70
x20: 0000005565612a70 x19: ffffff800208b600 x18: 0000000000000000
x17: 0000000000000000 x16: 0000000000000000 x15: 0000000000000000
x14: 0000000000000000 x13: 0000000000000000 x12: 0000000000000000
x11: 0000000000000000 x10: 0000000000000000 x9 : 0000000000000000
x8 : 0000000000000000 x7 : 0000000000000000 x6 : 0000000000000000
x5 : 0000000000000001 x4 : ffffffc0006f7000 x3 : ffffffc008d23df0
x2 : 0000000000000012 x1 : 0000000000000000 x0 : 0000000000000000
Call trace:
 faulty_write+0x14/0x20 [faulty]
 ksys_write+0x68/0x100
 __arm64_sys_write+0x20/0x30
 invoke_syscall+0x54/0x130
 el0_svc_common.constprop.0+0x44/0xf0
 do_el0_svc+0x40/0xa0
 el0_svc+0x20/0x60
 el0t_64_sync_handler+0xe8/0xf0
 el0t_64_sync+0x1a0/0x1a4
Code: d2800001 d2800000 d503233f d50323bf (b900003f) 
---[ end trace 2ee9b1b3b2ca233c ]---
```

# OBJDMP LOGS

```
0000000000000000 <faulty_write>:
   0:   d503245f bti    c
   4:   d2800001 mov    x1, #0x0                   // #0
   8:   d2800000 mov    x0, #0x0                   // #0
   c:   d503233f paciasp
  10:   d50323bf autiasp
  14:   b900003f str    wzr, [x1]
  18:   d65f03c0 ret
  1c:   d503201f nop
```

# DESCRITPION

    * The problematic section in the write program is the faulty_write function.
    * The program counter indicates an offset of 0x14 bytes from the beginning of the function, and the total function size is 0x20.
    * The crash occurred due to a NULL pointer dereference at this specific location.
    * Analyzing the OBJDUMP, it is evident that the crash is caused by storing the value at address 0x0 into x1 using the str instruction.
    * The NULL pointer dereference leads to a kernel crash.
    * The issue stems from attempting to dereference a pointer with a value of 0x0.
    * The offset +0x14 and the total function size of 0x20 pinpoint the exact location of the crash in the faulty_write function.
