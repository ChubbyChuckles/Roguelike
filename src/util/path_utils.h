#ifndef ROGUE_PATH_UTILS_H
#define ROGUE_PATH_UTILS_H

int rogue_find_asset_path(const char* filename, char* out, int out_sz);
/* Documentation helper: search for docs/ prefixed files using a small prefix list similar to
 * assets. */
int rogue_find_doc_path(const char* filename, char* out, int out_sz);

#endif
