Description

The majority of the code and work is in the file sys/kern/zones.c, other changes are to simply support the implementation.

For this project, processes must be isolated within 'zones'. Therefore, signals cannot cross zones. Furthermore, zones should not know what processes exist in other zones.

Changed Files in the kernel

bin/ps/extern.h
bin/ps/keyword.c
bin/ps/print.c
bin/ps/ps.1
bin/ps/ps.c
include/Makefile
include/zones.h
lib/libc/shlib_version
lib/libc/sys/Makefile.inc
sys/conf/files
sys/kern/init_sysent.c
sys/kern/kern_exit.c
sys/kern/kern_fork.c
sys/kern/kern_sig.c
sys/kern/kern_sysctl.c
sys/kern/syscalls.c
sys/kern/syscalls.master
sys/kern/zones.c
sys/sys/proc.h
sys/sys/syscall.h
sys/sys/syscallargs.h
sys/sys/sysctl.h
sys/sys/types.h
usr.bin/pkill/pkill.1
usr.bin/pkill/pkill.c
usr.sbin/zone/Makefile
usr.sbin/zone/main.c
usr.sbin/zone/zone.8
