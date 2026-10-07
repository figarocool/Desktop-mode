#ifndef FILE_JOBS_H
#define FILE_JOBS_H
#include "desktop_api.h"
typedef void (*DmJobResult)(int success,void*context);
int dm_job_copy(const char*source,const char*destination,DmJobResult,void*);
int dm_job_trash(const char*source,DmJobResult,void*);
int dm_job_empty_trash(DmJobResult,void*);
int dm_job_restore(const char*trashed_file,DmJobResult,void*);
typedef struct {int active,calculating;uint64_t bytes,total,items,total_items;} DmJobProgress;
void dm_job_progress(DmJobProgress*);
int dm_job_empty_trash_unit(const char*unit,DmJobResult,void*);
int dm_job_active(void);
void dm_job_tick(void);
void dm_job_draw(void);
void dm_job_cancel(void);
int dm_trash_has_items(const char*unit); /* NULL: all writable units */
int dm_trash_directories(const char*source,char*files,char*info);
#endif
