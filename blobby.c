// blobby.c
// blob file archiver
// COMP1521 20T3 Assignment 2
// Written by <YOUR NAME HERE>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <assert.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <dirent.h>
#include <spawn.h>
#include <sys/wait.h>

// the first byte of every blobette has this value
#define BLOBETTE_MAGIC_NUMBER          0x42

// number of bytes in fixed-length blobette fields
#define BLOBETTE_MAGIC_NUMBER_BYTES    1
#define BLOBETTE_MODE_LENGTH_BYTES     3
#define BLOBETTE_PATHNAME_LENGTH_BYTES 2
#define BLOBETTE_CONTENT_LENGTH_BYTES  6
#define BLOBETTE_HASH_BYTES            1

// maximum number of bytes in variable-length blobette fields
#define BLOBETTE_MAX_PATHNAME_LENGTH   65535
#define BLOBETTE_MAX_CONTENT_LENGTH    281474976710655


// ADD YOUR #defines HERE
#define BUF_SIZE 105
#define BYTE_VAL 256


typedef enum action {
	a_invalid,
	a_list,
	a_extract,
	a_create
} action_t;


void usage(char *myname);
action_t process_arguments(int argc, char *argv[], char **blob_pathname,
						   char ***pathnames, int *compress_blob);

void list_blob(char *blob_pathname);
void extract_blob(char *blob_pathname);
void create_blob(char *blob_pathname, char *pathnames[], int compress_blob);

uint8_t blobby_hash(uint8_t hash, uint8_t byte);


// ADD YOUR FUNCTION PROTOTYPES HERE

int get_metadata(int fd, u_int8_t *buf, unsigned long *mode, ssize_t *pathname_len, unsigned long *content_len, u_int8_t *hash_val) {
	int i;
	ssize_t len = 0;
	*pathname_len = 0;
	*content_len = 0;
	*mode = 0;
	u_int8_t _hash_val = 0;
	len = read(fd, buf, BLOBETTE_MAGIC_NUMBER_BYTES);
	if (len <= 0) {
		return 0;
	}
	_hash_val = blobby_hash(_hash_val, buf[0]);
	// printf("%x -> %x\n", buf[0], _hash_val);
	if (buf[0] != 0x42) {
		fprintf(stderr, "ERROR: Magic byte of blobette incorrect\n");
		exit(1);
	}
	read(fd, buf, BLOBETTE_MODE_LENGTH_BYTES);
	for (i=0; i<BLOBETTE_MODE_LENGTH_BYTES; i++) {
		*mode *= BYTE_VAL;
		*mode += buf[i];
		_hash_val = blobby_hash(_hash_val, buf[i]);
		// printf("%x -> %x\n", buf[i], _hash_val);
	}
	// printf("mode = %10o\n", *mode) ;
	read(fd, buf, BLOBETTE_PATHNAME_LENGTH_BYTES);
	for (i=0; i<BLOBETTE_PATHNAME_LENGTH_BYTES; i++) {
		*pathname_len *= BYTE_VAL;
		*pathname_len += buf[i];
		_hash_val = blobby_hash(_hash_val, buf[i]);
		// printf("%x -> %x\n", buf[i], _hash_val);
	}
	// printf("pathlen: %d\n", *pathname_len);
	read(fd, buf, BLOBETTE_CONTENT_LENGTH_BYTES);
	for (i=0; i<BLOBETTE_CONTENT_LENGTH_BYTES; i++) {
		*content_len *= BYTE_VAL;
		*content_len += buf[i];
		_hash_val = blobby_hash(_hash_val, buf[i]);
		// printf("%x -> %x\n", buf[i], _hash_val);
	}
	if (hash_val) {
		*hash_val = _hash_val;
	}
	return 1;
}

