/* Copyright 2001,2002,2003 Roger Dingledine. */
/* See LICENSE for licensing information */
/* $Id$ */

#include <stdio.h>
#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif

#ifdef MS_WINDOWS
/* For mkdir() */
#include <direct.h>
#endif

#include "or.h"
#include "../common/test.h"

void
dump_hex(char *s, int len)
{
  static const char TABLE[] = "0123456789ABCDEF";
  unsigned char *d = s;
  int i, j, nyb;
  for(i=0;i<len;++i) {
    for (j=1;j>=0;--j) {
      nyb = (((int) d[i]) >> (j*4)) & 0x0f;
      assert(0<=nyb && nyb <=15);
      putchar(TABLE[nyb]);
    }
  }
}

void
setup_directory() {
  char buf[256];
  int r;
  sprintf(buf, "/tmp/tor_test");
#ifdef _MSC_VER
  r = mkdir(buf);
#else
  r = mkdir(buf, 0700);
#endif
  if (r && errno != EEXIST)
    fprintf(stderr, "Can't create directory %s", buf);
}

void
test_buffers() {
#define MAX_BUF_SIZE 1024*1024
  char str[256];
  char str2[256];

  buf_t *buf;
  buf_t *buf2;

  int s, i, j, eof;

  /****
   * buf_new
   ****/
  if (!(buf = buf_new()))
    test_fail();

  test_eq(buf_capacity(buf), 512*1024);
  test_eq(buf_datalen(buf), 0);

  /****
   * read_to_buf
   ****/
  s = open("/tmp/tor_test/data", O_WRONLY|O_CREAT|O_TRUNC, 0600);
  for (j=0;j<256;++j) {
    str[j] = (char)j;
  }
  write(s, str, 256);
  close(s);
  
  s = open("/tmp/tor_test/data", O_RDONLY, 0);
  eof = 0;
  i = read_to_buf(s, 10, buf, &eof);
  test_eq(buf_capacity(buf), 512*1024);
  test_eq(buf_datalen(buf), 10);
  test_eq(eof, 0);
  test_eq(i, 10);
  test_memeq(str, (char*)_buf_peek_raw_buffer(buf), 10);

  /* Test reading 0 bytes. */
  i = read_to_buf(s, 0, buf, &eof);
  test_eq(buf_capacity(buf), 512*1024);
  test_eq(buf_datalen(buf), 10);
  test_eq(eof, 0);
  test_eq(i, 0);

  /* Now test when buffer is filled exactly. */
  buf2 = buf_new_with_capacity(6);
  i = read_to_buf(s, 6, buf2, &eof);
  test_eq(buf_capacity(buf2), 6);
  test_eq(buf_datalen(buf2), 6);
  test_eq(eof, 0);
  test_eq(i, 6);
  test_memeq(str+10, (char*)_buf_peek_raw_buffer(buf2), 6);
  buf_free(buf2);
  
  /* Now test when buffer is filled with more data to read. */
  buf2 = buf_new_with_capacity(32);
  i = read_to_buf(s, 128, buf2, &eof);
  test_eq(buf_capacity(buf2), 128);
  test_eq(buf_datalen(buf2), 32);
  test_eq(eof, 0);
  test_eq(i, 32);
  buf_free(buf2);

  /* Now read to eof. */
  test_assert(buf_capacity(buf) > 256);
  i = read_to_buf(s, 1024, buf, &eof);
  test_eq(i, (256-32-10-6));
  test_eq(buf_capacity(buf), MAX_BUF_SIZE);
  test_eq(buf_datalen(buf), 256-6-32);
  test_memeq(str, (char*)_buf_peek_raw_buffer(buf), 10); /* XXX Check rest. */
  test_eq(eof, 0);

  i = read_to_buf(s, 1024, buf, &eof);
  test_eq(i, 0);
  test_eq(buf_capacity(buf), MAX_BUF_SIZE);
  test_eq(buf_datalen(buf), 256-6-32);
  test_eq(eof, 1);

  close(s);

  /**** 
   * find_on_inbuf
   ****/
  buf_free(buf);
  buf = buf_new();
  s = open("/tmp/tor_test/data", O_RDONLY, 0);
  eof = 0;
  i = read_to_buf(s, 1024, buf, &eof); 
  test_eq(256, i);
  close(s);

  test_eq(((int)'d') + 1, find_on_inbuf("abcd", 4, buf));
  test_eq(-1, find_on_inbuf("xyzzy", 5, buf));
  /* Make sure we don't look off the end of the buffef */
  ((char*)_buf_peek_raw_buffer(buf))[256] = 'A';
  ((char*)_buf_peek_raw_buffer(buf))[257] = 'X';
  test_eq(-1, find_on_inbuf("\xff" "A", 2, buf));
  test_eq(-1, find_on_inbuf("AX", 2, buf));
  /* Make sure we use the string length */
  test_eq(((int)'d')+1, find_on_inbuf("abcdX", 4, buf));

  /****
   * fetch_from_buf
   ****/
  memset(str2, 255, 256);
  test_eq(246, fetch_from_buf(str2, 10, buf));
  test_memeq(str2, str, 10);
  test_memeq(str+10,(char*)_buf_peek_raw_buffer(buf),246);
  test_eq(buf_datalen(buf),246);

  test_eq(0, fetch_from_buf(str2, 246, buf));
  test_memeq(str2, str+10, 246);
  test_eq(buf_capacity(buf),MAX_BUF_SIZE);
  test_eq(buf_datalen(buf),0);

  /****
   * write_to_buf
   ****/
  memset((char *)_buf_peek_raw_buffer(buf), (int)'-', 256);
  i = write_to_buf("Hello world", 11, buf);
  test_eq(i, 11);
  test_eq(buf_datalen(buf), 11);
  test_memeq((char*)_buf_peek_raw_buffer(buf), "Hello world", 11);
  i = write_to_buf("XYZZY", 5, buf);
  test_eq(i, 16);
  test_eq(buf_datalen(buf), 16);
  test_memeq((char*)_buf_peek_raw_buffer(buf), "Hello worldXYZZY", 16);
  /* Test when buffer is overfull. */
#if 0
  buflen = 18;
  test_eq(-1, write_to_buf("This string will not fit.", 25, 
                           &buf, &buflen, &buf_datalen));
  test_eq(buf_datalen, 16);
  test_memeq(buf, "Hello worldXYZZY--", 18);
  buflen = MAX_BUF_SIZE;
#endif

  /****
   * flush_buf
   ****/
  /* XXXX Needs tests. */

  buf_free(buf);
}

