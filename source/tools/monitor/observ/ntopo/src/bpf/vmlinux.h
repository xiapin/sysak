
#ifndef __VMLINUX_ARCH_H__
#define __VMLINUX_ARCH_H__
#if defined(__TARGET_ARCH_x86)
    #include "../../../../../../lib/internal/ebpf/coolbpf/arch/x86_64/vmlinux.h"
#elif defined(__TARGET_ARCH_arm64)
    #include "../../../../../../lib/internal/ebpf/coolbpf/arch/aarch64/vmlinux.h"
#endif

#endif