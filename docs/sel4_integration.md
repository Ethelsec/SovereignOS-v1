# seL4 Integration Plan

## Overview

This document describes how God's Eye integrates with seL4. The seL4 environment has been set up with kernel, tools, and supporting components.

## seL4 Environment

Located in `~/seL4/`:
- **kernel/** - seL4 microkernel source
- **tools/** - Build system and utilities
- **capdl/** - Capability distribution
- **util_libs/** - Utility libraries
- **sel4runtime/** - Runtime support

## Integration Strategy

### For the Prototype

Given time constraints and the need to demonstrate the concept:
1. **Document the boundary** - Clearly show where seL4 sits relative to guest and collector
2. **Create seL4-aware components** - Design collector to be portable
3. **IPC mechanisms** - Plan for seL4's IPC (endpoints, notifications) rather than assuming POSIX
4. **Capabilities** - Respect seL4's capability-based security model

### Customizations

We need to "customize it a little to work with our project":
1. Create a simple seL4 application template for God's Eye components
2. Add build configuration examples
3. Document how events would flow across seL4 boundaries
4. Create adapter code that can work in both Linux and seL4 environments

## Trust Boundaries

As per God's Eye principles:
- Guest is untrusted
- All inputs must be validated
- No assumption of shared memory across domains
- Use explicit IPC with framing
- Respect capability boundaries

## Next Steps

For a full integration, would need:
- CAmkES or native seL4 app definition
- Proper capability allocation
- Device drivers if needed
- Communication channels between components
- Port of necessary libraries
