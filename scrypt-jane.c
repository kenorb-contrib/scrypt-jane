/*
	scrypt-jane by Andrew M, https://github.com/floodyberry/scrypt-jane

	Public Domain or MIT License, whichever is easier
*/

#include <string.h>

#include "scrypt-jane.h"
#include "code/scrypt-jane-portable.h"
#include "code/scrypt-jane-hash.h"
#include "code/scrypt-jane-romix.h"
#include "code/scrypt-jane-test-vectors.h"


#define scrypt_maxNfactor 30  /* (1 << (30 + 1)) = ~2 billion */
#if (SCRYPT_BLOCK_BYTES == 64)
#define scrypt_r_32kb 8 /* (1 << 8) = 256 * 2 blocks in a chunk * 64 bytes = Max of 32kb in a chunk */
#elif (SCRYPT_BLOCK_BYTES == 128)
#define scrypt_r_32kb 7 /* (1 << 7) = 128 * 2 blocks in a chunk * 128 bytes = Max of 32kb in a chunk */
#elif (SCRYPT_BLOCK_BYTES == 256)
#define scrypt_r_32kb 6 /* (1 << 6) = 64 * 2 blocks in a chunk * 256 bytes = Max of 32kb in a chunk */
#elif (SCRYPT_BLOCK_BYTES == 512)
#define scrypt_r_32kb 5 /* (1 << 5) = 32 * 2 blocks in a chunk * 512 bytes = Max of 32kb in a chunk */
#endif
#define scrypt_maxrfactor scrypt_r_32kb /* 32kb */
#define scrypt_maxpfactor 25  /* (1 << 25) = ~33 million */

#include <stdio.h>
//#include <malloc.h>

static void NORETURN
scrypt_fatal_error_default(const char *msg) {
	fprintf(stderr, "%s\n", msg);
	exit(1);
}

static scrypt_fatal_errorfn scrypt_fatal_error = scrypt_fatal_error_default;

void
scrypt_set_fatal_error(scrypt_fatal_errorfn fn) {
	scrypt_fatal_error = fn;
}

static int
scrypt_power_on_self_test(void) {
	const scrypt_test_setting *t;
	uint8_t test_digest[64];
	uint32_t i;
	int res = 7, scrypt_valid;

	if (!scrypt_test_mix()) {
#if !defined(SCRYPT_TEST)
		scrypt_fatal_error("scrypt: mix function power-on-self-test failed");
#endif
		res &= ~1;
	}

	if (!scrypt_test_hash()) {
#if !defined(SCRYPT_TEST)
		scrypt_fatal_error("scrypt: hash function power-on-self-test failed");
#endif
		res &= ~2;
	}

	for (i = 0, scrypt_valid = 1; post_settings[i].pw; i++) {
		t = post_settings + i;
		scrypt((uint8_t *)t->pw, strlen(t->pw), (uint8_t *)t->salt, strlen(t->salt), t->Nfactor, t->rfactor, t->pfactor, test_digest, sizeof(test_digest));
		scrypt_valid &= scrypt_verify(post_vectors[i], test_digest, sizeof(test_digest));
	}

	if (!scrypt_valid) {
#if !defined(SCRYPT_TEST)
		scrypt_fatal_error("scrypt: scrypt power-on-self-test failed");
#endif
		res &= ~4;
	}

	return res;
}



#if defined(SCRYPT_TEST_SPEED)
static uint8_t *mem_base = (uint8_t *)0;
static size_t mem_bump = 0;

/* allocations are assumed to be multiples of 64 bytes and total allocations not to exceed ~1.01gb */
static scrypt_aligned_alloc
scrypt_alloc(uint64_t size) {
	scrypt_aligned_alloc aa;
	if (!mem_base) {
		mem_base = (uint8_t *)malloc((1024 * 1024 * 1024) + (1024 * 1024) + (SCRYPT_BLOCK_BYTES - 1));
		if (!mem_base)
			scrypt_fatal_error("scrypt: out of memory");
		mem_base = (uint8_t *)(((size_t)mem_base + (SCRYPT_BLOCK_BYTES - 1)) & ~(SCRYPT_BLOCK_BYTES - 1));
	}
	aa.mem = mem_base + mem_bump;
	aa.ptr = aa.mem;
	mem_bump += (size_t)size;
	return aa;
}

static void
scrypt_free(scrypt_aligned_alloc *aa) {
	mem_bump = 0;
}
#else
static scrypt_aligned_alloc
scrypt_alloc(uint64_t size) {
	static const size_t max_alloc = (size_t)-1;
	scrypt_aligned_alloc aa;
	size += (SCRYPT_BLOCK_BYTES - 1);
	if (size > max_alloc)
		scrypt_fatal_error("scrypt: not enough address space on this CPU to allocate required memory");
	aa.mem = (uint8_t *)malloc((size_t)size);
	aa.ptr = (uint8_t *)(((size_t)aa.mem + (SCRYPT_BLOCK_BYTES - 1)) & ~(SCRYPT_BLOCK_BYTES - 1));
    //fprintf(stderr, "scrypt_alloc(%zu)\n", size);
	if (!aa.mem)
		scrypt_fatal_error("scrypt: out of memory");
	return aa;
}

static void
scrypt_free(const scrypt_aligned_alloc *aa) {
	free(aa->mem);
}
#endif


