#ifndef ESP_SRTP_COMPAT_H
#define ESP_SRTP_COMPAT_H

#include <stdint.h>
#include <stddef.h>
#include <arpa/inet.h>

/*
 * This compatibility header addresses the warnings between ESP-IDF's uint32_t (which is long unsigned int)
 * and libsrtp's uint32_t (which is unsigned int) on ESP32.
 * 
 * These inline wrapper functions handle the conversions properly and safely.
 */

/* Wrapper for srtp_cipher_encrypt */
static inline srtp_err_status_t esp_srtp_cipher_encrypt(
    srtp_cipher_t *c,
    uint8_t *buffer,
    unsigned int *num_octets_to_output)
{
    uint32_t temp = *num_octets_to_output;
    srtp_err_status_t status = srtp_cipher_encrypt(c, buffer, &temp);
    *num_octets_to_output = temp;
    return status;
}

/* Wrapper for srtp_cipher_decrypt */
static inline srtp_err_status_t esp_srtp_cipher_decrypt(
    srtp_cipher_t *c,
    uint8_t *buffer,
    unsigned int *num_octets_to_output)
{
    uint32_t temp = *num_octets_to_output;
    srtp_err_status_t status = srtp_cipher_decrypt(c, buffer, &temp);
    *num_octets_to_output = temp;
    return status;
}

#endif /* ESP_SRTP_COMPAT_H */