void
test_crypto_dh()
{
  crypto_dh_env_t *dh1, *dh2;
  char p1[CRYPTO_DH_SIZE];
  char p2[CRYPTO_DH_SIZE];
  char s1[CRYPTO_DH_SIZE];
  char s2[CRYPTO_DH_SIZE];
  int s1len, s2len;

  dh1 = crypto_dh_new();
  dh2 = crypto_dh_new();
  test_eq(crypto_dh_get_bytes(dh1), CRYPTO_DH_SIZE);
  test_eq(crypto_dh_get_bytes(dh2), CRYPTO_DH_SIZE);

  memset(p1, 0, CRYPTO_DH_SIZE);
  memset(p2, 0, CRYPTO_DH_SIZE);
  test_memeq(p1, p2, CRYPTO_DH_SIZE);
  test_assert(! crypto_dh_get_public(dh1, p1, CRYPTO_DH_SIZE));
  test_memneq(p1, p2, CRYPTO_DH_SIZE);
  test_assert(! crypto_dh_get_public(dh2, p2, CRYPTO_DH_SIZE));
  test_memneq(p1, p2, CRYPTO_DH_SIZE);
  
  memset(s1, 0, CRYPTO_DH_SIZE);
  memset(s2, 0xFF, CRYPTO_DH_SIZE);
  s1len = crypto_dh_compute_secret(dh1, p2, CRYPTO_DH_SIZE, s1, 50);
  s2len = crypto_dh_compute_secret(dh2, p1, CRYPTO_DH_SIZE, s2, 50);
  test_assert(s1len > 0);
  test_eq(s1len, s2len);
  test_memeq(s1, s2, s1len);
  
  crypto_dh_free(dh1);
  crypto_dh_free(dh2);
}

