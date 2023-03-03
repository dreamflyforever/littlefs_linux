#include "lfs.h"
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/types.h>
#include <unistd.h>

#define DEBUG 1
#if DEBUG
#define debug_printf(format, ...) \
	{printf(format, ##__VA_ARGS__);}
#else
#define rtb_printf(format, ...) 
#endif

int fd;
// Read a region in a block. Negative error codes are propagated
// to the user.
int device_read(const struct lfs_config *c, lfs_block_t block,
        lfs_off_t off, void *buffer, lfs_size_t size)
{
	//debug_printf("read --> address: %d, block: %d, off: %d, size: %d, %p\n", c->block_size * block + off, block, off, size, (int*)buffer);
    	assert(block < c->block_count);
	// go to block
	off_t err = lseek(fd, (off_t)block*c->block_size + (off_t)off, SEEK_SET);
	if (err < 0) {
		debug_printf("fd: %d\n", fd);
		perror("read lseek err");
	    	assert(0);
	    return -1;
	}
	
	// read block
	ssize_t res = read(fd, buffer, (size_t)size);
	if (res < 0) {
	    assert(0);
	    return -1;
	}

	//debug_printf("read boot_count: %p\n", buffer);
	int i;
	for (i = 0; i < size; i++) {
		//debug_printf("%02x ", ((uint8_t *)buffer)[i]);
	}
	//debug_printf("\n===========\n");
	return 0;
}
// Program a region in a block. The block must have previously
// been erased. Negative error codes are propagated to the user.
// May return LFS_ERR_CORRUPT if the block should be considered bad.
int device_prog(const struct lfs_config *c, lfs_block_t block,
        lfs_off_t off, const void *buffer, lfs_size_t size)
{
  	assert(block < c->block_count);

    // go to block
    off_t err = lseek(fd, (off_t)block*c->block_size + (off_t)off, SEEK_SET);
    if (err < 0) {
	    	assert(0);
        return -1;
    }

    // write block
    ssize_t res = write(fd, buffer, (size_t)size);
    if (res < 0) {
	    assert(0);
        return -1;
    }

	//debug_printf("write --> address: %d, block: %d, off: %d, size: %d\n", c->block_size * block + off, block, off, size);
	int i;
	for (i = 0; i < size; i++) {
		//debug_printf("%02x ", ((uint8_t *)buffer)[i]);
	}
	//debug_printf("\n===========\n");

	return 0;
}

// Erase a block. A block must be erased before being programmed.
// The state of an erased block is undefined. Negative error codes
// are propagated to the user.
// May return LFS_ERR_CORRUPT if the block should be considered bad.
int device_erase(const struct lfs_config *c, lfs_block_t block)
{
	debug_printf("erase >>>>>>>>>>>>>>>>>>>>>>>>>>>>\n");
	return 0;
}

// Sync the state of the underlying block device. Negative error codes
// are propagated to the user.
int device_sync(const struct lfs_config *c)
{
	
	int err = fsync(fd);
	if (err) {
		perror("sync error");
		return -1;
	}
	debug_printf("%s %s %d\n", __func__, __FILE__, __LINE__);
	return 0;
}

// configuration of the filesystem is provided by this struct
const struct lfs_config cfg = {
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

static int static_fd;
#define FD_MAX 10
static lfs_t lfs;
static lfs_file_t file;
typedef struct fd_str {
	int fd;
	char *path;
	int flag;
} fd_str;

fd_str * fd_entity[FD_MAX];

static fd_str *search(int fd)
{
	int i;
	fd_str * entity = NULL;
	for (i = 0; i < static_fd; i++) {
		if ((fd_entity[i] != NULL) &&
			(fd_entity[i]->fd == fd) &&
			(fd_entity[i]->flag == 1)) {
			entity = fd_entity[i];
			debug_printf("search fd: %d\n", fd);
			goto out;
		}
	}
	debug_printf("no search fd\n");
out:
	return  entity;
}


int posix_read(int fd, void *buf, size_t count)
{
	int retval = count;
	fd_str * fds = search(fd);
	if ((fds == NULL) && (count >= 256)) {
		debug_printf("%d no search\n", __LINE__);
		goto out;
	}

	lfs_file_read(&lfs, &file, buf, count);
out:
	return retval;
}

int posix_write(int fd, void *buf, size_t count)
{
	int retval = count;
	fd_str * fds = search(fd);
	if ((fds == NULL) && (count >= 256)) {
		debug_printf("%d no search\n", __LINE__);
		goto out;
	}

	lfs_file_write(&lfs, &file, buf, count);
out:
	return retval;
}

/*flag default mode: create | rdwr*/
int posix_open(char *path, int flag)
{
	int retval = -1;
	if (static_fd > FD_MAX) {
		goto out;
	}
	fd_entity[static_fd] = (fd_str *)malloc(sizeof(fd_str));
	fd_str * fds = fd_entity[static_fd];
	fds->flag = 1;
	fds->path = path;
	fds->fd = static_fd;
	lfs_file_open(&lfs, &file, path, LFS_O_RDWR | LFS_O_CREAT);
	static_fd++;
	retval = fds->fd;
out:
	return retval;
}

int posix_close(int fd)
{
	int retval = -1;
	fd_str * fds = search(fd);
	if (fds == NULL) {
		debug_printf("%d no search\n", __LINE__);
		goto out;
	}
	lfs_file_close(&lfs, &file);
	fds->flag = 0;
	free(fds);
	retval = 0;
out:
	return retval;
}

int posix_seek(int fd, int addr, int flag)
{
	int retval = -1;
	fd_str * fds = search(fd);
	if (fds == NULL) {
		debug_printf("%d no search\n", __LINE__);
		goto out;
	}

	lfs_file_rewind(&lfs, &file);
	retval = 0;
out:
	return retval;
}

// entry point
int main(void)
{
#if 1
	fd = open("test_file", O_RDWR);
	if (fd < 0) {
		perror("open error\n");
	} else {
		debug_printf("open success, fd: %d\n", fd);
	}
#endif
	// mount the filesystem
	int err = lfs_mount(&lfs, &cfg);
	// reformat if we can't mount the filesystem
	// this should only happen on the first boot
	if (err) {
		lfs_format(&lfs, &cfg);
		lfs_mount(&lfs, &cfg);
	}
	int one = posix_open("one", 0);
	char aa[10] = {1,2,3,4,5,6,7,8,9,10};
	posix_write(one, aa, 10);
	memset(aa, 0, 10);
	posix_seek(one, 0, 0);
	posix_read(one, aa, 10);
	for (int i = 0; i < 10; i++)
		debug_printf("%02d\t", aa[i]);
	posix_close(one);

	int two = posix_open("two", 0);
	aa[0] = 60;
	posix_write(two, aa, 10);
	memset(aa, 0, 10);
	posix_seek(two, 0, 0);
	posix_read(two, aa, 10);
	for (int i = 0; i < 10; i++)
		debug_printf("%02d\t", aa[i]);
	posix_close(two);

	int three = posix_open("two", 0);
	aa[0] = 61;
	posix_write(three, aa, 10);
	memset(aa, 0, 10);
	posix_seek(three, 0, 0);
	posix_read(three, aa, 10);
	for (int i = 0; i < 10; i++)
		debug_printf("%02d\t", aa[i]);
	posix_close(three);
#if 0

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
	debug_printf("boot_count: %d\n", boot_count);

#endif
	close(fd);
}
