# God's Eye Guest Agent

Guest-side telemetry collection component. Designed to run in the guest OS and collect selected events.

## Current Status

**Prototype phase**: Using replay fixtures for demonstration. Live collection via procfs is possible but requires verification of the actual deployment environment.

## Planned Sources

- Process start/exit events (via procfs monitoring)
- Network connections
- Authentication events
- Resource threshold monitoring

## Design

The guest agent would:
1. Monitor selected sources with minimal overhead
2. Normalize events to canonical schema
3. Buffer events with overflow protection
4. Transmit via agreed transport mechanism
5. Report health and counters