void 
test_crypto() 
{
  crypto_cipher_env_t *env1, *env2;
  crypto_pk_env_t *pk1, *pk2;
  char *data1, *data2, *data3, *cp;
  FILE *f;
  int i, j;
  int str_ciphers[] = { CRYPTO_CIPHER_IDENTITY, 
                        CRYPTO_CIPHER_DES,
                        CRYPTO_CIPHER_RC4,
                        CRYPTO_CIPHER_3DES,
                        CRYPTO_CIPHER_AES_CTR,
                        -1 };

  data1 = tor_malloc(1024);
  data2 = tor_malloc(1024);
  data3 = tor_malloc(1024);
  test_assert(data1 && data2 && data3);

  /* Try out RNG. */
  test_assert(! crypto_seed_rng());
  crypto_rand(100, data1);
  crypto_rand(100, data2);
  test_memneq(data1,data2,100);
  
  /* Try out identity ciphers. */
  env1 = crypto_new_cipher_env(CRYPTO_CIPHER_IDENTITY);
  test_neq(env1, 0);
  test_eq(crypto_cipher_generate_key(env1), 0);
  test_eq(crypto_cipher_set_iv(env1, ""), 0);
  test_eq(crypto_cipher_encrypt_init_cipher(env1), 0);
  for(i = 0; i < 1024; ++i) {
    data1[i] = (char) i*73;
  }
  crypto_cipher_encrypt(env1, data1, 1024, data2); 
  test_memeq(data1, data2, 1024);
  crypto_free_cipher_env(env1);
  
  /* Now, test encryption and decryption with stream ciphers. */
  data1[0]='\0';
  for(i = 1023; i>0; i -= 35)
    strncat(data1, "Now is the time for all good onions", i);
  for(i=0; str_ciphers[i] >= 0; ++i) {
    /* For each cipher... */
    memset(data2, 0, 1024);
    memset(data3, 0, 1024);
    env1 = crypto_new_cipher_env(str_ciphers[i]);
    test_neq(env1, 0);
    env2 = crypto_new_cipher_env(str_ciphers[i]);
    test_neq(env2, 0);
    j = crypto_cipher_generate_key(env1);
    if (str_ciphers[i] != CRYPTO_CIPHER_IDENTITY) {
      crypto_cipher_set_key(env2, crypto_cipher_get_key(env1));
    }
    crypto_cipher_set_iv(env1, "12345678901234567890");
    crypto_cipher_set_iv(env2, "12345678901234567890");
    crypto_cipher_encrypt_init_cipher(env1);
    crypto_cipher_decrypt_init_cipher(env2);

    /* Try encrypting 512 chars. */
    crypto_cipher_encrypt(env1, data1, 512, data2);
    crypto_cipher_decrypt(env2, data2, 512, data3);
    test_memeq(data1, data3, 512);
    if (str_ciphers[i] == CRYPTO_CIPHER_IDENTITY) {
      test_memeq(data1, data2, 512);
    } else {
      test_memneq(data1, data2, 512);
    }
    /* Now encrypt 1 at a time, and get 1 at a time. */
    for (j = 512; j < 560; ++j) {
      crypto_cipher_encrypt(env1, data1+j, 1, data2+j);
    }
    for (j = 512; j < 560; ++j) {
      crypto_cipher_decrypt(env2, data2+j, 1, data3+j);
    }
    test_memeq(data1, data3, 560);
    /* Now encrypt 3 at a time, and get 5 at a time. */
    for (j = 560; j < 1024-5; j += 3) {
      crypto_cipher_encrypt(env1, data1+j, 3, data2+j);
    }
    for (j = 560; j < 1024-5; j += 5) {
      crypto_cipher_decrypt(env2, data2+j, 5, data3+j);
    }
    test_memeq(data1, data3, 1024-5);
    /* Now make sure that when we encrypt with different chunk sizes, we get
       the same results. */
    crypto_free_cipher_env(env2);

    memset(data3, 0, 1024);
    env2 = crypto_new_cipher_env(str_ciphers[i]);
    test_neq(env2, 0);
    if (str_ciphers[i] != CRYPTO_CIPHER_IDENTITY) {
      crypto_cipher_set_key(env2, crypto_cipher_get_key(env1));
    }
    crypto_cipher_set_iv(env2, "12345678901234567890");
    crypto_cipher_encrypt_init_cipher(env2);
    for (j = 0; j < 1024-16; j += 17) {
      crypto_cipher_encrypt(env2, data1+j, 17, data3+j);
    }
    for (j= 0; j < 1024-16; ++j) {
      if (data2[j] != data3[j]) {
        printf("%d:  %d\t%d\n", j, (int) data2[j], (int) data3[j]);
      }
    }
    test_memeq(data2, data3, 1024-16);
    crypto_free_cipher_env(env1);
    crypto_free_cipher_env(env2);
  }
  
  /* Test vectors for stream ciphers. */
  /* XXXX Look up some test vectors for the ciphers and make sure we match. */

  /* Test SHA-1 with a test vector from the specification. */
  i = crypto_SHA_digest("abc", 3, data1);
  test_memeq(data1,
             "\xA9\x99\x3E\x36\x47\x06\x81\x6A\xBA\x3E\x25\x71\x78"
             "\x50\xC2\x6C\x9C\xD0\xD8\x9D", 20);

  /* Public-key ciphers */
  pk1 = crypto_new_pk_env(CRYPTO_PK_RSA);
  pk2 = crypto_new_pk_env(CRYPTO_PK_RSA);
  test_assert(pk1 && pk2);
  test_assert(! crypto_pk_generate_key(pk1));
  test_assert(! crypto_pk_write_public_key_to_string(pk1, &cp, &i));
  test_assert(! crypto_pk_read_public_key_from_string(pk2, cp, i));
  test_eq(0, crypto_pk_cmp_keys(pk1, pk2));

  test_eq(128, crypto_pk_keysize(pk1));
  test_eq(128, crypto_pk_keysize(pk2));
  
  test_eq(128, crypto_pk_public_encrypt(pk2, "Hello whirled.", 15, data1,
                                        RSA_PKCS1_OAEP_PADDING));
  test_eq(128, crypto_pk_public_encrypt(pk1, "Hello whirled.", 15, data2,
                                        RSA_PKCS1_OAEP_PADDING));
  /* oaep padding should make encryption not match */
  test_memneq(data1, data2, 128);
  test_eq(15, crypto_pk_private_decrypt(pk1, data1, 128, data3,
                                        RSA_PKCS1_OAEP_PADDING));
  test_streq(data3, "Hello whirled.");
  memset(data3, 0, 1024);
  test_eq(15, crypto_pk_private_decrypt(pk1, data2, 128, data3,
                                        RSA_PKCS1_OAEP_PADDING));
  test_streq(data3, "Hello whirled.");
  /* Can't decrypt with public key. */
  test_eq(-1, crypto_pk_private_decrypt(pk2, data2, 128, data3,
                                        RSA_PKCS1_OAEP_PADDING));
  /* Try again with bad padding */
  memcpy(data2+1, "XYZZY", 5);  /* This has fails ~ once-in-2^40 */
  test_eq(-1, crypto_pk_private_decrypt(pk1, data2, 128, data3,
                                        RSA_PKCS1_OAEP_PADDING));
  
  /* File operations: save and load private key */
  f = fopen("/tmp/tor_test/pkey1", "wb");
  test_assert(! crypto_pk_write_private_key_to_file(pk1, f));
  fclose(f);
  f = fopen("/tmp/tor_test/pkey1", "rb");
  test_assert(! crypto_pk_read_private_key_from_file(pk2, f));
  fclose(f);
  test_eq(15, crypto_pk_private_decrypt(pk2, data1, 128, data3,
                                        RSA_PKCS1_OAEP_PADDING));
  test_assert(! crypto_pk_read_private_key_from_filename(pk2, 
                                               "/tmp/tor_test/pkey1"));
  test_eq(15, crypto_pk_private_decrypt(pk2, data1, 128, data3,
                                        RSA_PKCS1_OAEP_PADDING));

  /* Now try signing. */
  strcpy(data1, "Ossifrage");
  test_eq(128, crypto_pk_private_sign(pk1, data1, 10, data2));
  test_eq(10, crypto_pk_public_checksig(pk1, data2, 128, data3));
  test_streq(data3, "Ossifrage");
  /*XXXX test failed signing*/
    
  crypto_free_pk_env(pk1);  
  crypto_free_pk_env(pk2);  

  /* Base64 tests */
  strcpy(data1, "Test string that contains 35 chars.");
  strcat(data1, " 2nd string that contains 35 chars.");

  i = base64_encode(data2, 1024, data1, 71);
  j = base64_decode(data3, 1024, data2, i);
  test_streq(data3, data1);
  test_eq(j, 71);
  test_assert(data2[i] == '\0');


  free(data1);
  free(data2);
  free(data3);

}

