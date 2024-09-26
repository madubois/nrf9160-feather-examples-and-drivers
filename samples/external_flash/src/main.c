/*
 * Copyright (c) 2022 Lukasz Majewski, DENX Software Engineering GmbH
 * Copyright (c) 2020 Circuit Dojo, LLC
 * Copyright (c) 2019 Peter Bigot Consulting, LLC
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/* Sample which uses the filesystem API with littlefs */

#include <stdio.h>

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/fs/fs.h>
#include <zephyr/fs/littlefs.h>
#include <zephyr/storage/flash_map.h>
#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(main);

/* Used to determine if FS is in good state */
#define NOR_STORAGE_ERASED_ON_BOOT "/lfs/erased"

/* Matches LFS_NAME_MAX */
#define MAX_PATH_LEN 255

#define PARTITION_NODE DT_NODELABEL(lfs)
FS_FSTAB_DECLARE_ENTRY(PARTITION_NODE);

struct fs_mount_t *mp =
	&FS_FSTAB_ENTRY(PARTITION_NODE);

static int littlefs_flash_erase(unsigned int id)
{
	const struct flash_area *pfa;
	int rc;

	rc = flash_area_open(id, &pfa);
	if (rc < 0)
	{
		LOG_ERR("FAIL: unable to find flash area %u: %d\n",
				id, rc);
		return rc;
	}

	LOG_PRINTK("Area %u at 0x%x on %s for %u bytes\n",
			   id, (unsigned int)pfa->fa_off, pfa->fa_dev->name,
			   (unsigned int)pfa->fa_size);

	/* Optional wipe flash contents */
	if (IS_ENABLED(CONFIG_APP_WIPE_STORAGE))
	{
		rc = flash_area_erase(pfa, 0, pfa->fa_size);
		LOG_ERR("Erasing flash area ... %d", rc);
	}

	flash_area_close(pfa);
	return rc;
}

int nor_storage_init(void)
{
	int err;
	struct fs_dirent dirent;
	struct fs_statvfs sbuf;

	err = fs_statvfs(mp->mnt_point, &sbuf);
	if (err < 0)
		LOG_ERR("statvfs: %d", err);

	LOG_INF("bsize = %lu ; frsize = %lu ; blocks = %lu ; bfree = %lu",
			sbuf.f_bsize, sbuf.f_frsize,
			sbuf.f_blocks, sbuf.f_bfree);

	/* Make sure the flash has been erased */
	err = fs_stat(NOR_STORAGE_ERASED_ON_BOOT, &dirent);
	if (err)
	{
		LOG_INF("Erasing storage!");

		err = littlefs_flash_erase((uintptr_t)mp->storage_dev);
		if (err < 0)
			return err;

		/* Create folder */
		err = fs_mkdir(NOR_STORAGE_ERASED_ON_BOOT);
		if (err)
			return err;
	}

	return 0;
}

static int nor_storage_increment(char *fname)
{
	uint8_t boot_count = 0;
	struct fs_file_t file;
	int rc, ret;

	fs_file_t_init(&file);
	rc = fs_open(&file, fname, FS_O_CREATE | FS_O_RDWR);
	if (rc < 0)
	{
		LOG_ERR("FAIL: open %s: %d", fname, rc);
		return rc;
	}

	rc = fs_read(&file, &boot_count, sizeof(boot_count));
	if (rc < 0)
	{
		LOG_ERR("FAIL: read %s: [rd:%d]", fname, rc);
		goto out;
	}
	LOG_PRINTK("%s read count:%u (bytes: %d)\n", fname, boot_count, rc);

	rc = fs_seek(&file, 0, FS_SEEK_SET);
	if (rc < 0)
	{
		LOG_ERR("FAIL: seek %s: %d", fname, rc);
		goto out;
	}

	boot_count += 1;
	rc = fs_write(&file, &boot_count, sizeof(boot_count));
	if (rc < 0)
	{
		LOG_ERR("FAIL: write %s: %d", fname, rc);
		goto out;
	}

	LOG_PRINTK("%s write new boot count %u: [wr:%d]\n", fname,
			   boot_count, rc);

out:
	ret = fs_close(&file);
	if (ret < 0)
	{
		LOG_ERR("FAIL: close %s: %d", fname, ret);
		return ret;
	}

	return (rc < 0 ? rc : 0);
}

int main(void)
{
	int err;

	LOG_INF("External Flash on %s", CONFIG_BOARD);

	err = nor_storage_init();
	if (err < 0)
	{
		LOG_ERR("FAIL: nor_storage_init: %d", err);
		return err;
	}

	return nor_storage_increment("/lfs/boot_count");
}