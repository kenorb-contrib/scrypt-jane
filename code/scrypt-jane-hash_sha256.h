#define SCRYPT_HASH "SHA-2-256"
#define SCRYPT_HASH_BLOCK_SIZE 64
#define SCRYPT_HASH_DIGEST_SIZE 32

#include <openssl/sha.h>

typedef uint8_t scrypt_hash_digest[SCRYPT_HASH_DIGEST_SIZE];
typedef SHA256_CTX scrypt_hash_state;

static void
scrypt_hash_init(scrypt_hash_state *S) {
	SHA256_Init(S);
}

static void
scrypt_hash_update(scrypt_hash_state *S, const uint8_t *in, size_t inlen) {
	SHA256_Update(S, in, inlen);
}

static void
scrypt_hash_finish(scrypt_hash_state *S, uint8_t *hash) {
	SHA256_Final(hash, S);
}


static const uint8_t scrypt_test_hash_expected[SCRYPT_HASH_DIGEST_SIZE] = {
	0xee,0x36,0xae,0xa6,0x65,0xf0,0x28,0x7d,0xc9,0xde,0xd8,0xad,0x48,0x33,0x7d,0xbf,
	0xcb,0xc0,0x48,0xfa,0x5f,0x92,0xfd,0x0a,0x95,0x6f,0x34,0x8e,0x8c,0x1e,0x73,0xad,
};
