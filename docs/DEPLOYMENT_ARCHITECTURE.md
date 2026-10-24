# Deployment Architecture - seL4, Guest OS, and God's Eye

## How seL4 is Deployed

### Current Environment
We have seL4 source code cloned in `~/seL4/`. To actually run seL4, you need to:

1. **Choose a target platform** (ARM is easier with QEMU, x86 also possible)
2. **Build seL4 kernel + userland**
3. **Create a system image**
4. **Run in QEMU or on hardware**

### Build Example for ARM (QEMU virt)
```bash
cd ~/seL4
mkdir build && cd build
../init-build.sh -DPLATFORM=qemu-arm-virt -DAARCH64=1
ninja
```

### Running in VM/QEMU
seL4 can run under QEMU. The tools have `simulate.py` scripts. Yes, you can run it in a VM/emulator.

## Secure Configuration

### Network Topology
```
┌─────────────┐
│   Guest OS   │  (Linux/Windows - to be ported)
│   (untrusted)│
└──────┬──────┘
       │ Telemetry Events
       ▼
┌─────────────────────────────────┐
│     Buffer Gateway (Trusted)     │  ← Security boundary
│  - Rate limiting                 │
│  - Frame validation              │
│  - ACL enforcement               │
│  - Audit logging                 │
└─────────────┬───────────────────┘
              │ Only validated frames
              │ via capability-gated IPC
              ▼
         ┌──────────┐
         │   seL4   │  ← Minimal, verified microkernel
         │ (trusted)│     Only gets what it needs
         └──────────┘
```

### Security Principles Applied
1. **Least Privilege**: seL4 only has capability to the shared buffer endpoint
2. **Defense in Depth**: Gateway validates everything
3. **No Direct Access**: Guest cannot write directly to seL4 memory
4. **Audit Trail**: All cross-boundary communication logged
5. **Rate Limiting**: Prevents DoS
6. **Fail Secure**: Invalid frames dropped, not forwarded

## Cross-Platform Support (Windows Guest)

### Current State
**The project is primarily Linux-focused.** Here's the breakdown:

| Component | Linux | Windows | Notes |
|-----------|-------|---------|-------|
| ML Detector (Python) | ✅ | ✅ | Fully cross-platform |
| LLM Explainer (Python) | ✅ | ✅ | Fully cross-platform |
| Collector Core (C) | ✅ | ⚠️ | Uses POSIX APIs (pthreads, signals, sockets) - needs porting |
| Guest Agent | ✅ | ❌ | Uses /proc filesystem - needs ETW/WMI |
| Shared Buffer | ✅ | ⚠️ | Could work with named pipes/shared memory on Windows |
| seL4 | N/A | N/A | Runs separately as microkernel |

### Would SSH detect a Windows guest OS?
**Yes and no:**
- **SSH protocol is network-based** - If Windows has OpenSSH Server installed and running, you can SSH into it regardless of OS
- **Our telemetry system is separate** - We have our own collectors, not relying on SSH for detection
- **OS fingerprinting via SSH** - Possible (banner, key exchange) but our system doesn't do that currently
- **For our purposes**: The guest agent would need to be rewritten for Windows to collect meaningful telemetry

### Making it Truly Cross-Platform
To support Windows guests:
1. Create `guest-agent/windows/` with ETW (Event Tracing for Windows) collectors
2. Port collector to use Windows APIs or use a POSIX layer (Cygwin/MSYS2)
3. Replace Unix sockets with Windows named pipes
4. Abstract filesystem/proc access
5. Update build system

**Currently this is a Linux prototype** as per the original plans.

## Recommendations for Production

1. **For seL4**: Deploy in isolated environment, use CAmkES for strict capability control
2. **Gateway**: Make it the only path, run with minimal privileges
3. **Monitoring**: Enable all audit logging
4. **Updates**: Keep seL4 updated from upstream
5. **Testing**: Use QEMU for development, hardware for production
