#include "otlp_client.h"

int tp_otlp_client_send(tp_otlp_client_t *client, const tp_event_t *event)
{
    return tp_exporter_export(client, event);
}
