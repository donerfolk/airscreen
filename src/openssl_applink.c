/* Compiled as C into AirScreen.exe. OpenSSL DLLs look up OPENSSL_Applink
 * via GetProcAddress; without it, BIO/file I/O shows
 * "OPENSSL_Uplink: no OPENSSL_Applink". */
#include <openssl/applink.c>