void write_content(int fd, char *filename, size_t pathname_len, int flag) {
	int ffd = open(filename, O_RDONLY);
	uint8_t buf[BUF_SIZE + 1];
	struct stat _stat;
	unsigned long content_len;
	u_int8_t tmp, hash_val = 0;
	int len;
	int i;

	i = stat(filename, &_stat);
	if (i < 0) {
		// perror(filename);
		fprintf(stderr, "%s: No such file or directory\n", filename);
		exit(0);
	}
	printf("Adding: %s\n", filename);
	content_len = _stat.st_size;
	if (S_ISDIR(_stat.st_mode)) {
		content_len = 0;
	}
	// printf("content_len: %d\n", content_len);
	buf[0] = 0x42;
	len = write(fd, buf, BLOBETTE_MAGIC_NUMBER_BYTES);
	hash_val = blobby_hash(hash_val, 0x42);

	for (i=0; i<BLOBETTE_MODE_LENGTH_BYTES; i++) {
		int offset = (BLOBETTE_MODE_LENGTH_BYTES - 1 - i) * 8;
		tmp = (_stat.st_mode & (0xff << offset)) >> offset;
		buf[i] = tmp;
		hash_val = blobby_hash(hash_val, tmp);
	}
	write(fd, buf, BLOBETTE_MODE_LENGTH_BYTES);

	for (i=0; i<BLOBETTE_PATHNAME_LENGTH_BYTES; i++) {
		int offset = (BLOBETTE_PATHNAME_LENGTH_BYTES - 1 - i) * 8;
		tmp = (pathname_len & (0xff << offset)) >> offset;
		buf[i] = tmp;
		hash_val = blobby_hash(hash_val, tmp);
	}
	write(fd, buf, BLOBETTE_PATHNAME_LENGTH_BYTES);

	for (i=0; i<BLOBETTE_CONTENT_LENGTH_BYTES; i++) {
		int offset = (BLOBETTE_CONTENT_LENGTH_BYTES - 1 - i) * 8;
		tmp = (content_len & (0xffUL << offset)) >> offset;
		buf[i] = tmp;
		hash_val = blobby_hash(hash_val, tmp);
	}
	write(fd, buf, BLOBETTE_CONTENT_LENGTH_BYTES);

	for (i=0; i<pathname_len; i++) {
		write(fd, filename + i, 1);
		hash_val = blobby_hash(hash_val, filename[i]);
	}

	if (S_ISDIR(_stat.st_mode)) {
		write(fd, &hash_val, BLOBETTE_HASH_BYTES);
		close(ffd);
		if (flag) {
			// printf("open: %s\n", filename);
			DIR *dirp = opendir(filename);
			struct dirent *de;
			if (dirp == NULL) {
				perror("1");
				exit(1);
			}
			while ((de = readdir(dirp)) != NULL) {
				char *c_filename;
				int j = 0;
				if (!strcmp(de->d_name, ".") || !strcmp(de->d_name, "..")) {
					continue;
				}
				c_filename = malloc(BLOBETTE_MAX_PATHNAME_LENGTH);
				memcpy(c_filename, filename, pathname_len);
				c_filename[pathname_len] = '/';
				while (de->d_name[j]) {
					c_filename[pathname_len + j + 1] = de->d_name[j];
					j++;
				}
				c_filename[pathname_len + j + 1] = 0;
				write_content(fd, c_filename, pathname_len + j + 1, 1);
				free(c_filename);
			}
			closedir(dirp);	
		}
	} else if (S_ISREG(_stat.st_mode)) {
		ssize_t copied = 0;
		while (copied < content_len) {
			ssize_t len = content_len - copied;
			if (len > BUF_SIZE) {
				len = BUF_SIZE;
			}
			len = read(ffd, buf, len);
			len = write(fd, buf, len);
			copied += len;
			for(i=0; i<len; i++) {
				hash_val = blobby_hash(hash_val, buf[i]);
			}
			// printf("copied： %d\n", copied);
			// printf("content_len %d\n", content_len);
			// printf("len %d\n", len);
		}
		write(fd, &hash_val, BLOBETTE_HASH_BYTES);
		close(ffd);
	} else {
		close(ffd);
		exit(1);
	}
}



