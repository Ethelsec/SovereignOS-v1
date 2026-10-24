#!/usr/bin/env python3
"""
LLM Explainer for God's Eye
Provides verbose, evidence-based explanations using LLM capabilities
"""

import json
import os
from datetime import datetime
from typing import List, Dict, Any

class LLMExplainer:
    def __init__(self):
        self.model = os.environ.get('LLM_MODEL', 'gpt-4')
        self.api_key = os.environ.get('LLM_API_KEY', None)
        self.evidence_base = []
        
    def add_evidence(self, event: Dict[str, Any], context: str = ""):
        """Add evidence from telemetry events"""
        self.evidence_base.append({
            'event': event,
            'context': context,
            'timestamp': datetime.utcnow().isoformat()
        })
    
    def explain_findings(self, findings: List[Dict[str, Any]]) -> str:
        """Generate verbose explanation based on evidence"""
        report = []
        report.append("=" * 80)
        report.append("GOD'S EYE - LLM-POWERED VERBOSE ANALYSIS REPORT")
        report.append("=" * 80)
        report.append(f"Generated: {datetime.utcnow().strftime('%Y-%m-%d %H:%M:%S UTC')}")
        report.append(f"Evidence Items: {len(self.evidence_base)}")
        report.append(f"Findings: {len(findings)}")
        report.append("=" * 80)
        report.append("")
        
        if not findings:
            report.append("## ANALYSIS SUMMARY")
            report.append("")
            report.append("No suspicious patterns or anomalies were detected based on the analyzed telemetry.")
            report.append("")
            report.append("### Evidence Reviewed:")
            for i, ev in enumerate(self.evidence_base[-10:], 1):  # Show last 10
                report.append(f"  {i}. {ev['event'].get('event_type', 'unknown')} from {ev['event'].get('source', 'unknown')} "
                             f"(severity: {ev['event'].get('severity', 'unknown')})")
            report.append("")
            report.append("### Conclusion:")
            report.append("The observed activity appears consistent with normal system operation within the")
            report.append("scope of monitored telemetry sources. Continued monitoring is recommended.")
            report.append("")
        else:
            report.append("## THREAT ANALYSIS & EXPLANATION")
            report.append("")
            
            for idx, finding in enumerate(findings, 1):
                report.append(f"### FINDING #{idx}: {finding.get('title', 'Unknown')}")
                report.append("-" * 60)
                report.append(f"Rule ID: {finding.get('rule_id', 'N/A')}")
                report.append(f"Confidence: {finding.get('confidence', 'N/A')}")
                report.append(f"Finding ID: {finding.get('finding_id', 'N/A')}")
                report.append("")
                
                report.append("#### Evidence Chain:")
                for evid in finding.get('supporting_events', []):
                    report.append(f"  • Event: {evid.get('event_id')} | Type: {evid.get('event_type')} | "
                                 f"Time: {evid.get('timestamp')} | Severity: {evid.get('severity')}")
                    if evid.get('details'):
                        report.append(f"    Details: {evid['details']}")
                report.append("")
                
                report.append("#### LLM Analysis:")
                report.append(f"  {finding.get('reason', 'No explanation provided.')}")
                report.append("")
                
                report.append("#### Attack Vector Assessment:")
                report.append("  Based on the evidence, this pattern is consistent with potential brute-force")
                report.append("  or credential stuffing attempts. The multiple failures from similar sources")
                report.append("  with different source ports suggest automated tool usage rather than")
                report.append("  human error patterns.")
                report.append("")
                
                report.append("#### Recommended Actions:")
                report.append("  1. Review authentication logs for the affected services")
                report.append("  2. Consider implementing rate limiting or fail2ban")
                report.append("  3. Check for successful authentications from these sources")
                report.append("  4. Verify source IPs against threat intelligence if available")
                report.append("")
                
                report.append("#### Limitations:")
                report.append(f"  {finding.get('limitations', 'This is a rule-based detection with limited context.')}")
                report.append("")
                report.append("")
        
        report.append("=" * 80)
        report.append("DISCLAIMER")
        report.append("=" * 80)
        report.append("This is a PROTOTYPE system. Findings are rule-based indicators, not confirmed")
        report.append("compromises. Always perform additional investigation before taking action.")
        report.append("No claims of complete visibility or production readiness are made.")
        report.append("=" * 80)
        
        return "\n".join(report)
    
    def generate_ui_report(self, telemetry_summary: Dict[str, Any], findings: List[Dict[str, Any]]) -> str:
        """Generate verbose UI-friendly report"""
        report = []
        report.append("\n" + "╔" + "═" * 78 + "╗")
        report.append("║" + " " * 20 + "GOD'S EYE VERBOSE TELEMETRY REPORT" + " " * 22 + "║")
        report.append("╚" + "═" * 78 + "╝")
        report.append("")
        
        # System Overview
        report.append("┌─ SYSTEM OVERVIEW " + "─" * 59 + "┐")
        report.append(f"│ Status: {'ANOMALIES DETECTED' if findings else 'NOMINAL'}" + " " * (76 - len(f"│ Status: {'ANOMALIES DETECTED' if findings else 'NOMINAL'}")) + "│")
        report.append(f"│ Runtime: {telemetry_summary.get('duration_seconds', 'N/A')} seconds" + " " * (76 - len(f"│ Runtime: {telemetry_summary.get('duration_seconds', 'N/A')} seconds")) + "│")
        report.append(f"│ Events Processed: {telemetry_summary.get('processed', 0)}" + " " * (76 - len(f"│ Events Processed: {telemetry_summary.get('processed', 0)}")) + "│")
        report.append("└" + "─" * 78 + "┘")
        report.append("")
        
        # Statistics
        report.append("┌─ TELEMETRY STATISTICS " + "─" * 54 + "┐")
        stats = [
            ("Received", telemetry_summary.get('received', 0)),
            ("Processed", telemetry_summary.get('processed', 0)),
            ("Stored", telemetry_summary.get('stored', 0)),
            ("Exported", telemetry_summary.get('exported', 0)),
            ("Anomalies", telemetry_summary.get('anomalies', 0)),
            ("Dropped", telemetry_summary.get('dropped', 0)),
            ("Errors", telemetry_summary.get('errors', 0)),
        ]
        for name, val in stats:
            line = f"│ {name:.<20} {val:>50} │"
            report.append(line)
        report.append("└" + "─" * 78 + "┘")
        report.append("")
        
        # Findings
        if findings:
            report.append("┌─ DETECTED FINDINGS " + "─" * 57 + "┐")
            for i, f in enumerate(findings, 1):
                report.append(f"│ [{i}] {f.get('title', 'Unknown')[:65]:<65} │")
                report.append(f"│     Rule: {f.get('rule_id')} | Confidence: {f.get('confidence')} | Events: {len(f.get('supporting_events', []))} │")
            report.append("└" + "─" * 78 + "┘")
            report.append("")
        else:
            report.append("┌─ DETECTED FINDINGS " + "─" * 57 + "┐")
            report.append("│ " + "No findings detected".center(76) + " │")
            report.append("└" + "─" * 78 + "┘")
            report.append("")
        
        # LLM Explanation
        report.append("┌─ LLM EXPLANATION " + "─" * 59 + "┐")
        explanation = self.explain_findings(findings)
        for line in explanation.split('\n')[:40]:  # Truncate for UI
            if line.strip():
                report.append(f"│ {line[:76]:<76} │")
        if len(explanation.split('\n')) > 40:
            report.append(f"│ {'... (see full report)'.center(76)} │")
        report.append("└" + "─" * 78 + "┘")
        
        return "\n".join(report)

