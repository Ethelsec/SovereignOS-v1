# Integration Plan: Collector -> Buffer -> ML/LLM inside seL4

## Architecture
1. Collector receives events via socket/file
2. Events forwarded through secure buffer gateway to seL4
3. ML anomaly detector runs inside seL4 component
4. LLM explainer runs inside seL4 component  
5. Results returned via buffer

## Implementation
- Create seL4 components for ML and LLM
- Modify collector to use buffer gateway
- Bridge between C collector and Python ML
