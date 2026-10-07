#ifndef DM_FILE_TYPES_H
#define DM_FILE_TYPES_H
#include "desktop_api.h"
int dm_extension_valid(const char*);
int dm_extension_reserved(const char*);
int dm_association_count(void);
const char*dm_association_extension(int);
const char*dm_association_default(const char*);
const char*dm_association_app(const char*);
int dm_association_choose(const char*,const char*);
int dm_association_reset(const char*);
void dm_associations_load(void);
extern const DmApp dm_file_types_app;
#ifdef DESKTOP_PREVIEW
void dm_associations_test_path(const char*);
#endif
#endif
