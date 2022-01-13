#include "operations.h"
#include "config.h"
#include <math.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

pthread_mutex_t open_lock = PTHREAD_MUTEX_INITIALIZER;

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
	bool append = false;
	int fhandle;

	/* Checks if the path name is valid */
	if (!valid_pathname(name)) {
		return -1;
	}

	pthread_mutex_lock(&open_lock);
	inum = tfs_lookup(name);
	pthread_mutex_unlock(&open_lock);

	if (inum >= 0) {
		/* The file already exists */
		inode_t *inode = inode_get(inum);
		if (inode == NULL) {
			return -1;
		}
		pthread_rwlock_wrlock(&inode->lock);

		/* Trucate (if requested) */
		if (flags & TFS_O_TRUNC) {
			if (inode->i_size > 0) {
				for (int i = 0; inode->i_data_block[i] != -1; i++) {
					if (data_block_free(inode->i_data_block[i]) == -1) {
						pthread_rwlock_unlock(&inode->lock);
						return -1;
					}
				}

				if (inode->i_indirect_block != -1) {
					int *indirect_block = data_block_get(inode->i_indirect_block);
					for (int i = 0; indirect_block[i] != -1; i++) {
						if (data_block_free(indirect_block[i]) == -1) {
							pthread_rwlock_unlock(&inode->lock);
							return -1;
						}
					}
				}

				inode->i_size = 0;
			}
		}

		/* Determine initial offset */
		if (flags & TFS_O_APPEND) {
			offset = inode->i_size;
			append = true;
		} else {
			offset = 0;
		}

		pthread_rwlock_unlock(&inode->lock);

	} else if (flags & TFS_O_CREAT) {
		/* The file doesn't exist; the flags specify that it should be created */
		/* Create inode */
		inum = inode_create(T_FILE);
		if (inum == -1) {
			return -1;
		}
		inode_t *inode = inode_get(inum);

		pthread_rwlock_wrlock(&inode->lock);

		/* Add entry in the root directory */
		if (add_dir_entry(ROOT_DIR_INUM, inum, name + 1) == -1) {
			inode_delete(inum);
			pthread_rwlock_unlock(&inode->lock);
			return -1;
		}
		offset = 0;

		pthread_rwlock_unlock(&inode->lock);

	} else {
		return -1;
	}

	/* Finally, add entry to the open file table and
	 * return the corresponding handle */
	return add_to_open_file_table(inum, offset, append);

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
	int *indirect_block;

	pthread_rwlock_wrlock(&inode->lock);
	pthread_mutex_lock(&file->lock);

	if (file->of_append) {
		file->of_offset = inode->i_size;
	}

	/* Block to start writing to */
	int block_ind = file->of_offset / BLOCK_SIZE;
	while (to_write > 0) {

		/* Locate current block */
		/* Direct blocks */
		if (block_ind < DIRECT_BLOCK_NUMBER) {

			/* Allocate and get direct block */
			if (inode->i_data_block[block_ind] == -1) {
				inode->i_data_block[block_ind] = data_block_alloc();
			}
			if ((block = data_block_get(inode->i_data_block[block_ind++])) == NULL) {
				pthread_rwlock_unlock(&inode->lock);
				pthread_mutex_unlock(&file->lock);
				return -1;
			}

		/* Supplemental blocks */
		} else if (block_ind < INDIRECT_BLOCK_NUMBER + DIRECT_BLOCK_NUMBER) {
			if (inode->i_indirect_block == -1) {
				inode->i_indirect_block = data_block_alloc();
				if ((indirect_block = data_block_get(inode->i_indirect_block)) == NULL) {
					pthread_rwlock_unlock(&inode->lock);
					pthread_mutex_unlock(&file->lock);
					return -1;
				}

				/* Initialize supplemental blocks */
				for (int i = 0; i < INDIRECT_BLOCK_NUMBER; i++) {
					indirect_block[i] = -1;
				}
			}

			if ((indirect_block = data_block_get(inode->i_indirect_block)) == NULL) {
				pthread_rwlock_unlock(&inode->lock);
				pthread_mutex_unlock(&file->lock);
				return -1;
			}

			/* Allocate and get supplemental block */
			if (indirect_block[block_ind - DIRECT_BLOCK_NUMBER] == -1) {
				indirect_block[block_ind - DIRECT_BLOCK_NUMBER] = data_block_alloc();
			}
			if ((block = data_block_get(indirect_block[block_ind++ - DIRECT_BLOCK_NUMBER])) == NULL) {
				pthread_rwlock_unlock(&inode->lock);
				pthread_mutex_unlock(&file->lock);
				return -1;
			}

		/* Can't write the whole buffer */
		} else {
			break;
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

	pthread_rwlock_unlock(&inode->lock);
	pthread_mutex_unlock(&file->lock);

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

	pthread_rwlock_rdlock(&inode->lock);

	pthread_mutex_lock(&file->lock);
	size_t offset = file->of_offset;
	pthread_mutex_unlock(&file->lock);

	/* Determine how many bytes to read */
	size_t to_read = inode->i_size - offset;
	if (to_read > len) {
		to_read = len;
	}

	/* Perform the read */
	ssize_t read = 0;
	ssize_t current_read = 0;
	void *block;
	int block_ind = offset / BLOCK_SIZE;

	while (to_read > 0) {

		if (block_ind < DIRECT_BLOCK_NUMBER) {
			if ((block = data_block_get(inode->i_data_block[block_ind++])) == NULL) {
				pthread_rwlock_unlock(&inode->lock);
				return -1;
			}

		} else if (block_ind < DIRECT_BLOCK_NUMBER + INDIRECT_BLOCK_NUMBER) {
			int *indirect_block;
			if ((indirect_block = data_block_get(inode->i_indirect_block)) == NULL) {
				pthread_rwlock_unlock(&inode->lock);
				return -1;
			}

			if ((block = data_block_get(indirect_block[block_ind++ - DIRECT_BLOCK_NUMBER])) == NULL) {
				pthread_rwlock_unlock(&inode->lock);
				return -1;
			}

		} else {
			break;
		}

		/* Perform the actual read */
		/* First block */
		if (read == 0) {
			if ((to_read + (offset % BLOCK_SIZE)) / BLOCK_SIZE > 0) {
				current_read = BLOCK_SIZE - (offset % BLOCK_SIZE);
			} else {
				current_read = to_read;
			}
			memcpy(buffer, block + (offset % BLOCK_SIZE), current_read);

		/* Other blocks */
		} else {
			if (to_read / BLOCK_SIZE > 0) {
				current_read = BLOCK_SIZE;
			} else {
				current_read = to_read;
			}
			memcpy(buffer + read, block, current_read);
		}

		read += current_read;
		to_read -= current_read;
	}

	/* The offset associated with the file handle is
	 * incremented accordingly */
	pthread_mutex_lock(&file->lock);
	file->of_offset += read;
	pthread_mutex_unlock(&file->lock);

	pthread_rwlock_unlock(&inode->lock);

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
