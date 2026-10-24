# God's Eye Transport Layer

Transport abstraction for moving events between producer and consumer across boundaries.

## Current Implementation

- **File transport**: Read JSONL from file (`--ingest-file`) - used for replay/demo
- **Unix socket transport**: Stream over AF_UNIX (`--ingest-socket`) - local communication

## Future/Planned

- **seL4 IPC**: Integration with seL4's inter-process communication mechanisms
- **Virtio**: For VM-based deployments
- **Shared memory ring buffer**: High-performance transport
- **vsock**: For hypervisor-guest communication

## Design Principles

- Framing with explicit length/checksums
- Bounded buffers with overflow handling
- Clear error reporting
- Trust boundary enforcement (validate on receive)
- No assumptions about cross-domain memory sharing
