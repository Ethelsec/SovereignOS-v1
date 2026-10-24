/*
 * God's Eye Guest Agent - Process Monitor
 * Simplified probe for Linux procfs
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define MAX_EVENT_SIZE 1024

/* Generate a canonical event in JSONL format */
void generate_process_event(const char *event_type, pid_t pid, pid_t ppid, 
                           const char *exe, const char *output_file) {
    FILE *f = fopen(output_file, "a");
    if (!f) return;
    
    time_t now = time(NULL);
    struct tm *tm_info = gmtime(&now);
    char timestamp[32];
    strftime(timestamp, 32, "%Y-%m-%dT%H:%M:%SZ", tm_info);
    
    fprintf(f, "{\"schema_version\":\"1\",\"event_id\":\"ga-%ld-%d\",\"timestamp_utc\":\"%s\","
            "\"timestamp_monotonic_ns\":%ld,\"source\":\"proc_adapter\",\"event_type\":\"%s\","
            "\"severity\":\"info\",\"host_id\":\"guest-agent\",\"process\":{\"pid\":%d,\"ppid\":%d,"
            "\"executable\":\"%s\"},\"network\":null,\"attributes\":{\"probe\":\"procfs\"},"
            "\"sequence\":%ld}\n", 
            now, pid, timestamp, (long)now * 1000000000, event_type, pid, ppid, exe, now);
    
    fclose(f);
}

int main(int argc, char **argv) {
    if (argc < 2) {
        printf("Usage: %s <output.jsonl>\n", argv[0]);
        return 1;
    }
    
    printf("God's Eye Guest Agent - Starting (prototype)\n");
    printf("Output: %s\n", argv[1]);
    printf("Note: This is a simplified probe for demonstration\n");
    
    /* For demo purposes, generate a sample event */
    generate_process_event("process.start", getpid(), getppid(), argv[0], argv[1]);
    
    return 0;
}
