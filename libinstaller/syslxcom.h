#ifndef _H_SYSLXCOM_
#define _H_SYSLXCOM_

#if SYSLXCOM_FOR_MAC
#include <sys/types.h>
#endif

#include "syslinux.h"

extern const char *program;
void clear_attributes(int fd);
void set_attributes(int fd);

#if SYSLXCOM_FOR_MAC
/*Mount arguments for MSDOSFS*/
struct msdosfs_args {
    const char  *fspec;
    uid_t       uid;
    gid_t       gid;
    mode_t      mask;
    uint32_t    flags;
    uint32_t    magic;
    int32_t     secondsWest;
    char        label[11];
};
int msdosfs_loaded(void);
void msdosfs_params(struct msdosfs_args *args, const char *diskptr, int dev, char *mntpath);
#else
int sectmap(int fd, sector_t *sectors, int nsectors);
#endif /*SYSLXCOM_FOR_MAC*/

int syslinux_already_installed(int dev_fd);

#endif