void 
test_util() {
  struct timeval start, end;
  struct tm a_time;

  start.tv_sec = 5;
  start.tv_usec = 5000;

  end.tv_sec = 5;
  end.tv_usec = 5000;

  test_eq(0L, tv_udiff(&start, &end));

  end.tv_usec = 7000;

  test_eq(2000L, tv_udiff(&start, &end));

  end.tv_sec = 6;

  test_eq(1002000L, tv_udiff(&start, &end));

  end.tv_usec = 0;

  test_eq(995000L, tv_udiff(&start, &end));

  end.tv_sec = 4;

  test_eq(0L, tv_udiff(&start, &end));

  /* The test values here are confirmed to be correct on a platform
   * with a working timegm. */
  a_time.tm_year = 2003-1900;
  a_time.tm_mon = 7;
  a_time.tm_mday = 30;
  a_time.tm_hour = 6;
  a_time.tm_min = 14;
  a_time.tm_sec = 55;
  test_eq((time_t) 1062224095UL, tor_timegm(&a_time));
  a_time.tm_year = 2004-1900; /* Try a leap year, after feb. */
  test_eq((time_t) 1093846495UL, tor_timegm(&a_time));
  a_time.tm_mon = 1;          /* Try a leap year, in feb. */
  a_time.tm_mday = 10;
  test_eq((time_t) 1076393695UL, tor_timegm(&a_time));
}

