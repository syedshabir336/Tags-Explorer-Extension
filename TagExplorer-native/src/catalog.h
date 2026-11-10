#pragma once
#include <windows.h>
int catalog_open(void);
void catalog_close(void);
int catalog_add(const wchar_t *path);
int catalog_add_tree(const wchar_t *folder);
int catalog_apply_tags(const wchar_t *path, const wchar_t *csv);
int catalog_set_tags(const wchar_t *path, const wchar_t *csv);
int catalog_rename(const wchar_t *old_path, const wchar_t *new_path);
int catalog_list_tags(wchar_t ***names, int *count);
int catalog_get_photo_tags(const wchar_t *path, wchar_t **tags);
int catalog_search(const wchar_t *query, const wchar_t *tag_filter, int no_tags, wchar_t ***paths, wchar_t ***tags, int *count);
void catalog_free_results(wchar_t **paths, wchar_t **tags, int count);
