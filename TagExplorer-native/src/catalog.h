#pragma once
#include <windows.h>
int catalog_open(void);
void catalog_close(void);
int catalog_add(const wchar_t *path);
int catalog_add_tree(const wchar_t *folder);
int catalog_apply_tags(const wchar_t *path, const wchar_t *csv);
int catalog_search(const wchar_t *query, wchar_t ***paths, wchar_t ***tags, int *count);
void catalog_free_results(wchar_t **paths, wchar_t **tags, int count);
