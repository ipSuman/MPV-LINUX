#include "MpvNodeUtils.h"

#include <QtGlobal>

#include <mpv/client.h>

namespace MpvNodeUtils {

const mpv_node* mapValue(const mpv_node_list* map, const char* key) {
    if (!map || !map->keys || !map->values) return nullptr;
    for (int i = 0; i < map->num; ++i) {
        if (map->keys[i] && qstrcmp(map->keys[i], key) == 0) return &map->values[i];
    }
    return nullptr;
}

QString nodeString(const mpv_node* node) {
    if (!node) return {};
    if (node->format == MPV_FORMAT_STRING && node->u.string) {
        return QString::fromUtf8(node->u.string);
    }
    return {};
}

int nodeInt(const mpv_node* node, int fallback) {
    if (!node) return fallback;
    if (node->format == MPV_FORMAT_INT64) return static_cast<int>(node->u.int64);
    if (node->format == MPV_FORMAT_DOUBLE) return static_cast<int>(node->u.double_);
    return fallback;
}

bool nodeFlag(const mpv_node* node) {
    return node && node->format == MPV_FORMAT_FLAG && node->u.flag != 0;
}

} // namespace MpvNodeUtils
