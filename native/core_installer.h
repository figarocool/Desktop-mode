#ifndef DM_CORE_INSTALLER_H
#define DM_CORE_INSTALLER_H

/* Stages a verified Desktop Mode VPK, launches the bundled updater, and exits. */
int dm_core_install_vpk(const char *vpk_path, const char *expected_version, int *must_exit);
/* Removes the temporary updater bubble after it has handed control back. */
void dm_core_cleanup_updater(void);

#endif
