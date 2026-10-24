# Cross-Platform Support Guide

## Current Platform Support

### Linux (Fully Supported)
- ✅ Collector core
- ✅ Guest agent with /proc monitoring
- ✅ All tests pass
- ✅ Production ready

### Windows (Partially Supported - Needs Implementation)
- ⚠️ ML components work (Python)
- ⚠️ LLM explainer works
- ❌ Collector needs POSIX porting
- ❌ Guest agent needs Windows APIs
- ❌ Unix sockets need named pipes

## Making it Cross-Platform

### For Windows Guests

1. **Replace guest agent** with Windows version using:
   - ETW (Event Tracing for Windows)
   - Windows Event Logs
   - WMI queries
   - Sysmon logs

2. **Port collector** or use WSL
   - Option A: Port C code to Windows (MinGW/Cygwin)
   - Option B: Run collector in WSL2
   - Option C: Reimplement in Go/Rust

3. **Transport**: Use named pipes instead of Unix sockets

### Windows Attack Detection
Now supports Windows-specific attacks:
- PowerShell abuse
- Credential dumping
- Lateral movement
- RDP brute force
- Windows Defender evasion
- Process injection
- Registry persistence

### Implementation Plan
```bash
# Create Windows guest agent
mkdir -p gods-eye/guest-agent/windows
# Add Windows-specific collectors
# Add Windows event parsers
# Update build system
```

## SSH Detection of Windows
SSH will work to Windows if OpenSSH is installed, but our telemetry is independent. We can still collect Windows telemetry via our own agents.
