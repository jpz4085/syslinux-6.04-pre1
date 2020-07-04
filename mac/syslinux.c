/* ----------------------------------------------------------------------- *
 *
 *   Copyright 2020 JPZ4085 - Based on Linux alternate installer
 *   Copyright 1998-2008 H. Peter Anvin - All Rights Reserved
 *   Copyright 2009-2010 Intel Corporation; author: H. Peter Anvin
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, Inc., 53 Temple Place Ste 330,
 *   Boston MA 02111-1307, USA; either version 2 of the License, or
 *   (at your option) any later version; incorporated herein by reference.
 *
 * ----------------------------------------------------------------------- */

/*
 * syslinux.c - macOS/OS X installer program for SYSLINUX
 *
 * This is an alternate version of the installer which doesn't require
 * mtools, but requires root privilege.
 */

#include <errno.h>
#include <fcntl.h>
#include <paths.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <inttypes.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/mount.h>
#include <sys/param.h>
#include <sys/statvfs.h>

#include <sys/ioctl.h>

#define __u32 uint32_t

#include <paths.h>
#ifndef _PATH_TMP
# define _PATH_TMP "/tmp/"
#endif

#include "syslinux.h"

#include <getopt.h>
#include <sysexits.h>
#include "syslxrw.h"
#include "syslxcom.h"
#include "syslxfs.h"
#include "setadv.h"
#include "syslxopt.h" /* unified options */
#include "libfat.h"

extern const char *program;	/* Name of program */

pid_t mypid;
char *mntpath = NULL;		/* Path on which to mount */

void __attribute__ ((noreturn)) die(const char *msg)
{
    fprintf(stderr, "%s: %s\n", program, msg);
    if (mntpath)
	rmdir(mntpath);
    exit(1);
}

/*
 * Modify the ADV of an existing installation
 */
int modify_existing_adv(const char *path)
{
    if (opt.reset_adv)
	syslinux_reset_adv(syslinux_adv);
    else if (read_adv(path, "ldlinux.sys") < 0)
	return 1;

    if (modify_adv() < 0)
	return 1;

    if (write_adv(path, "ldlinux.sys") < 0)
	return 1;

    return 0;
}

int do_open_file(char *name)
{
    int fd;

    if ((fd = open(name, O_RDONLY)) >= 0) {
    fchflags(fd, 0x00000000);
	close(fd);
    }

    unlink(name);
    fd = open(name, O_WRONLY | O_CREAT | O_TRUNC, 0444);
    if (fd < 0)
	perror(name);

    return fd;
}

/*
 * Wrapper for ReadFile suitable for libfat
 */
int libfat_readfile(intptr_t pp, void *buf, size_t secsize,
                    libfat_sector_t sector)
{
    uint32_t offset = (uint32_t) sector * secsize;
    uint32_t filepos = lseek(pp, offset, SEEK_SET);
    uint32_t bytes_read = read(pp, buf, secsize);
    
    if (filepos != offset || bytes_read != secsize ) {
        printf("Cannot read sector %llu\n", sector);
        exit(1);
    }
    
    return bytes_read;
}

