#ifndef DM_CORE_INSTALLER_H
#define DM_CORE_INSTALLER_H

/* Installs a verified Desktop Mode VPK through the Vita package promoter. */
int dm_core_install_vpk(const char *vpk_path, const char *expected_version, int *must_exit);

#endif
