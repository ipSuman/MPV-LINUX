#pragma once

#include <QString>

struct mpv_node;
struct mpv_node_list;

namespace MpvNodeUtils {

const mpv_node* mapValue(const mpv_node_list* map, const char* key);
QString nodeString(const mpv_node* node);
int nodeInt(const mpv_node* node, int fallback = -1);
bool nodeFlag(const mpv_node* node);

} // namespace MpvNodeUtils
