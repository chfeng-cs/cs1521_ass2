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

void read_buf(int fd, char *buf, size_t *len_left) {
	
}

int get_metadata(int fd, u_int8_t *buf, unsigned long *mode, ssize_t *pathname_len, unsigned long *content_len) {
	int i;
	ssize_t len = 0;
	*pathname_len = 0;
	*content_len = 0;
	*mode = 0;
	len = read(fd, buf, BLOBETTE_MAGIC_NUMBER_BYTES);
	if (len <= 0) {
		return 0;
	}
	if (buf[0] != 0x42) {
		fprintf(stderr, "ERROR: Magic byte of blobette incorrect\n");
		exit(1);
	}
	read(fd, buf, BLOBETTE_MODE_LENGTH_BYTES);
	for (i=0; i<BLOBETTE_MODE_LENGTH_BYTES; i++) {
		*mode *= BYTE_VAL;
		*mode += buf[i];
	}
	// printf("mode = %10o\n", mode) ;
	read(fd, buf, BLOBETTE_PATHNAME_LENGTH_BYTES);
	for (i=0; i<BLOBETTE_PATHNAME_LENGTH_BYTES; i++) {
		*pathname_len *= BYTE_VAL;
		*pathname_len += buf[i];
	}
	// printf("pathlen: %d\n", pathname_len);
	read(fd, buf, BLOBETTE_CONTENT_LENGTH_BYTES);
	for (i=0; i<BLOBETTE_CONTENT_LENGTH_BYTES; i++) {
		*content_len *= BYTE_VAL;
		*content_len += buf[i];
	}
	return 1;
}

// void check_magic(int fd, uint8_t* buf) {
// 	int len = read(fd, buf, BUF_SIZE);
// 	uint8_t ret = 0;
// 	while (len > 0) {
// 		int i;
// 		u_int8_t last_bit;
// 		for(i=0; i<0; i++) {
// 			last_bit = 
// 			ret = blobby_hash(ret, buf[i]);
// 		}
// 	}
// 	lseek(fd, 0, SEEK_SET);
// }

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
	while (get_metadata(fd, buf, &mode, &pathname_len, &content_len)) {
		ssize_t copied = 0;
		printf("%06lo %5lu ", mode, content_len);
		while (copied < pathname_len) {
			copied += read(fd, filename + copied, pathname_len - copied);
		}
		filename[pathname_len] = 0;
		printf("%s\n", filename);
		lseek(fd, content_len+1, SEEK_CUR);
	}
	close(fd);
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
	if (fd < 0) {
		close(fd);
		free(filename);
		exit(1);
	}
	while (get_metadata(fd, buf, &mode, &pathname_len, &content_len)) {
		ssize_t copied = 0;
		// unsigned long left_len = content_len;
		int cfd;
		while (copied < pathname_len) {
			copied += read(fd, filename + copied, pathname_len - copied);
		}
		filename[pathname_len] = 0;
		printf("Extracting: %s\n", filename);
		if (mode & S_IFDIR) {
			mkdir(filename, mode);
			read(fd, buf, 1);
			continue;
		}
		cfd = open(filename, O_WRONLY|O_CREAT|O_TRUNC, mode);
		copied = 0;
		while (copied < content_len) {
			ssize_t len = content_len - copied;
			if (len > BUF_SIZE) {
				len = BUF_SIZE;
			}
			printf("len1 = %d\n", len);
			len = read(fd, buf, len);
			printf("len2 = %d\n", len);
			len = write(cfd, buf, len);
			printf("len3 = %d\n", len);
			printf("copied1 = %d\n", copied);
			copied += len;
			printf("copied2 = %d\n", copied);
			printf("content_len: %d\tcopied: %d\n", content_len, copied);
		}
		printf("content_len: %d\tcopied: %d\n", content_len, copied);
		read(fd, buf, 1);
		// lseek(fd, content_len+1, SEEK_CUR);
		close(cfd);
	}
	close(fd);
	free(filename);

}

// create blob_pathname from NULL-terminated array pathnames
// compress with xz if compress_blob non-zero (subset 4)

void create_blob(char *blob_pathname, char *pathnames[], int compress_blob) {

	// REPLACE WITH YOUR CODE FOR -c

	printf("create_blob called to create %s blob '%s' containing:\n",
		   compress_blob ? "compressed" : "non-compressed", blob_pathname);

	for (int p = 0; pathnames[p]; p++) {
		printf("%s\n", pathnames[p]);
	}

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
