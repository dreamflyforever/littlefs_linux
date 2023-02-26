#include "lfs.h"

#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdint.h>
#include <assert.h>

int fd;

int device_read(const struct lfs_config *cfg, lfs_block_t block,
            lfs_off_t off, void *buffer, lfs_size_t size)
{

    // check if read is valid
    assert(block < cfg->block_count);

    // go to block
    off_t err = lseek(fd, (off_t)block*cfg->block_size + (off_t)off, SEEK_SET);
    if (err < 0) {
    	printf("%s %d\n", __func__, __LINE__);
        return -errno;
    }
    // read block
    ssize_t res = read(fd, buffer, (size_t)size);
    if (res < 0) {
    	printf("%s %d\n", __func__, __LINE__);
        return -errno;
    }

    return 0;

}

int device_prog(const struct lfs_config *cfg, lfs_block_t block,
            lfs_off_t off, const void *buffer, lfs_size_t size)
{

    // check if write is valid
    assert(block < cfg->block_count);

    // go to block
    off_t err = lseek(fd, (off_t)block*cfg->block_size + (off_t)off, SEEK_SET);
    if (err < 0) {
    	printf("%s %d\n", __func__, __LINE__);
        return -errno;
    }

    // write block
    ssize_t res = write(fd, buffer, (size_t)size);
    if (res < 0) {
    	printf("%s %d\n", __func__, __LINE__);
        return -errno;
    }

    return 0;
}

// Erase a block. A block must be erased before being programmed.
// The state of an erased block is undefined. Negative error codes
// are propagated to the user.
// May return LFS_ERR_CORRUPT if the block should be considered bad.
int device_erase(const struct lfs_config *cfg, lfs_block_t block)
{
	return 0;
}

// Sync the state of the underlying block device. Negative error codes
// are propagated to the user.
int device_sync(const struct lfs_config *cfg)
{

    int err = fsync(fd);
    if (err) {
        return -errno;
    }

    return 0;
}

// variables used by the filesystem
lfs_t lfs;
lfs_file_t file;

// configuration of the filesystem is provided by this struct
struct lfs_config cfg = {
    // block device operations
    .read  = device_read,
    .prog  = device_prog,
    .erase = device_erase,
    .sync  = device_sync,

    // block device configuration
    .read_size = 16,
    .prog_size = 16,
    .block_size = 4096,
    .block_count = 128,
    .cache_size = 16,
    .lookahead_size = 16,
    .block_cycles = 500,
};

// entry point
int main(void) {
    fd = open("test_file", O_RDWR);
    if (fd < 0) {
	printf("open file fail\n");
    } else {
	printf("open success fd: %d\n", fd);
    }
#if 0
    char * tmp = malloc(1024 * 1024);
    memset(tmp, 0xff, 1024 * 1024);
    write(fd, tmp, 1024*1024);
#endif
    // mount the filesystem
    int err = lfs_mount(&lfs, &cfg);

    // reformat if we can't mount the filesystem
    // this should only happen on the first boot
    if (err) {
        lfs_format(&lfs, &cfg);
        lfs_mount(&lfs, &cfg);
    }

    // read current count
    uint32_t boot_count = 0;
    lfs_file_open(&lfs, &file, "boot_count", LFS_O_RDWR | LFS_O_CREAT);
    lfs_file_read(&lfs, &file, &boot_count, sizeof(boot_count));

    // update boot count
    boot_count += 1;
    lfs_file_rewind(&lfs, &file);
    lfs_file_write(&lfs, &file, &boot_count, sizeof(boot_count));

    // remember the storage is not updated until the file is closed successfully
    lfs_file_close(&lfs, &file);

    // release any resources we were using
    lfs_unmount(&lfs);

    // print the boot count
    printf("boot_count: %d\n", boot_count);
    close(fd);
}
