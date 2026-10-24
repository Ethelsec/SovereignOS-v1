#include "exporter.h"
#include "logger.h"
#include "protobuf_serializer.h"
#include "utilities.h"
#include <string.h>

int tp_exporter_open(tp_exporter_t *exporter, const tp_config_t *cfg)
{
    memset(exporter, 0, sizeof(*exporter));
    exporter->enabled = cfg->export_enabled;
    tp_safe_copy(exporter->endpoint, sizeof(exporter->endpoint), cfg->export_endpoint);
    if (!exporter->enabled) return TP_OK;
    exporter->file = fopen(exporter->endpoint, "ab");
    if (!exporter->file) {
        exporter->failures++;
        return TP_ERR_IO;
    }
    setvbuf(exporter->file, NULL, _IOFBF, 1024 * 1024);
    return TP_OK;
}

int tp_exporter_export(tp_exporter_t *exporter, const tp_event_t *event)
{
    if (!exporter || !event) return TP_ERR_INVAL;
    if (!exporter->enabled) return TP_OK;
    char buf[4096];
    int n = tp_otlp_json_serialize(event, buf, sizeof(buf));
    if (n < 0) {
        exporter->failures++;
        return TP_ERR;
    }
    if (fwrite(buf, 1, (size_t)n, exporter->file) != (size_t)n) {
        exporter->failures++;
        return TP_ERR_IO;
    }
    exporter->exported++;
    return TP_OK;
}

int tp_exporter_flush(tp_exporter_t *exporter)
{
    if (!exporter || !exporter->file) return TP_OK;
    return fflush(exporter->file) == 0 ? TP_OK : TP_ERR_IO;
}

void tp_exporter_close(tp_exporter_t *exporter)
{
    if (!exporter) return;
    if (exporter->file) fclose(exporter->file);
    memset(exporter, 0, sizeof(*exporter));
}

int tp_exporter_is_enabled(const tp_exporter_t *exporter)
{
    return exporter && exporter->enabled;
}