// YOU SHOULD NOT NEED TO CHANGE main, usage or process_arguments

int main(int argc, char *argv[]) {
	char *blob_pathname = NULL;
	char **pathnames = NULL;
	int compress_blob = 0;
	action_t action = process_arguments(argc, argv, &blob_pathname, &pathnames,
										&compress_blob);

	switch (action) {
	case a_list:
		list_blob(blob_pathname);
		break;

	case a_extract:
		extract_blob(blob_pathname);
		break;

	case a_create:
		create_blob(blob_pathname, pathnames, compress_blob);
		break;

	default:
		usage(argv[0]);
	}

	return 0;
}

// print a usage message and exit

void usage(char *myname) {
	fprintf(stderr, "Usage:\n");
	fprintf(stderr, "\t%s -l <blob-file>\n", myname);
	fprintf(stderr, "\t%s -x <blob-file>\n", myname);
	fprintf(stderr, "\t%s [-z] -c <blob-file> pathnames [...]\n", myname);
	exit(1);
}

// process command-line arguments
// check we have a valid set of arguments
// and return appropriate action
// *blob_pathname set to pathname for blobfile
// *pathname and *compress_blob set for create action

action_t process_arguments(int argc, char *argv[], char **blob_pathname,
						   char ***pathnames, int *compress_blob) {
	extern char *optarg;
	extern int optind, optopt;
	int create_blob_flag = 0;
	int extract_blob_flag = 0;
	int list_blob_flag = 0;
	int opt;
	while ((opt = getopt(argc, argv, ":l:c:x:z")) != -1) {
		switch (opt) {
		case 'c':
			create_blob_flag++;
			*blob_pathname = optarg;
			break;

		case 'x':
			extract_blob_flag++;
			*blob_pathname = optarg;
			break;

		case 'l':
			list_blob_flag++;
			*blob_pathname = optarg;
			break;

		case 'z':
			(*compress_blob)++;
			break;

		default:
			return a_invalid;
		}
	}

	if (create_blob_flag + extract_blob_flag + list_blob_flag != 1) {
		return a_invalid;
	}

	if (list_blob_flag && argv[optind] == NULL) {
		return a_list;
	} else if (extract_blob_flag && argv[optind] == NULL) {
		return a_extract;
	} else if (create_blob_flag && argv[optind] != NULL) {
		*pathnames = &argv[optind];
		return a_create;
	}

	return a_invalid;
}

int check_xz(int fd, int *pipe_file_descriptors, pid_t *pid) {
	const int CHECK_LEN = 4;
	u_int8_t head[CHECK_LEN];
	u_int8_t xz_head[] = {0xfd, 0x37, 0x7a, 0x58};
	int i, flag = 1;
	read(fd, head, CHECK_LEN);
	for(i=0; i<CHECK_LEN; i++) {
		if (head[i] != xz_head[i]) {
			flag = 0;
			break;
		}
	}
	lseek(fd, 0, SEEK_SET);
	if (flag == 0) {
		return 0;
	}
	posix_spawn_file_actions_t actions;
	if (pipe(pipe_file_descriptors) == -1) {
		perror("pipe");
		exit(1);
	}
	if (pipe(pipe_file_descriptors + 2) == -1) {
		perror("pipe");
		exit(1);
	}
	if (posix_spawn_file_actions_init(&actions) != 0) {
		perror("posix_spawn_file_actions_init");
		exit(1);
	}

	if (posix_spawn_file_actions_addclose(&actions, pipe_file_descriptors[1]) != 0 ||
			posix_spawn_file_actions_addclose(&actions, pipe_file_descriptors[2]) != 0) {
		perror("posix_spawn_file_actions_addclose");
		exit(1);
	}
	
	if (posix_spawn_file_actions_adddup2(&actions, pipe_file_descriptors[0], 0) != 0 ||
			posix_spawn_file_actions_adddup2(&actions, pipe_file_descriptors[3], 1) != 0) {
		perror("posix_spawn_file_actions_adddup2");
		exit(1);
	}
	char *xz_argv[] = {"xz", "-d", NULL};
	extern char **environ;
	if (posix_spawn(pid, "/usr/bin/xz", &actions, NULL, xz_argv, environ) != 0) {
		perror("spawn");
		exit(1);
	}
	return 1;
	
}


