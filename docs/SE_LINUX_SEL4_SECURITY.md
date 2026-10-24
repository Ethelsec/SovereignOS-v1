# seL4 Security Hardening & Deployment Guide

## Understanding seL4 Deployment

### How seL4 Works
- **seL4 is a microkernel** - Not a full OS, runs minimal services
- **Capability-based security** - Everything requires explicit capabilities
- **Formal verification** - seL4 has been formally verified for certain properties
- **Usually runs in VMs/embedded** - Often on ARM/x86 with hypervisor support
- **IPC via endpoints** - Components communicate through capability-protected IPC

### Running seL4
seL4 is typically deployed as:
1. **On bare metal** - Direct hardware access
2. **As a guest VM** - Under a hypervisor (like KVM, Xen) 
3. **In simulation** - Using QEMU (we have qemu-system-arm, can build for ARM targets)
4. **With CAmkES framework** - For component-based systems

**In our current environment:**
- We have seL4 source in `~/seL4/`
- QEMU available for ARM (`qemu-system-aarch64`)
- No x86_64 seL4 VM running currently - it's source code we need to build
- To actually run seL4, we need to build an image for a specific platform

## Security Hardening for seL4

### Principle of Least Privilege
The seL4 component in God's Eye must:
- **Only access what it needs** - Via the shared buffer gateway
- **No direct hardware access** unless absolutely necessary
- **Minimal capabilities** - Only IPC endpoint to buffer gateway
- **No network access** directly
- **No filesystem access** outside designated areas

### Buffer Gateway Security
Our shared buffer (`shared-buffer/guest_sel4_buffer.c`) must be hardened:

```c
// Security measures needed:
// 1. Frame validation with magic numbers
// 2. Size limits enforced
// 3. Source/destination validation
// 4. CRC32 checksums
// 5. Rate limiting
// 6. Access control lists
// 7. Memory bounds checking
```

### Gateway Design
```
[Guest OS] --(validated)--> [Buffer Gateway] --(capability-gated)--> [seL4]
                              |                        |
                           [ACL/Filter]           [Minimal Caps]
                           [Rate Limit]           [No Direct HW]
                           [Checksum]             [IPC Only]
```

## Making It Secure

### 1. Harden the Shared Buffer
Update `shared-buffer/guest_sel4_buffer.c` with security controls:
- Strict frame validation
- Rate limiting per source
- Sequence number validation
- Checksum verification
- Drop on violation
- Audit logging

### 2. Create Buffer Gateway Component
Create `shared-buffer/buffer_gateway.c` as a security boundary:
- Runs with elevated privilege to manage buffer
- Validates all frames
- Enforces ACLs
- Only forwards allowed message types to seL4
- Logs all access

### 3. seL4 Component Configuration
For seL4, use CAmkES to define:
- Minimal capabilities
- No device drivers
- Only endpoint to gateway
- Read-only shared memory regions where possible
- No VFS, no network stack

## Cross-Platform Guest OS Support

### Is God's Eye Dynamic?
**Partially dynamic** - depends on components:

**Platform Independent:**
- ML anomaly detector (Python) - works on Windows/Linux/Mac
- LLM explainer - works anywhere Python runs
- Analysis logic - portable

**Linux-Specific (need porting):**
- Collector core (C) uses POSIX APIs: pthreads, procfs, signals, Unix sockets
- Guest agent uses /proc filesystem
- Some system calls

**Would SSH detect Windows guest OS?**
- **SSH itself is network-based** - if Windows has OpenSSH server running, SSH will connect regardless
- **But our telemetry collection differs:**
  - Linux: reads /proc, uses procfs monitors
  - Windows: would need different approach (ETW, WMI, Event Logs, PowerShell)
  - Current guest agent only works on Linux

**To make it truly cross-platform:**
1. Abstract OS-specific collectors
2. Add Windows collector (using Windows APIs)
3. Add event normalization layer
4. Make protocol agnostic

## Current State Assessment
- **Designed for Linux primarily** (as stated in plans)
- **Collector uses Linux/POSIX APIs heavily**
- **Would need significant porting for Windows**
- **SSH detection is separate from our telemetry** - our system collects its own telemetry

## Recommendations

### For seL4 Security
1. Never give seL4 direct access to raw guest memory
2. All data must go through validated gateway
3. Use capability whitelisting
4. Implement audit trail
5. Run in isolated VM if possible
6. Use formally verified components where available

### For Deployment
1. Build seL4 for ARM target with QEMU for testing
2. Create minimal CAmkES configuration
3. Implement gateway with strict validation
4. Test IPC thoroughly
5. Enable all security features
