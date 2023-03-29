
#ifndef __VMLINUX_ARCH_H__
#define __VMLINUX_ARCH_H__
#if defined(__TARGET_ARCH_x86)
    #include "../../../../../../../lib/internal/ebpf/vmlinux_x86_64.h"
#elif defined(__TARGET_ARCH_arm64)
    #include "../../../../../../../lib/internal/ebpf/vmlinux_arm64.h"
#endif

#endif