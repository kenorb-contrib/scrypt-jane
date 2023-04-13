#ifndef SCRYPT_JANE_H
#define SCRYPT_JANE_H

/*
	Nfactor: Increases CPU & Memory Hardness
	N = (1 << (Nfactor + 1)): How many times to mix a chunk and how many temporary chunks are used

	rfactor: Increases Memory Hardness
	r = (1 << rfactor): How large a chunk is

	pfactor: Increases CPU Hardness
	p = (1 << pfactor): Number of times to mix the main chunk

	A block is the basic mixing unit (salsa/chacha block = 64 bytes)
	A chunk is (2 * r) blocks

	~Memory used = (N + 2) * ((2 * r) * block size)
*/

#include <stdlib.h>
#include <stdint.h>

typedef struct scrypt_aligned_alloc_t {
	uint8_t *mem, *ptr;
} scrypt_aligned_alloc;

typedef struct scrypt_instance_t {
	scrypt_aligned_alloc V;
	scrypt_aligned_alloc YX;
	uint32_t N, r, p;
} scrypt_instance;

typedef void (*scrypt_fatal_errorfn)(const char *msg);
void scrypt_set_fatal_error(scrypt_fatal_errorfn fn);

void scrypt(const unsigned char *password, size_t password_len, const unsigned char *salt, size_t salt_len, unsigned char Nfactor, unsigned char rfactor, unsigned char pfactor, unsigned char *out, size_t bytes);

void scrypt_preallocated(const scrypt_instance* const instance, const unsigned char *password, size_t password_len, const unsigned char *salt, size_t salt_len, unsigned char *out, size_t bytes);

const scrypt_instance* new_instance(unsigned char Nfactor, unsigned char rfactor, unsigned char pfactor);

void free_instance(const scrypt_instance* instance);

#endif /* SCRYPT_JANE_H */