void print_file(int fd) {
	u_int8_t buf[BUF_SIZE];
	ssize_t copied = 0;
	size_t len = 0;
	do {
		len = read(fd, buf, BUF_SIZE);
		int i;
		for(i=0; i<len; i++) {
			printf("%02x ", buf[i]);
		}
		copied += len;
	} while (len > 0);
	printf("\ncopied: %lu\n", copied);
	int g = lseek(fd, 0, SEEK_SET);
	printf("g=%d\n", g);
}
// list the contents of blob_pathname

void list_blob(char *blob_pathname) {
	// int i;
	ssize_t pathname_len = 0;
	uint8_t buf[BUF_SIZE + 1];
	unsigned long content_len = 0;
	unsigned long mode = 0;
	char *filename= malloc(BLOBETTE_MAX_PATHNAME_LENGTH);
	int fd = open(blob_pathname, O_RDONLY);
	if (fd < 0) {
		close(fd);
		free(filename);
		exit(1);
	}
	int pipe_file_descriptors[4];
	pid_t pid;
	int xz = check_xz(fd, pipe_file_descriptors, &pid);
	if (xz) {
		u_int8_t buf[BUF_SIZE];
		int len;
		close(pipe_file_descriptors[0]);
		do {
			len = read(fd, buf, BUF_SIZE);
			// printf("len1 = %d\n", len);
			len = write(pipe_file_descriptors[1], buf, len);
			// printf("len 2= %d\n", len);
		} while (len > 0);
		
		close(pipe_file_descriptors[1]);
		close(pipe_file_descriptors[3]);
		close(fd);
		fd = pipe_file_descriptors[2];
	}

	while (get_metadata(fd, buf, &mode, &pathname_len, &content_len, NULL)) {
		ssize_t copied = 0;
		printf("%06lo %5lu ", mode, content_len);
		// printf("pathname_len: %ld\n", pathname_len);
		// printf("content_len: %ld\n", content_len);

		while (copied < pathname_len) {
			int tmp = read(fd, filename + copied, pathname_len - copied);
			// printf("tmp: %d\n", tmp);
			copied += tmp;
		}
		filename[pathname_len] = 0;
		printf("%s\n", filename);
		copied = 0;
		while (copied < content_len + 1) {
			int left = content_len + 1 - copied;
			if (left > BLOBETTE_MAX_PATHNAME_LENGTH) {
				left = BLOBETTE_MAX_PATHNAME_LENGTH;
			}
			copied += read(fd, filename, left);
		}
		// lseek(fd, content_len+1, SEEK_CUR);
		
	}
	close(fd);
	if (xz) {
		if (waitpid(pid, NULL, 0) == -1) {
			perror("waitpid");
			exit(1);
		}
	}
	free(filename);
}

// extract the contents of blob_pathname