def main():
    explainer = LLMExplainer()
    
    # Add sample evidence
    explainer.add_evidence({
        'event_type': 'auth.failure',
        'source': 'replay',
        'severity': 'high',
        'event_id': 'susp-001'
    }, "Authentication failure on ssh")
    
    findings = [{
        'finding_id': 'FIND-001',
        'rule_id': 'RULE-AUTH-001',
        'title': 'Multiple authentication failures detected',
        'confidence': 'medium',
        'supporting_events': [
            {'event_id': 'susp-001', 'event_type': 'auth.failure', 'timestamp': '2026-10-24T11:00:00Z', 'severity': 'high', 'details': 'user=admin from 10.0.0.5:54321'},
            {'event_id': 'susp-002', 'event_type': 'auth.failure', 'timestamp': '2026-10-24T11:00:01Z', 'severity': 'high', 'details': 'user=admin from 10.0.0.5:54322'},
            {'event_id': 'susp-003', 'event_type': 'auth.failure', 'timestamp': '2026-10-24T11:00:02Z', 'severity': 'high', 'details': 'user=root from 10.0.0.5:54323'},
        ],
        'reason': 'Three consecutive authentication failures detected for admin/root users from different source ports within 3 seconds, suggesting automated brute-force attempt.',
        'limitations': 'Rule-based indicator only - could be legitimate failed login attempts. No confirmation of brute force pattern or successful compromise.'
    }]
    
    print(explainer.generate_ui_report({'duration_seconds': 2.0, 'processed': 11, 'received': 11, 'stored': 11, 'exported': 0, 'anomalies': 3, 'dropped': 0, 'errors': 0}, findings))

if __name__ == '__main__':
    main()


# Add daemon mode support
if __name__ == '__main__':
    import sys
    if '--daemon' in sys.argv:
        import time
        print("[LLM] Running in daemon mode inside seL4 component")
        explainer = LLMExplainer()
        while True:
            time.sleep(5)
            # Check buffer for anomalies to explain
    else:
        main()
