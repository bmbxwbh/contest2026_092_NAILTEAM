/**
 * @file triad_ca_api.h
 * Stub implementation for triad CA API.
 * Provides empty implementations when vendor/xiaomi/mitee_iot is not available.
 */

#ifndef __TRIAD_CA_API_H__
#define __TRIAD_CA_API_H__

#include <stdint.h>

/* Stub: always returns -1 (failure) to indicate no triad data available */
static inline int triad_load_did(uint8_t *did, int len)
{
  (void)did;
  (void)len;
  return -1;
}

static inline int triad_load_key(uint8_t *key, int len)
{
  (void)key;
  (void)len;
  return -1;
}

#endif /* __TRIAD_CA_API_H__ */
