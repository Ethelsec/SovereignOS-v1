# God's Eye Analyzer

Rule-based analysis component for the God's Eye prototype. Implements explainable detection rules.

## Rules

- **RULE-AUTH-001**: Multiple authentication failures (burst detection)
  - Triggers on multiple auth.failure events in short time window
  - Confidence: medium (could be legitimate)
  - Provides evidence with supporting event IDs

## Design Principles

- **Explainable**: Every finding includes reason and supporting evidence
- **Conservative**: Labels as "indicators" not "confirmed attacks"
- **Transparent**: Documents limitations for each rule
- **Testable**: Deterministic rule logic
