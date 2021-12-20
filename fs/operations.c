#include "operations.h"
#include "config.h"
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int tfs_init() {
	state_init();

	/* create root inode */
	int root = inode_create(T_DIRECTORY);
	if (root != ROOT_DIR_INUM) {
		return -1;
	}

	return 0;
}

int tfs_destroy() {
	state_destroy();
	return 0;
}

static bool valid_pathname(char const *name) {
	return name != NULL && strlen(name) > 1 && name[0] == '/';
}

int tfs_lookup(char const *name) {
	if (!valid_pathname(name)) {
		return -1;
	}

	// skip the initial '/' character
	name++;

	return find_in_dir(ROOT_DIR_INUM, name);
}

int tfs_open(char const *name, int flags) {
	int inum;
	size_t offset;

	/* Checks if the path name is valid */
	if (!valid_pathname(name)) {
		return -1;
	}

	inum = tfs_lookup(name);
	if (inum >= 0) {
		/* The file already exists */
		inode_t *inode = inode_get(inum);

		if (inode == NULL) {
			return -1;
		}

		/* Trucate (if requested) */
		if (flags & TFS_O_TRUNC) {
			if (inode->i_size > 0) {
				int blocks = number_blocks_alloc(inum);
				for (int i = 0; i < blocks; i++) {
					if (data_block_free(inode->i_data_block[i]) == -1) {
						return -1;
					}
				}
				inode->i_size = 0;
			}
		}

		/* Determine initial offset */
		if (flags & TFS_O_APPEND) {
			offset = inode->i_size;
		} else {
			offset = 0;
		}

	} else if (flags & TFS_O_CREAT) {
		/* The file doesn't exist; the flags specify that it should be created*/
		/* Create inode */
		inum = inode_create(T_FILE);
		if (inum == -1) {
			return -1;
		}
		/* Add entry in the root directory */
		if (add_dir_entry(ROOT_DIR_INUM, inum, name + 1) == -1) {
			inode_delete(inum);
			return -1;
		}
		offset = 0;

	} else {
		return -1;
	}

	/* Finally, add entry to the open file table and
	 * return the corresponding handle */
	return add_to_open_file_table(inum, offset);

	/* Note: for simplification, if file was created with TFS_O_CREAT and there
	 * is an error adding an entry to the open file table, the file is not
	 * opened but it remains created */
}

int tfs_close(int fhandle) {
	return remove_from_open_file_table(fhandle);
}

ssize_t tfs_write(int fhandle, void const *buffer, size_t to_write) {
	open_file_entry_t *file = get_open_file_entry(fhandle);
	if (file == NULL) {
		return -1;
	}

	/* From the open file table entry, we get the inode */
	inode_t *inode = inode_get(file->of_inumber);
	if (inode == NULL) {
		return -1;
	}

	/* Perform the write */
	ssize_t written = 0;
	ssize_t current_written = 0;
	void *block;
	int block_ind = file->of_offset / BLOCK_SIZE;

	while (to_write > 0) {

		/* Alloc new block */
		if (inode->i_data_block[block_ind] == -1) {
			inode->i_data_block[block_ind] = data_block_alloc();
		}

		/* Check for valid block */
		if ((block = data_block_get(inode->i_data_block[block_ind++])) == NULL) {
			return -1;
		}

		/* First block */
		if (written == 0) {
			if ((to_write + (file->of_offset % BLOCK_SIZE)) / BLOCK_SIZE > 0) {
				current_written = BLOCK_SIZE - (file->of_offset % BLOCK_SIZE);
			} else {
				current_written = to_write;
			}
			memcpy(block + (file->of_offset % BLOCK_SIZE), buffer, current_written);

		/* Other blocks */
		} else {
			if (to_write / BLOCK_SIZE > 0) {
				current_written = BLOCK_SIZE;
			} else {
				current_written = to_write;
			}
			memcpy(block, buffer + written, current_written);
		}

		written += current_written;
		to_write -= current_written;
	}

	/* Increment the offset associated with the file handle */
	file->of_offset += written;
	if (file->of_offset > inode->i_size) {
		inode->i_size = file->of_offset;
	}

	return written;
}

ssize_t tfs_read(int fhandle, void *buffer, size_t len) {
	open_file_entry_t *file = get_open_file_entry(fhandle);
	if (file == NULL) {
		return -1;
	}

	/* From the open file table entry, we get the inode */
	inode_t *inode = inode_get(file->of_inumber);
	if (inode == NULL) {
		return -1;
	}

	/* Determine how many bytes to read */
	size_t to_read = inode->i_size - file->of_offset;
	if (to_read > len) {
		to_read = len;
	}

	/* Perform the read */
	ssize_t read = 0;
	ssize_t current_read = 0;
	void *block;
	int block_ind = file->of_offset / BLOCK_SIZE;

	while (to_read > 0) {

		/* Check for valid block */
		if ((block = data_block_get(inode->i_data_block[block_ind++])) == NULL) {
			return -1;
		}

		/* Perform the actual read */
		/* First block */
		if (read == 0) {
			if ((to_read + (file->of_offset % BLOCK_SIZE)) / BLOCK_SIZE > 0) {
				current_read = BLOCK_SIZE - (file->of_offset % BLOCK_SIZE);
			} else {
				current_read = to_read;
			}
			memcpy(buffer, block + (file->of_offset % BLOCK_SIZE), current_read);

		/* Other blocks */
		} else {
			if (to_read / BLOCK_SIZE > 0) {
				current_read = BLOCK_SIZE;
			} else {
				current_read = to_read;
			}
			memcpy(buffer, block, current_read);
		}

		read += current_read;
		to_read -= current_read;
	}

	/* The offset associated with the file handle is
	 * incremented accordingly */
	file->of_offset += read;

	return read;
}

int tfs_copy_to_external_fs(char const *source_path, char const *dest_path) {

	/* Check for valid pathnames */
	if (!valid_pathname(source_path) || dest_path == NULL) {
		return -1;
	}

	/* Open tfs file */
	int source_file = tfs_open(source_path, 0);
	if (source_file == -1) {
		return -1;
	}

	/* Open filesystem file */
	FILE *dest_file = fopen(dest_path, "w");
	if (dest_file == NULL) {
		return -1;
	}

	/* Read from source_file and write to dest_file */
	char buffer[128];
	memset(buffer, 0, sizeof(buffer));

	ssize_t bytes_read;

	do {
		bytes_read = tfs_read(source_file, buffer, 128);
		fwrite(buffer, sizeof(char), bytes_read, dest_file);
	} while (bytes_read == 128);

	/* Error reading file */
	if (bytes_read == -1) {
		return -1;
	}

	/* Close files */
	if (dest_file != NULL) {
		fclose(dest_file);
	}
	if (source_file != -1) {
		tfs_close(source_file);
	}

	return 0;
}
