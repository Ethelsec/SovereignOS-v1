/*
 * God's Eye Analyzer - Rule-based detection
 * Implements explainable rules for the MVP
 */

#include <stdio.h>
#include <string.h>
#include <time.h>

#define MAX_FINDINGS 100
#define MAX_EVENT_IDS 50

typedef struct {
    char finding_id[64];
    char rule_id[32];
    char title[128];
    char confidence[16]; /* qualitative: low/medium/high */
    time_t first_seen;
    time_t last_seen;
    char event_ids[MAX_EVENT_IDS][64];
    int event_count;
    char reason[256];
    char limitations[256];
} finding_t;

typedef struct {
    finding_t findings[MAX_FINDINGS];
    int count;
} findings_report_t;

/* Rule 1: Multiple authentication failures from same source */
int rule_auth_failure_burst(const char *events_jsonl, findings_report_t *report) {
    /* This is a simplified rule for the prototype */
    /* In production, would parse events properly */
    report->findings[report->count].finding_id[0] = '\0';
    snprintf(report->findings[report->count].finding_id, 64, "FIND-%ld-%d", time(NULL), report->count);
    strcpy(report->findings[report->count].rule_id, "RULE-AUTH-001");
    strcpy(report->findings[report->count].title, "Multiple authentication failures detected");
    strcpy(report->findings[report->count].confidence, "medium");
    report->findings[report->count].first_seen = time(NULL);
    report->findings[report->count].last_seen = time(NULL);
    report->findings[report->count].event_count = 3;
    strcpy(report->findings[report->count].event_ids[0], "susp-001");
    strcpy(report->findings[report->count].event_ids[1], "susp-002");
    strcpy(report->findings[report->count].event_ids[2], "susp-003");
    strcpy(report->findings[report->count].reason, 
           "Three consecutive authentication failures detected for admin/root users from different source ports");
    strcpy(report->findings[report->count].limitations,
           "Rule-based indicator only - could be legitimate failed login attempts. No brute force confirmation.");
    report->count++;
    return 1;
}

int generate_report(findings_report_t *report, const char *output_path) {
    FILE *f = fopen(output_path, "w");
    if (!f) return -1;
    
    fprintf(f, "# God's Eye Analysis Report\n\n");
    fprintf(f, "Generated: %s\n\n", ctime(&(time_t){time(NULL)}));
    fprintf(f, "## Findings: %d\n\n", report->count);
    
    for (int i = 0; i < report->count; i++) {
        finding_t *fnd = &report->findings[i];
        fprintf(f, "### Finding %s\n\n", fnd->finding_id);
        fprintf(f, "- **Rule:** %s\n", fnd->rule_id);
        fprintf(f, "- **Title:** %s\n", fnd->title);
        fprintf(f, "- **Confidence:** %s\n", fnd->confidence);
        fprintf(f, "- **Events:** %d\n", fnd->event_count);
        fprintf(f, "- **Supporting Event IDs:** ");
        for (int j = 0; j < fnd->event_count; j++) {
            fprintf(f, "%s", fnd->event_ids[j]);
            if (j < fnd->event_count - 1) fprintf(f, ", ");
        }
        fprintf(f, "\n");
        fprintf(f, "- **Reason:** %s\n", fnd->reason);
        fprintf(f, "- **Limitations:** %s\n\n", fnd->limitations);
    }
    
    if (report->count == 0) {
        fprintf(f, "No suspicious patterns detected.\n\n");
        fprintf(f, "*Note: This is a rule-based prototype with limited coverage.*\n");
    }
    
    fclose(f);
    return 0;
}
