/* VitaSDK 2026.08 ships a libcurl archive built against the OpenSSL 1.0 ABI,
 * while its OpenSSL library is 1.1.1. Export the removed ABI entry points
 * that libcurl still references, forwarding them to their OpenSSL 1.1 forms.
 */
#include <openssl/opensslv.h>

#if OPENSSL_VERSION_NUMBER >= 0x10100000L
#include <openssl/ssl.h>
#include <openssl/evp.h>
#include <openssl/crypto.h>
#include <openssl/stack.h>

const SSL_METHOD *dm_ssl23_client_method(void) __asm__("SSLv23_client_method");
const SSL_METHOD *dm_ssl23_client_method(void) { return TLS_client_method(); }

EVP_MD_CTX *dm_evp_md_ctx_create(void) __asm__("EVP_MD_CTX_create");
EVP_MD_CTX *dm_evp_md_ctx_create(void) { return EVP_MD_CTX_new(); }
void dm_evp_md_ctx_destroy(EVP_MD_CTX *ctx) __asm__("EVP_MD_CTX_destroy");
void dm_evp_md_ctx_destroy(EVP_MD_CTX *ctx) { EVP_MD_CTX_free(ctx); }

int dm_ssl_library_init(void) __asm__("SSL_library_init");
int dm_ssl_library_init(void) { return OPENSSL_init_ssl(0, NULL) == 1; }
void dm_ssl_load_error_strings(void) __asm__("SSL_load_error_strings");
void dm_ssl_load_error_strings(void) { OPENSSL_init_ssl(OPENSSL_INIT_LOAD_SSL_STRINGS, NULL); }
unsigned long dm_ssleay(void) __asm__("SSLeay");
unsigned long dm_ssleay(void) { return OpenSSL_version_num(); }

void dm_add_all_algorithms(void) __asm__("OPENSSL_add_all_algorithms_noconf");
void dm_add_all_algorithms(void) { OPENSSL_init_crypto(OPENSSL_INIT_ADD_ALL_CIPHERS | OPENSSL_INIT_ADD_ALL_DIGESTS, NULL); }
void dm_err_free_strings(void) __asm__("ERR_free_strings");
void dm_err_free_strings(void) { }
void dm_evp_cleanup(void) __asm__("EVP_cleanup");
void dm_evp_cleanup(void) { }
void dm_engine_cleanup(void) __asm__("ENGINE_cleanup");
void dm_engine_cleanup(void) { }
void dm_conf_modules_free(void) __asm__("CONF_modules_free");
void dm_conf_modules_free(void) { }
void dm_ssl_comp_free(void) __asm__("SSL_COMP_free_compression_methods");
void dm_ssl_comp_free(void) { }

int dm_sk_num(const OPENSSL_STACK *s) __asm__("sk_num");
int dm_sk_num(const OPENSSL_STACK *s) { return OPENSSL_sk_num(s); }
void *dm_sk_value(const OPENSSL_STACK *s, int i) __asm__("sk_value");
void *dm_sk_value(const OPENSSL_STACK *s, int i) { return OPENSSL_sk_value(s, i); }
void *dm_sk_pop(OPENSSL_STACK *s) __asm__("sk_pop");
void *dm_sk_pop(OPENSSL_STACK *s) { return OPENSSL_sk_pop(s); }
void dm_sk_pop_free(OPENSSL_STACK *s, OPENSSL_sk_freefunc f) __asm__("sk_pop_free");
void dm_sk_pop_free(OPENSSL_STACK *s, OPENSSL_sk_freefunc f) { OPENSSL_sk_pop_free(s, f); }

/* OpenSSL 1.1 removed UI_OpenSSL; libcurl only uses it as an optional
 * interactive password UI, which is unavailable in Desktop Mode. */
const UI_METHOD *dm_ui_openssl(void) __asm__("UI_OpenSSL");
const UI_METHOD *dm_ui_openssl(void) { return NULL; }
#endif