void test_onion() {
#if 0
  char **names;
  int i,num;

  names = parse_nickname_list("  foo bar	 baz quux  ", &num);
  test_eq(num,4); 
  test_streq(names[0],"foo");
  test_streq(names[1],"bar");
  test_streq(names[2],"baz");
  test_streq(names[3],"quux");
  for(i=0;i<num;i++)
    tor_free(names[i]);
  tor_free(names);
#endif
}

void
test_onion_handshake() {
  /* client-side */
  crypto_dh_env_t *c_dh = NULL;
  char c_buf[DH_ONIONSKIN_LEN];
  char c_keys[40];

  /* server-side */
  char s_buf[DH_KEY_LEN];
  char s_keys[40];
  
  /* shared */
  crypto_pk_env_t *pk = NULL;

  pk = crypto_new_pk_env(CRYPTO_PK_RSA);
  test_assert(! crypto_pk_generate_key(pk));

  /* client handshake 1. */
  memset(c_buf, 0, DH_ONIONSKIN_LEN);
  test_assert(! onion_skin_create(pk, &c_dh, c_buf));

  /* server handshake */
  memset(s_buf, 0, DH_KEY_LEN);
  memset(s_keys, 0, 40);
  test_assert(! onion_skin_server_handshake(c_buf, pk, s_buf, s_keys, 40));
  
  /* client handshake 2 */
  memset(c_keys, 0, 40);
  test_assert(! onion_skin_client_handshake(c_dh, s_buf, c_keys, 40));
  
  crypto_dh_free(c_dh);

  if (memcmp(c_keys, s_keys, 40)) {
    puts("Aiiiie");
    exit(1);
  }
  test_memeq(c_keys, s_keys, 40);
  memset(s_buf, 0, 40);
  test_memneq(c_keys, s_buf, 40);
  crypto_free_pk_env(pk);
}

/* from routers.c */
int compare_recommended_versions(char *myversion, char *start);