int main(int argc, char *argv[])
{
    static unsigned char sectbuf[SECTOR_SIZE];
    int err = 0;
    int dev_fd, fd;
    struct stat dst, st;
    struct msdosfs_args args;
    char *ldlinuxsys_name;
    char *ldlinuxc32_name;
    char *ldlinuxdir_name;
    char *ldlinux_path;
    char mntname[128];
    char *subdir;
    libfat_sector_t *sectors = NULL;
    int ldlinux_sectors = (boot_image_len + SECTOR_SIZE - 1) >> SECTOR_SHIFT;
    const char *errmsg;
    int patch_sectors;
    int i, rv;

    mypid = getpid();
    parse_options(argc, argv, MODE_SYSLINUX);

    /* Note: subdir is guaranteed to start and end in / */
    if (opt.directory && opt.directory[0]) {
	int len = strlen(opt.directory);
	int rv = asprintf(&subdir, "%s%s%s",
			  opt.directory[0] == '/' ? "" : "/",
			  opt.directory,
			  opt.directory[len-1] == '/' ? "" : "/");
	if (rv < 0 || !subdir) {
	    perror(program);
	    exit(1);
	}
    } else {
	subdir = "/";
    }

    if (!opt.device || opt.install_mbr || opt.activate_partition)
	usage(EX_USAGE, MODE_SYSLINUX);
    
    /* Root permission required for device access */
    if (geteuid())
        die("This program needs root privilege");

    /*
     * First make sure we can open the device at all, and that we have
     * read/write permission.
     */
    dev_fd = open(opt.device, O_RDWR);
    if (dev_fd < 0 || fstat(dev_fd, &st) < 0) {
	perror(opt.device);
	exit(1);
    }

    if (!S_ISBLK(st.st_mode) && !S_ISREG(st.st_mode) && !S_ISCHR(st.st_mode)) {
	die("not a device or regular file");
    }

    if (opt.offset && S_ISBLK(st.st_mode)) {
	die("can't combine an offset with a block device");
    }

    xpread(dev_fd, sectbuf, SECTOR_SIZE, opt.offset);
    fsync(dev_fd);

    /*
     * Check to see that what we got was indeed an FAT/NTFS
     * boot sector/superblock
     */
    if ((errmsg = syslinux_check_bootsect(sectbuf, &fs_type))) {
	fprintf(stderr, "%s: %s\n", opt.device, errmsg);
	exit(1);
    }

    /*
     * Now mount the device.
     */
	if (chdir(_PATH_TMP)) {
	    fprintf(stderr, "%s: Cannot access the %s directory.\n",
		    program, _PATH_TMP);
	    exit(1);
	}
#define TMP_MODE (S_IXUSR|S_IWUSR|S_IXGRP|S_IWGRP|S_IWOTH|S_IXOTH|S_ISVTX)

	if (stat(".", &dst) || !S_ISDIR(dst.st_mode) ||
	    (dst.st_mode & TMP_MODE) != TMP_MODE) {
	    die("possibly unsafe " _PATH_TMP " permissions");
	}

	for (i = 0;; i++) {
	    snprintf(mntname, sizeof mntname, "syslinux.mnt.%lu.%d",
		     (unsigned long)mypid, i);

	    if (lstat(mntname, &dst) != -1 || errno != ENOENT)
		continue;

	    rv = mkdir(mntname, 0000);

	    if (rv == -1) {
		if (errno == EEXIST || errno == EINTR)
		    continue;
		perror(program);
		exit(1);
	    }

	    if (lstat(mntname, &dst) || dst.st_mode != (S_IFDIR | 0000) ||
		dst.st_uid != 0) {
		die("someone is trying to symlink race us!");
	    }
	    break;		/* OK, got something... */
	}

	mntpath = mntname;
    
    if (!msdosfs_loaded()) {
        msdosfs_params(&args, opt.device, dev_fd, mntpath);
    } else {
        die("msdos filesystem is not available");
    }
    
    char *mntdirpath = alloca(strlen(_PATH_TMP) + strlen(mntpath) + 1);
    sprintf(mntdirpath, "%s%s", _PATH_TMP, mntpath);
    
#define MNTFLAGS 0x0010000C /* MNT_NOEXEC|MNT_NOSUID|MNT_DONTBROWSE */
    if (mount("msdos", mntdirpath, MNTFLAGS, &args) < 0) {
        die("failed on mounting msdos volume");
    }
    
    ldlinux_path = alloca(strlen(mntpath) + strlen(subdir) + 1);
    sprintf(ldlinux_path, "%s%s", mntpath, subdir);

    ldlinuxsys_name = alloca(strlen(ldlinux_path) + 14);
    if (!ldlinuxsys_name) {
	perror(program);
	err = 1;
	goto umount;
    }
    sprintf(ldlinuxsys_name, "%s/ldlinux.sys", mntpath);

    /* update ADV only ? */
    if (opt.update_only == -1) {
	if (opt.reset_adv || opt.set_once) {
	    modify_existing_adv(ldlinux_path);
	    unmount(mntdirpath, 0);
	    sync();
        rmdir(mntpath);
	    exit(0);
    } else if (opt.update_only && !syslinux_already_installed(dev_fd)) {
        fprintf(stderr, "%s: no previous syslinux boot sector found\n", argv[0]);
        unmount(mntdirpath, 0);
        rmdir(mntpath);
        exit(1);
	} else {
	    fprintf(stderr, "%s: please specify --install or --update for the future\n", argv[0]);
	    opt.update_only = 0;
	}
    }

    /* Read a pre-existing ADV, if already installed */
    if (opt.reset_adv)
	syslinux_reset_adv(syslinux_adv);
    else if (read_adv(ldlinux_path, "ldlinux.sys") < 0)
	syslinux_reset_adv(syslinux_adv);
    if (modify_adv() < 0)
	exit(1);

    fd = do_open_file(ldlinuxsys_name);
    if (fd < 0) {
	err = 1;
	goto umount;
    }

    /* Write it the first time */
    if (xpwrite(fd, (const char _force *)boot_image, boot_image_len, 0)
	!= (int)boot_image_len ||
	xpwrite(fd, syslinux_adv, 2 * ADV_SIZE,
		boot_image_len) != 2 * ADV_SIZE) {
	fprintf(stderr, "%s: write failure on %s\n", program, ldlinuxsys_name);
	exit(1);
    }

    fsync(fd);
    close(fd);
    sync();
    
    ldlinuxc32_name = alloca(strlen(ldlinux_path) + 14);
    if (!ldlinuxc32_name) {
        perror(program);
        err = 1;
        goto umount;
    }
    sprintf(ldlinuxc32_name, "%sldlinux.c32", ldlinux_path);
    fd = do_open_file(ldlinuxc32_name);
    if (fd < 0) {
        remove(ldlinuxsys_name);
        err = 1;
        goto umount;
    }
    
    rv = xpwrite(fd, (const char _force *)syslinux_ldlinuxc32,
                 syslinux_ldlinuxc32_len, 0);
    if (rv != (int)syslinux_ldlinuxc32_len) {
        fprintf(stderr, "%s: write failure on %s\n", program, ldlinuxc32_name);
        exit(1);
    }
    
    fsync(fd);
    /*
     * Set the attributes
     */
    chflags(ldlinuxc32_name, 0x00008002); /* Hidden+Immutable */
    
    close(fd);
    sync();
    
umount:
    unmount(mntdirpath, 0);
    sync();
    
    if (err) {
        rmdir(mntpath);
        exit(err);
    }
    
    /* Map the file (is there a better way to do this?)*/
    struct libfat_filesystem *fs;
    libfat_sector_t s;
    libfat_sector_t *secp;
    uint32_t ldlinux_cluster;
    
    ldlinux_sectors += 2; /* 2 ADV sectors */
    sectors = calloc(ldlinux_sectors, sizeof *sectors);
    if (dev_fd < 0 || fstat(dev_fd, &st) < 0) {
        perror(opt.device);
        rmdir(mntpath);
        exit(1);
    }
    fs = libfat_open(libfat_readfile, (intptr_t) dev_fd);
    if (fs == NULL) printf("null fs struct\n");
    ldlinux_cluster = libfat_searchdir(fs, 0, "LDLINUX SYS", NULL);
    
    secp = sectors;
    int nsectors;
    nsectors  = 0;
    s = libfat_clustertosector(fs, ldlinux_cluster);
    while (s && nsectors < ldlinux_sectors) {
        *secp++ = s;
        nsectors++;
        s = libfat_nextsector(fs, s);
    }
    libfat_close(fs);

    /*
     * Patch ldlinux.sys and the boot sector
     */
    i = syslinux_patch(sectors, ldlinux_sectors, opt.stupid_mode, opt.raid_mode, subdir, NULL);
    patch_sectors = (i + SECTOR_SIZE - 1) >> SECTOR_SHIFT;
    
    /* Mount the device again for reliable writing */
    mount("msdos", mntdirpath, MNTFLAGS, &args);
    
    /*
     * Write the now-patched first sectors of ldlinux.sys
     */
    for (i = 0; i < patch_sectors; i++) {
    xpwrite(dev_fd,
        (const char _force *)boot_image + i * SECTOR_SIZE,
        SECTOR_SIZE,
        (opt.offset + (off_t) sectors[i]) << SECTOR_SHIFT);
    }
    
    /* Move the file to the desired location and set attributes */
    if (opt.directory) {
        ldlinuxdir_name = alloca(strlen(ldlinux_path) + 14);
        if (!ldlinuxdir_name) {
            perror(program);
            rmdir(mntpath);
            exit(1);
        }
        sprintf(ldlinuxdir_name, "%sldlinux.sys", ldlinux_path);
        if (!rename(ldlinuxsys_name, ldlinuxdir_name)) {
            chflags(ldlinuxdir_name, 0x00008002); /* Hidden+Immutable */
            unmount(mntdirpath, 0);
            rmdir(mntpath);
        } else {
            fprintf(stderr, "%s: Failed to move ldlinux.sys to destination directory: %s\n", program, opt.directory);
            unmount(mntdirpath, 0);
            rmdir(mntpath);
            exit(1);
        }
    } else {
        /* Set attributes without moving the file */
        chflags(ldlinuxsys_name, 0x00008002); /* Hidden+Immutable */
        unmount(mntdirpath, 0);
        rmdir(mntpath);
    }

    /*
     * To finish up, write the boot sector
     */

    /* Read the superblock again since it might have changed while mounted */
    xpread(dev_fd, sectbuf, SECTOR_SIZE, opt.offset);

    /* Copy the syslinux code into the boot sector */
    syslinux_make_bootsect(sectbuf, fs_type);

    /* Write new boot sector */
    xpwrite(dev_fd, sectbuf, SECTOR_SIZE, opt.offset);

    close(dev_fd);
    sync();

    /* Done! */

    return 0;
}
