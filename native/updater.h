#ifndef DM_UPDATER_H
#define DM_UPDATER_H

typedef struct {
    int apps_updated;
    int core_ready;
    int failed;
    char tag[48];
} DmUpdateResult;

/* Checks the latest GitHub release before app modules are loaded. */
void dm_updates_check(DmUpdateResult *result);

#endif