void extract_blob(char *blob_pathname) {

	ssize_t pathname_len = 0;
	uint8_t buf[BUF_SIZE + 1];
	unsigned long content_len = 0;
	unsigned long mode = 0;
	char *filename= malloc(BLOBETTE_MAX_PATHNAME_LENGTH);
	int fd = open(blob_pathname, O_RDONLY);
	u_int8_t hash_val;
	u_int8_t c_hash_val;
	if (fd < 0) {
		close(fd);
		free(filename);
		exit(1);
	}
	while (get_metadata(fd, buf, &mode, &pathname_len, &content_len, &hash_val)) {
		ssize_t copied = 0;
		int cfd;
		int i;
		// while (copied < pathname_len) {
		// 	ssize_t len = pathname_len - copied;
		// 	if (len > BUF_SIZE) {
		// 		len = BUF_SIZE;
		// 	}
		// 	len = read(fd, buf, len);
		// 	// printf("len = %d\n", len);
		// 	buf[len] = 0;
		// 	// printf("-----------\n");
		// 	printf("%s", buf);
		// 	copied += len;
		// 	for(i=0; i<len; i++) {
		// 		hash_val = blobby_hash(hash_val, buf[i]);
		// 		// printf("%x -> %x\n", buf[i], hash_val);
		// 	}
		// }
		read(fd, filename, pathname_len);
		for(i=0; i<pathname_len; i++) {
				hash_val = blobby_hash(hash_val, filename[i]);
				// printf("%x -> %x\n", buf[i], hash_val);
			}
		filename[pathname_len] = 0;
		if (mode & S_IFDIR) {
			printf("Creating directory: %s\n", filename);
			mkdir(filename, mode);
			// chmod(filename, mode);
			read(fd, buf, 1);
			continue;
		}
		printf("Extracting: %s\n", filename);
		cfd = open(filename, O_WRONLY|O_CREAT, 0);
		chmod(filename, mode);
		// printf("mode: %10lo\n", mode);
		copied = 0;
		while (copied < content_len) {
			ssize_t len = content_len - copied;
			if (len > BUF_SIZE) {
				len = BUF_SIZE;
			}
			len = read(fd, buf, len);
			len = write(cfd, buf, len);
			copied += len;
			for(i=0; i<len; i++) {
				hash_val = blobby_hash(hash_val, buf[i]);
				// printf("%x -> %x\n", buf[i], hash_val);
			}
		}
		read(fd, buf, 1);
		c_hash_val = buf[0];
		// printf("c_hash_val: %x\n", c_hash_val);
		// printf("hash_val: %x\n", hash_val);
		// lseek(fd, content_len+1, SEEK_CUR);
		if (c_hash_val != hash_val) {
			fprintf(stderr, "ERROR: blob hash incorrect\n");
			close(cfd);
			close(fd);
			free(filename);
			exit(0);
		}
		close(cfd);
	}
	close(fd);
	free(filename);

}

// create blob_pathname from NULL-terminated array pathnames
// compress with xz if compress_blob non-zero (subset 4)

void create_blob(char *blob_pathname, char *pathnames[], int compress_blob) {

	// printf("create_blob called to create %s blob '%s' containing:\n",
	// 	   compress_blob ? "compressed" : "non-compressed", blob_pathname);

	char *filename= malloc(BLOBETTE_MAX_PATHNAME_LENGTH);
	int fd = open(blob_pathname, O_WRONLY|O_CREAT, 0644);
	int pfd = fd;
	int pipe_file_descriptors[4];
	posix_spawn_file_actions_t actions;
	if (compress_blob) {
		if (pipe(pipe_file_descriptors) == -1) {
			perror("pipe");
			exit(1);
		}
		if (pipe(pipe_file_descriptors + 2) == -1) {
			perror("pipe");
			exit(1);
		}
		if (posix_spawn_file_actions_init(&actions) != 0) {
			perror("posix_spawn_file_actions_init");
			exit(1);
		}

		if (posix_spawn_file_actions_addclose(&actions, pipe_file_descriptors[1]) != 0 ||
				posix_spawn_file_actions_addclose(&actions, pipe_file_descriptors[2]) != 0) {
			perror("posix_spawn_file_actions_addclose");
			exit(1);
		}
		
		if (posix_spawn_file_actions_adddup2(&actions, pipe_file_descriptors[0], 0) != 0 ||
				posix_spawn_file_actions_adddup2(&actions, pipe_file_descriptors[3], 1) != 0) {
			perror("posix_spawn_file_actions_adddup2");
			exit(1);
		}
		pfd = pipe_file_descriptors[1];
		char *xz_argv[] = {"xz", "-c", NULL};
		pid_t pid;
		extern char **environ;
		if (posix_spawn(&pid, "/usr/bin/xz", &actions, NULL, xz_argv, environ) != 0) {
			perror("spawn");
			exit(1);
		}
	}
	for (int p = 0; pathnames[p]; p++) {
		int i = 0;
		char tmp;
		char *_filename = pathnames[p];
		while (_filename[i]) {
			tmp = _filename[i];
			if (tmp != '/') {
				filename[i] = _filename[i];
			} else {
				filename[i] = 0;
				write_content(pfd, filename, i, 0);
				filename[i] = '/';
			}
			i++;
		}
		filename[i] = 0;
		write_content(pfd, filename, i, 1);
	}

	if (compress_blob) {

		u_int8_t buf[BUF_SIZE];
		int len;
		close(pipe_file_descriptors[0]);
		close(pipe_file_descriptors[1]);
		close(pipe_file_descriptors[3]);
		while ((len = read(pipe_file_descriptors[2], buf, BUF_SIZE)) > 0) {
			write(fd, buf, len);
		}
		
		close(pipe_file_descriptors[2]);
		
	}

	close(fd);
	free(filename);
}