void
test_dir_format()
{
  char buf[8192], buf2[8192];
  char *pk1_str = NULL, *pk2_str = NULL, *pk3_str = NULL, *cp;
  int pk1_str_len, pk2_str_len, pk3_str_len;
  routerinfo_t r1, r2;
  crypto_pk_env_t *pk1 = NULL, *pk2 = NULL, *pk3 = NULL;
  routerinfo_t *rp1 = NULL, *rp2 = NULL;
  struct exit_policy_t ex1, ex2;
  routerlist_t *dir1 = NULL, *dir2 = NULL;

  test_assert( (pk1 = crypto_new_pk_env(CRYPTO_PK_RSA)) );
  test_assert( (pk2 = crypto_new_pk_env(CRYPTO_PK_RSA)) );
  test_assert( (pk3 = crypto_new_pk_env(CRYPTO_PK_RSA)) );
  test_assert(! crypto_pk_generate_key(pk1));
  test_assert(! crypto_pk_generate_key(pk2));
  test_assert(! crypto_pk_generate_key(pk3));
  
  r1.address = "testaddr1.foo.bar";
  r1.addr = 0xc0a80001u; /* 192.168.0.1 */
  r1.published_on = 0;
  r1.or_port = 9000;
  r1.socks_port = 9002;
  r1.dir_port = 9003;
  r1.onion_pkey = pk1;
  r1.identity_pkey = pk2;
  r1.link_pkey = pk3;
  r1.bandwidth = 1000;
  r1.exit_policy = NULL;
  r1.nickname = "Magri";

  ex1.policy_type = EXIT_POLICY_ACCEPT;
  ex1.string = NULL;
  ex1.addr = 0;
  ex1.msk = 0;
  ex1.prt_min = ex1.prt_max = 80;
  ex1.next = &ex2;
  ex2.policy_type = EXIT_POLICY_REJECT;
  ex2.addr = 18 << 24;
  ex2.msk = 0xFF000000u;
  ex2.prt_min = ex1.prt_max = 24;
  ex2.next = NULL;
  r2.address = "tor.tor.tor";
  r2.addr = 0x0a030201u; /* 10.3.2.1 */
  r2.published_on = 5;
  r2.or_port = 9005;
  r2.socks_port = 0;
  r2.dir_port = 0;
  r2.onion_pkey = pk2;
  r2.identity_pkey = pk1;
  r2.link_pkey = pk2;
  r2.bandwidth = 3000;
  r2.exit_policy = &ex1;

  test_assert(!crypto_pk_write_public_key_to_string(pk1, &pk1_str, 
                                                    &pk1_str_len));
  test_assert(!crypto_pk_write_public_key_to_string(pk2 , &pk2_str, 
                                                    &pk2_str_len));
  test_assert(!crypto_pk_write_public_key_to_string(pk3 , &pk3_str, 
                                                    &pk3_str_len));

  memset(buf, 0, 2048);
  log_set_severity(LOG_WARN);
  test_assert(router_dump_router_to_string(buf, 2048, &r1, pk2)>0);
  
  strcpy(buf2, "router Magri testaddr1.foo.bar 9000 9002 9003 1000\n"
         "platform Tor "VERSION" on ");
  strcat(buf2, get_uname());
  strcat(buf2, "\n"
         "published 1970-01-01 00:00:00\n"
         "onion-key\n");
  strcat(buf2, pk1_str);
  strcat(buf2, "link-key\n");
  strcat(buf2, pk3_str);
  strcat(buf2, "signing-key\n");
  strcat(buf2, pk2_str);
  strcat(buf2, "router-signature\n");
  buf[strlen(buf2)] = '\0'; /* Don't compare the sig; it's never the same twice*/
  
  test_streq(buf, buf2);
  
  test_assert(router_dump_router_to_string(buf, 2048, &r1, pk2)>0);
  cp = buf;
  rp1 = router_get_entry_from_string((const char**)&cp);
  test_assert(rp1);
  test_streq(rp1->address, r1.address);
  test_eq(rp1->or_port, r1.or_port);
  test_eq(rp1->socks_port, r1.socks_port);
  test_eq(rp1->dir_port, r1.dir_port);
  test_eq(rp1->bandwidth, r1.bandwidth);
  test_assert(crypto_pk_cmp_keys(rp1->onion_pkey, pk1) == 0);
  test_assert(crypto_pk_cmp_keys(rp1->link_pkey, pk3) == 0);
  test_assert(crypto_pk_cmp_keys(rp1->identity_pkey, pk2) == 0);
  test_assert(rp1->exit_policy == NULL);

#if 0 
  /* XXX Once we have exit policies, test this again. XXX */
  strcpy(buf2, "router tor.tor.tor 9005 0 0 3000\n");
  strcat(buf2, pk2_str);
  strcat(buf2, "signing-key\n");
  strcat(buf2, pk1_str);
  strcat(buf2, "accept *:80\nreject 18.*:24\n\n");
  test_assert(router_dump_router_to_string(buf, 2048, &r2, pk2)>0);
  test_streq(buf, buf2);

  cp = buf;
  rp2 = router_get_entry_from_string(&cp);
  test_assert(rp2);
  test_streq(rp2->address, r2.address);
  test_eq(rp2->or_port, r2.or_port);
  test_eq(rp2->socks_port, r2.socks_port);
  test_eq(rp2->dir_port, r2.dir_port);
  test_eq(rp2->bandwidth, r2.bandwidth);
  test_assert(crypto_pk_cmp_keys(rp2->onion_pkey, pk2) == 0);
  test_assert(crypto_pk_cmp_keys(rp2->identity_pkey, pk1) == 0);
  test_eq(rp2->exit_policy->policy_type, EXIT_POLICY_ACCEPT);
  test_streq(rp2->exit_policy->string, "accept *:80");
  test_streq(rp2->exit_policy->address, "*");
  test_streq(rp2->exit_policy->port, "80");
  test_eq(rp2->exit_policy->next->policy_type, EXIT_POLICY_REJECT);
  test_streq(rp2->exit_policy->next->string, "reject 18.*:24");
  test_streq(rp2->exit_policy->next->address, "18.*");
  test_streq(rp2->exit_policy->next->port, "24");
  test_assert(rp2->exit_policy->next->next == NULL);
#endif

#if 0 
  /* XXX To re-enable this test, we need to separate directory generation
   * XXX from the directory backend again.  Do this the next time we have
   * XXX directory trouble. */
  /* Okay, now for the directories. */
  dir1 = (directory_t*) tor_malloc(sizeof(directory_t));
  dir1->n_routers = 2;
  dir1->routers = (routerinfo_t**) tor_malloc(sizeof(routerinfo_t*)*2);
  dir1->routers[0] = &r1;
  dir1->routers[1] = &r2;
  test_assert(! dump_signed_directory_to_string_impl(buf, 4096, dir1, pk1));
  /* puts(buf); */
  
  test_assert(! router_get_dir_from_string_impl(buf, &dir2, pk1));
  test_eq(2, dir2->n_routers);
#endif
 
  tor_free(pk1_str);
  tor_free(pk2_str);
  if (pk1) crypto_free_pk_env(pk1);
  if (pk2) crypto_free_pk_env(pk2);
  if (rp1) routerinfo_free(rp1);
  if (rp2) routerinfo_free(rp2);
  tor_free(dir1); /* And more !*/
  tor_free(dir2); /* And more !*/

  /* make sure compare_recommended_versions() works */
  test_eq(0, compare_recommended_versions("abc", "abc"));
  test_eq(0, compare_recommended_versions("abc", "ab,abd,abde,abc,abcde"));
  test_eq(0, compare_recommended_versions("abc", "ab,abd,abde,abcde,abc"));
  test_eq(0, compare_recommended_versions("abc", "abc,abd,abde,abc,abcde"));
  test_eq(0, compare_recommended_versions("a", "a,ab,abd,abde,abc,abcde"));
  test_eq(-1, compare_recommended_versions("a", "ab,abd,abde,abc,abcde"));
  test_eq(-1, compare_recommended_versions("abb", "ab,abd,abde,abc,abcde"));
  test_eq(-1, compare_recommended_versions("a", ""));
}

int 
main(int c, char**v){ 
#if 0
  or_options_t options; /* command-line and config-file options */

  if(getconfig(c,v,&options))
    exit(1);
#endif
  log_set_severity(LOG_ERR);         /* make logging quieter */

  crypto_seed_rng();

  setup_directory();
  puts("========================== Buffers =========================");
  test_buffers();
  puts("\n========================== Crypto ==========================");
  test_crypto();
  test_crypto_dh();
  puts("\n========================= Util ============================");
  test_util();
  puts("\n========================= Onion Skins =====================");
  test_onion();
  test_onion_handshake();
  puts("\n========================= Directory Formats ===============");
  test_dir_format();
  puts("");
  return 0;
}

/*
  Local Variables:
  mode:c
  indent-tabs-mode:nil
  c-basic-offset:2
  End:
*/
