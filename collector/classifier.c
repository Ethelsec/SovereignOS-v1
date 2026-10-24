#include "classifier.h"

uint32_t tp_classify_event(const tp_event_t *event)
{
    if (!event) return TP_EVENT_CUSTOM;
    switch (event->type) {
    case TP_TYPE_PROCESS_START:
    case TP_TYPE_PROCESS_EXIT:
    case TP_TYPE_THREAD_START:
    case TP_TYPE_THREAD_EXIT:
    case TP_TYPE_EXEC:
    case TP_TYPE_FORK: return TP_EVENT_PROCESS | TP_EVENT_USERSPACE;
    case TP_TYPE_FILE_OPEN:
    case TP_TYPE_FILE_WRITE:
    case TP_TYPE_FILE_CLOSE: return TP_EVENT_FILESYSTEM | TP_EVENT_STORAGE;
    case TP_TYPE_SOCKET_CONNECT:
    case TP_TYPE_SOCKET_ACCEPT:
    case TP_TYPE_SOCKET_CLOSE:
    case TP_TYPE_TCP_STATE:
    case TP_TYPE_DNS:
    case TP_TYPE_HTTP: return TP_EVENT_NETWORK;
    case TP_TYPE_MEMORY_ALLOC:
    case TP_TYPE_PAGE_FAULT: return TP_EVENT_MEMORY | TP_EVENT_PROCESS;
    case TP_TYPE_CPU_SAMPLE:
    case TP_TYPE_CONTEXT_SWITCH: return TP_EVENT_CPU | TP_EVENT_SCHEDULER;
    case TP_TYPE_SECURITY: return TP_EVENT_SECURITY;
    case TP_TYPE_KERNEL_WARNING: return TP_EVENT_KERNEL | TP_EVENT_SECURITY;
    case TP_TYPE_RESOURCE_SPIKE: return TP_EVENT_SYSTEM | TP_EVENT_CPU | TP_EVENT_MEMORY;
    case TP_TYPE_HEALTH:
    case TP_TYPE_METRIC:
    case TP_TYPE_LOG: return TP_EVENT_SYSTEM;
    default: return TP_EVENT_CUSTOM;
    }
}