// ADD YOUR FUNCTIONS HERE


// YOU SHOULD NOT CHANGE CODE BELOW HERE

// Lookup table for a simple Pearson hash
// https://en.wikipedia.org/wiki/Pearson_hashing
// This table contains an arbitrary permutation of integers 0..255

const uint8_t blobby_hash_table[256] = {
	241, 18,  181, 164, 92,  237, 100, 216, 183, 107, 2,   12,  43,  246, 90,
	143, 251, 49,  228, 134, 215, 20,  193, 172, 140, 227, 148, 118, 57,  72,
	119, 174, 78,  14,  97,  3,   208, 252, 11,  195, 31,  28,  121, 206, 149,
	23,  83,  154, 223, 109, 89,  10,  178, 243, 42,  194, 221, 131, 212, 94,
	205, 240, 161, 7,   62,  214, 222, 219, 1,   84,  95,  58,  103, 60,  33,
	111, 188, 218, 186, 166, 146, 189, 201, 155, 68,  145, 44,  163, 69,  196,
	115, 231, 61,  157, 165, 213, 139, 112, 173, 191, 142, 88,  106, 250, 8,
	127, 26,  126, 0,   96,  52,  182, 113, 38,  242, 48,  204, 160, 15,  54,
	158, 192, 81,  125, 245, 239, 101, 17,  136, 110, 24,  53,  132, 117, 102,
	153, 226, 4,   203, 199, 16,  249, 211, 167, 55,  255, 254, 116, 122, 13,
	236, 93,  144, 86,  59,  76,  150, 162, 207, 77,  176, 32,  124, 171, 29,
	45,  30,  67,  184, 51,  22,  105, 170, 253, 180, 187, 130, 156, 98,  159,
	220, 40,  133, 135, 114, 147, 75,  73,  210, 21,  129, 39,  138, 91,  41,
	235, 47,  185, 9,   82,  64,  87,  244, 50,  74,  233, 175, 247, 120, 6,
	169, 85,  66,  104, 80,  71,  230, 152, 225, 34,  248, 198, 63,  168, 179,
	141, 137, 5,   19,  79,  232, 128, 202, 46,  70,  37,  209, 217, 123, 27,
	177, 25,  56,  65,  229, 36,  197, 234, 108, 35,  151, 238, 200, 224, 99,
	190
};

// Given the current hash value and a byte
// blobby_hash returns the new hash value
//
// Call repeatedly to hash a sequence of bytes, e.g.:
// uint8_t hash = 0;
// hash = blobby_hash(hash, byte0);
// hash = blobby_hash(hash, byte1);
// hash = blobby_hash(hash, byte2);
// ...

uint8_t blobby_hash(uint8_t hash, uint8_t byte) {
	return blobby_hash_table[hash ^ byte];
}