void
scrypt(const uint8_t *password, size_t password_len, const uint8_t *salt, size_t salt_len, uint8_t Nfactor, uint8_t rfactor, uint8_t pfactor, uint8_t *out, size_t bytes) {
	static scrypt_aligned_alloc YX, V;
	uint8_t *X, *Y;
	uint32_t N, r, p, chunk_bytes, i;
    static int last_Nfactor = -1, last_rfactor = -1, last_pfactor = -1;

#if !defined(SCRYPT_CHOOSE_COMPILETIME)
	scrypt_ROMixfn scrypt_ROMix = scrypt_getROMix();
#endif

#if defined(SCRYPT_TEST)
	static int power_on_self_test = 0;
	if (!power_on_self_test) {
		power_on_self_test = 1;
		if (!scrypt_power_on_self_test())
			scrypt_fatal_error("scrypt: power on self test failed");
	}
#endif

	if (Nfactor > scrypt_maxNfactor)
		scrypt_fatal_error("scrypt: N out of range");
	if (rfactor > scrypt_maxrfactor)
		scrypt_fatal_error("scrypt: r out of range");
	if (pfactor > scrypt_maxpfactor)
		scrypt_fatal_error("scrypt: p out of range");

	N = (1 << (Nfactor + 1));
	r = (1 << rfactor);
	p = (1 << pfactor);

    //fprintf(stderr, "scrypt(%u,%u,%u) %p %p %p %p\n", N, r, p, &V, V.mem, &YX, YX.mem);
	chunk_bytes = SCRYPT_BLOCK_BYTES * r * 2;
    if (Nfactor != last_Nfactor || rfactor != last_rfactor || pfactor != last_pfactor) {
        //fprintf(stderr, "need to reallocate\n");
        if (last_Nfactor >= 0 || last_rfactor >= 0 || last_pfactor >= 0) {
            //fprintf(stderr, "freeing old V and YX\n");
	        scrypt_free(&V);
	        scrypt_free(&YX);
        }
	    V = scrypt_alloc((uint64_t)N * chunk_bytes);
	    YX = scrypt_alloc((p + 1) * chunk_bytes);
        last_Nfactor = Nfactor;
        last_rfactor = rfactor;
        last_pfactor = pfactor;
    }

	/* 1: X = PBKDF2(password, salt) */
	Y = YX.ptr;
	X = Y + chunk_bytes;
	scrypt_pbkdf2(password, password_len, salt, salt_len, 1, X, chunk_bytes * p);

	/* 2: X = ROMix(X) */
	for (i = 0; i < p; i++)
		scrypt_ROMix((scrypt_mix_word_t *)(X + (chunk_bytes * i)), (scrypt_mix_word_t *)Y, (scrypt_mix_word_t *)V.ptr, N, r);

	/* 3: Out = PBKDF2(password, X) */
	scrypt_pbkdf2(password, password_len, X, chunk_bytes * p, 1, out, bytes);

	//scrypt_ensure_zero(YX.ptr, (p + 1) * chunk_bytes);
}

const scrypt_instance* new_instance(unsigned char Nfactor, unsigned char rfactor, unsigned char pfactor) {
	if (Nfactor > scrypt_maxNfactor)
		scrypt_fatal_error("scrypt: N out of range");
	if (rfactor > scrypt_maxrfactor)
		scrypt_fatal_error("scrypt: r out of range");
	if (pfactor > scrypt_maxpfactor)
		scrypt_fatal_error("scrypt: p out of range");

	scrypt_instance* instance = (scrypt_instance *)malloc((size_t)sizeof(scrypt_instance));

	instance->N = (1 << (Nfactor + 1));
	instance->r = (1 << rfactor);
	instance->p = (1 << pfactor);

	uint32_t chunk_bytes = SCRYPT_BLOCK_BYTES * instance->r * 2;
	instance->V = scrypt_alloc((uint64_t)instance->N * chunk_bytes);
	instance->YX = scrypt_alloc((instance->p + 1) * chunk_bytes);
	return instance;
}

void free_instance(const scrypt_instance* instance) {
	scrypt_free(&instance->V);
	scrypt_free(&instance->YX);
	free((void*)instance);
}


void scrypt_preallocated(const scrypt_instance* const instance, const unsigned char *password, size_t password_len, const unsigned char *salt, size_t salt_len, unsigned char *out, size_t bytes) {
#if !defined(SCRYPT_CHOOSE_COMPILETIME)
	scrypt_ROMixfn scrypt_ROMix = scrypt_getROMix();
#endif

#if defined(SCRYPT_TEST)
	static int power_on_self_test = 0;
	if (!power_on_self_test) {
		power_on_self_test = 1;
		if (!scrypt_power_on_self_test())
			scrypt_fatal_error("scrypt: power on self test failed");
	}
#endif
	uint32_t chunk_bytes = SCRYPT_BLOCK_BYTES * instance->r * 2;
	/* 1: X = PBKDF2(password, salt) */
	uint8_t* Y = instance->YX.ptr;
	uint8_t* X = Y + chunk_bytes;
	scrypt_pbkdf2(password, password_len, salt, salt_len, 1, X, chunk_bytes * instance->p);

	/* 2: X = ROMix(X) */
	for (uint32_t i = 0; i < instance->p; i++) {
		scrypt_ROMix((scrypt_mix_word_t *)(X + (chunk_bytes * i)), (scrypt_mix_word_t *)Y, (scrypt_mix_word_t *)instance->V.ptr, instance->N, instance->r);
	}

	/* 3: Out = PBKDF2(password, X) */
	scrypt_pbkdf2(password, password_len, X, chunk_bytes * instance->p, 1, out, bytes);
}
