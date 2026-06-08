#include "x509.h"
#include "rsa.h"
#include "ecdsa.h"
#include "bignum_4k.h"
#include "rsa_pss.h"

static const uint8_t OID_RSA_PSS[] = { 0x2A,0x86,0x48,0x86,0xF7,0x0D,0x01,0x01,0x0A };

#define TAG_BOOLEAN        0x01
#define TAG_INTEGER        0x02
#define TAG_BITSTRING      0x03
#define TAG_OCTETSTRING    0x04
#define TAG_NULL           0x05
#define TAG_OID            0x06
#define TAG_UTF8STRING     0x0c
#define TAG_SEQUENCE       0x30
#define TAG_SET            0x31
#define TAG_PRINTSTRING    0x13
#define TAG_IA5STRING      0x16
#define TAG_UTCTIME        0x17
#define TAG_GENTIME        0x18
#define TAG_CTX_CONSTRUCT  0xA0
#define TAG_CTX3           0xA3

typedef struct {
    const uint8_t* p;
    const uint8_t* end;
} der_t;

static int der_tlv(der_t* d, uint8_t* tag, const uint8_t** val, uint32_t* vlen) {
    if (d->p >= d->end) return 0;
    *tag = *d->p++;
    if (d->p >= d->end) return 0;
    uint32_t len = *d->p++;
    if (len & 0x80) {
        uint32_t nl = len & 0x7F;
        if (nl == 0 || nl > 4) return 0;
        if (d->p + nl > d->end) return 0;
        len = 0;
        for (uint32_t i = 0; i < nl; i++) len = (len << 8) | *d->p++;
    }
    if (d->p + len > d->end) return 0;
    *val  = d->p;
    *vlen = len;
    d->p += len;
    return 1;
}

static int der_expect(der_t* d, uint8_t expected, const uint8_t** val, uint32_t* vlen) {
    uint8_t t;
    if (!der_tlv(d, &t, val, vlen)) return 0;
    return t == expected;
}

static int bytes_eq(const uint8_t* a, const uint8_t* b, uint32_t n) {
    for (uint32_t i = 0; i < n; i++) if (a[i] != b[i]) return 0;
    return 1;
}

static const uint8_t OID_CN[]              = { 0x55, 0x04, 0x03 };
static const uint8_t OID_SAN[]             = { 0x55, 0x1D, 0x11 };
static const uint8_t OID_RSA[]             = { 0x2A,0x86,0x48,0x86,0xF7,0x0D,0x01,0x01,0x01 };
static const uint8_t OID_SHA256_RSA[]      = { 0x2A,0x86,0x48,0x86,0xF7,0x0D,0x01,0x01,0x0B };
static const uint8_t OID_SHA384_RSA[]      = { 0x2A,0x86,0x48,0x86,0xF7,0x0D,0x01,0x01,0x0C };
static const uint8_t OID_SHA512_RSA[]      = { 0x2A,0x86,0x48,0x86,0xF7,0x0D,0x01,0x01,0x0D };
static const uint8_t OID_EC[]              = { 0x2A,0x86,0x48,0xCE,0x3D,0x02,0x01 };
static const uint8_t OID_ECDSA_SHA256[]    = { 0x2A,0x86,0x48,0xCE,0x3D,0x04,0x03,0x02 };
static const uint8_t OID_ECDSA_SHA384[]    = { 0x2A,0x86,0x48,0xCE,0x3D,0x04,0x03,0x03 };

static int oid_eq(const uint8_t* a, uint32_t alen, const uint8_t* b, uint32_t blen) {
    return alen == blen && bytes_eq(a, b, alen);
}

static uint8_t map_sig_alg(const uint8_t* oid, uint32_t len) {
    if (oid_eq(oid, len, OID_SHA256_RSA,   sizeof(OID_SHA256_RSA)))   return X509_SIG_SHA256_RSA;
    if (oid_eq(oid, len, OID_SHA384_RSA,   sizeof(OID_SHA384_RSA)))   return X509_SIG_SHA384_RSA;
    if (oid_eq(oid, len, OID_SHA512_RSA,   sizeof(OID_SHA512_RSA)))   return X509_SIG_SHA512_RSA;
    if (oid_eq(oid, len, OID_ECDSA_SHA256, sizeof(OID_ECDSA_SHA256))) return X509_SIG_ECDSA_SHA256;
    if (oid_eq(oid, len, OID_ECDSA_SHA384, sizeof(OID_ECDSA_SHA384))) return X509_SIG_ECDSA_SHA384;
    if (oid_eq(oid, len, OID_RSA_PSS,      sizeof(OID_RSA_PSS)))      return X509_SIG_RSA_PSS_SHA256;
    return X509_SIG_UNKNOWN;
}

static uint8_t map_pkey_alg(const uint8_t* oid, uint32_t len) {
    if (oid_eq(oid, len, OID_RSA, sizeof(OID_RSA))) return X509_PKEY_RSA;
    if (oid_eq(oid, len, OID_EC,  sizeof(OID_EC)))  return X509_PKEY_EC;
    return X509_PKEY_UNKNOWN;
}

static void extract_cn_from_name(const uint8_t* name, uint32_t name_len, char* out, uint32_t out_max) {
    out[0] = 0;
    der_t d = { name, name + name_len };
    while (d.p < d.end) {
        uint8_t t; const uint8_t* rdn; uint32_t rdn_len;
        if (!der_tlv(&d, &t, &rdn, &rdn_len) || t != TAG_SET) return;
        der_t rs = { rdn, rdn + rdn_len };
        while (rs.p < rs.end) {
            const uint8_t* atv; uint32_t atv_len;
            if (!der_expect(&rs, TAG_SEQUENCE, &atv, &atv_len)) return;
            der_t a = { atv, atv + atv_len };
            const uint8_t* oid; uint32_t oid_len;
            if (!der_expect(&a, TAG_OID, &oid, &oid_len)) continue;
            const uint8_t* val; uint32_t val_len;
            uint8_t vt;
            if (!der_tlv(&a, &vt, &val, &val_len)) continue;
            if (oid_eq(oid, oid_len, OID_CN, sizeof(OID_CN))) {
                if (vt == TAG_UTF8STRING || vt == TAG_PRINTSTRING ||
                    vt == TAG_IA5STRING) {
                    uint32_t n = val_len < out_max - 1 ? val_len : out_max - 1;
                    for (uint32_t i = 0; i < n; i++) out[i] = (char)val[i];
                    out[n] = 0;
                    return;
                }
            }
        }
    }
}

static void extract_san(const uint8_t* ext_seq, uint32_t ext_len, x509_cert_t* out) {
    out->san_count = 0;
    der_t d = { ext_seq, ext_seq + ext_len };
    while (d.p < d.end) {
        const uint8_t* ext; uint32_t ext_clen;
        if (!der_expect(&d, TAG_SEQUENCE, &ext, &ext_clen)) return;
        der_t e = { ext, ext + ext_clen };
        const uint8_t* oid; uint32_t oid_len;
        if (!der_expect(&e, TAG_OID, &oid, &oid_len)) continue;


        if (e.p < e.end && *e.p == TAG_BOOLEAN) {
            uint8_t t; const uint8_t* v; uint32_t vl;
            der_tlv(&e, &t, &v, &vl);
        }
        const uint8_t* ext_val; uint32_t ext_val_len;
        if (!der_expect(&e, TAG_OCTETSTRING, &ext_val, &ext_val_len)) continue;
        if (!oid_eq(oid, oid_len, OID_SAN, sizeof(OID_SAN))) continue;


        der_t inner_w = { ext_val, ext_val + ext_val_len };
        const uint8_t* san_seq; uint32_t san_seq_len;
        if (!der_expect(&inner_w, TAG_SEQUENCE, &san_seq, &san_seq_len)) return;
        der_t names = { san_seq, san_seq + san_seq_len };
        while (names.p < names.end && out->san_count < X509_SAN_MAX) {
            uint8_t t; const uint8_t* gv; uint32_t gv_len;
            if (!der_tlv(&names, &t, &gv, &gv_len)) return;

            if (t == 0x82) {
                uint32_t n = gv_len < X509_SAN_LEN_MAX - 1 ? gv_len : X509_SAN_LEN_MAX - 1;
                for (uint32_t i = 0; i < n; i++) out->san[out->san_count][i] = (char)gv[i];
                out->san[out->san_count][n] = 0;
                out->san_count++;
            }
        }
        return;
    }
}

static void copy_time(const uint8_t* src, uint32_t len, char* dst) {
    uint32_t n = len < X509_TIME_MAX - 1 ? len : X509_TIME_MAX - 1;
    for (uint32_t i = 0; i < n; i++) dst[i] = (char)src[i];
    dst[n] = 0;
}

int x509_parse(const uint8_t* der, uint32_t len, x509_cert_t* out) {
    if (!der || !out || len < 8) return 0;

    for (uint32_t i = 0; i < sizeof(*out); i++) ((uint8_t*)out)[i] = 0;

    der_t d = { der, der + len };


    const uint8_t* cert; uint32_t cert_len;
    if (!der_expect(&d, TAG_SEQUENCE, &cert, &cert_len)) return 0;
    der_t c = { cert, cert + cert_len };



    const uint8_t* tbs_after_header_p = c.p;
    const uint8_t* tbs; uint32_t tbs_len;
    if (!der_expect(&c, TAG_SEQUENCE, &tbs, &tbs_len)) return 0;
    out->tbs_off = (uint32_t)(tbs_after_header_p - der);
    out->tbs_len = (uint32_t)((tbs + tbs_len) - tbs_after_header_p);
    der_t t = { tbs, tbs + tbs_len };


    if (t.p < t.end && *t.p == TAG_CTX_CONSTRUCT) {
        uint8_t tg; const uint8_t* v; uint32_t vl;
        if (!der_tlv(&t, &tg, &v, &vl)) return 0;
    }

    { uint8_t tg; const uint8_t* v; uint32_t vl;
      if (!der_tlv(&t, &tg, &v, &vl) || tg != TAG_INTEGER) return 0; }


    { const uint8_t* alg; uint32_t alg_len;
      if (!der_expect(&t, TAG_SEQUENCE, &alg, &alg_len)) return 0; }


    { const uint8_t* name; uint32_t nl;
      if (!der_expect(&t, TAG_SEQUENCE, &name, &nl)) return 0;
      extract_cn_from_name(name, nl, out->issuer_cn, X509_CN_MAX); }


    { const uint8_t* val; uint32_t vlen;
      if (!der_expect(&t, TAG_SEQUENCE, &val, &vlen)) return 0;
      der_t v = { val, val + vlen };
      uint8_t tg; const uint8_t* tv; uint32_t tlen;
      if (!der_tlv(&v, &tg, &tv, &tlen)) return 0;
      if (tg != TAG_UTCTIME && tg != TAG_GENTIME) return 0;
      copy_time(tv, tlen, out->not_before);
      if (!der_tlv(&v, &tg, &tv, &tlen)) return 0;
      if (tg != TAG_UTCTIME && tg != TAG_GENTIME) return 0;
      copy_time(tv, tlen, out->not_after);
    }


    { const uint8_t* name; uint32_t nl;
      if (!der_expect(&t, TAG_SEQUENCE, &name, &nl)) return 0;
      extract_cn_from_name(name, nl, out->subject_cn, X509_CN_MAX); }


    { const uint8_t* spki; uint32_t spki_len;
      if (!der_expect(&t, TAG_SEQUENCE, &spki, &spki_len)) return 0;
      der_t s = { spki, spki + spki_len };
      const uint8_t* alg; uint32_t alg_len;
      if (!der_expect(&s, TAG_SEQUENCE, &alg, &alg_len)) return 0;
      der_t a = { alg, alg + alg_len };
      const uint8_t* oid; uint32_t oid_len;
      if (!der_expect(&a, TAG_OID, &oid, &oid_len)) return 0;
      out->pkey_alg = map_pkey_alg(oid, oid_len);


      const uint8_t* bs; uint32_t bs_len;
      if (der_expect(&s, TAG_BITSTRING, &bs, &bs_len) && bs_len > 1 && bs[0] == 0
          && out->pkey_alg == X509_PKEY_RSA) {


          der_t k = { bs + 1, bs + bs_len };
          const uint8_t* rsa; uint32_t rsa_len;
          if (der_expect(&k, TAG_SEQUENCE, &rsa, &rsa_len)) {
              der_t r = { rsa, rsa + rsa_len };
              const uint8_t* nv; uint32_t nl;
              const uint8_t* ev; uint32_t el;
              if (der_expect(&r, TAG_INTEGER, &nv, &nl) &&
                  der_expect(&r, TAG_INTEGER, &ev, &el)) {

                  if (nl > 0 && nv[0] == 0x00) { nv++; nl--; }
                  if (el > 0 && ev[0] == 0x00) { ev++; el--; }
                  if (nl > X509_PUBKEY_N_MAX) nl = X509_PUBKEY_N_MAX;
                  if (el > X509_PUBKEY_E_MAX) el = X509_PUBKEY_E_MAX;
                  for (uint32_t i = 0; i < nl; i++) out->pubkey_n[i] = nv[i];
                  out->pubkey_n_len = nl;
                  for (uint32_t i = 0; i < el; i++) out->pubkey_e[i] = ev[i];
                  out->pubkey_e_len = el;
              }
          }
      } else if (der_expect(&s, TAG_BITSTRING, &bs, &bs_len) && bs_len >= 1
                 && bs[0] == 0 && out->pkey_alg == X509_PKEY_EC) {

          if (bs_len == 1 + 1 + 64 && bs[1] == 0x04) {
              for (int i = 0; i < 32; i++) out->pubkey_ec_x[i] = bs[2 + i];
              for (int i = 0; i < 32; i++) out->pubkey_ec_y[i] = bs[2 + 32 + i];
              out->pubkey_ec_valid = 1;
          }
      }
    }


    while (t.p < t.end) {
        if (*t.p == TAG_CTX3) {
            uint8_t tg; const uint8_t* ext_outer; uint32_t ext_outer_len;
            if (!der_tlv(&t, &tg, &ext_outer, &ext_outer_len)) return 0;

            der_t w = { ext_outer, ext_outer + ext_outer_len };
            const uint8_t* exts; uint32_t exts_len;
            if (!der_expect(&w, TAG_SEQUENCE, &exts, &exts_len)) return 0;
            extract_san(exts, exts_len, out);
            break;
        } else {
            uint8_t tg; const uint8_t* v; uint32_t vl;
            if (!der_tlv(&t, &tg, &v, &vl)) return 0;
        }
    }


    { const uint8_t* alg; uint32_t alg_len;
      if (!der_expect(&c, TAG_SEQUENCE, &alg, &alg_len)) return 0;
      der_t a = { alg, alg + alg_len };
      const uint8_t* oid; uint32_t oid_len;
      if (!der_expect(&a, TAG_OID, &oid, &oid_len)) return 0;
      out->sig_alg = map_sig_alg(oid, oid_len);
    }


    { const uint8_t* bs; uint32_t bs_len;
      if (der_expect(&c, TAG_BITSTRING, &bs, &bs_len) && bs_len > 1 && bs[0] == 0) {
          out->sig_off = (uint32_t)((bs + 1) - der);
          out->sig_len = bs_len - 1;
      }
    }
    return 1;
}

int x509_verify_signature(const uint8_t* child_der, uint32_t child_der_len,
                          const x509_cert_t* child, const x509_cert_t* parent) {
    if (!child_der || !child || !parent) return 0;
    if (child->tbs_off + child->tbs_len > child_der_len) return 0;
    if (child->sig_off + child->sig_len > child_der_len) return 0;
    const uint8_t* sig = child_der + child->sig_off;
    uint32_t sig_l    = child->sig_len;
    const uint8_t* tbs = child_der + child->tbs_off;
    uint32_t tbs_l    = child->tbs_len;
    if (child->sig_alg == X509_SIG_SHA256_RSA &&
        parent->pkey_alg == X509_PKEY_RSA && parent->pubkey_n_len > 0) {


        if (parent->pubkey_n_len <= 256) {
            return rsa_pkcs1_v15_sha256_verify(
                parent->pubkey_n, parent->pubkey_n_len,
                parent->pubkey_e, parent->pubkey_e_len,
                sig, sig_l, tbs, tbs_l);
        } else {
            return rsa4k_pkcs1_v15_sha256_verify(
                parent->pubkey_n, parent->pubkey_n_len,
                parent->pubkey_e, parent->pubkey_e_len,
                sig, sig_l, tbs, tbs_l);
        }
    }
    if (child->sig_alg == X509_SIG_RSA_PSS_SHA256 &&
        parent->pkey_alg == X509_PKEY_RSA && parent->pubkey_n_len > 0) {
        return rsa_pss_sha256_verify(
            parent->pubkey_n, parent->pubkey_n_len,
            parent->pubkey_e, parent->pubkey_e_len,
            sig, sig_l, tbs, tbs_l);
    }
    if (child->sig_alg == X509_SIG_ECDSA_SHA256 &&
        parent->pkey_alg == X509_PKEY_EC && parent->pubkey_ec_valid) {
        return ecdsa_p256_sha256_verify(
            parent->pubkey_ec_x, parent->pubkey_ec_y,
            sig, sig_l, tbs, tbs_l);
    }
    return 0;
}

extern const uint8_t x509_testvec_der[406];

static int str_eq(const char* a, const char* b) {
    while (*a && *b) { if (*a != *b) return 0; a++; b++; }
    return *a == *b;
}

int x509_selftest(void) {
    x509_cert_t c;
    if (!x509_parse(x509_testvec_der, 406, &c)) return 0;
    if (!str_eq(c.subject_cn, "vyro.test")) return 0;
    if (!str_eq(c.issuer_cn,  "vyro.test")) return 0;
    if (c.sig_alg  != X509_SIG_ECDSA_SHA256) return 0;
    if (c.pkey_alg != X509_PKEY_EC)          return 0;
    if (c.san_count < 1)                     return 0;
    if (!str_eq(c.san[0], "vyro.test"))      return 0;
    return 1;
}

const char* x509_sig_alg_name(uint8_t e) {
    switch (e) {
    case X509_SIG_SHA256_RSA:   return "sha256WithRSAEncryption";
    case X509_SIG_SHA384_RSA:   return "sha384WithRSAEncryption";
    case X509_SIG_SHA512_RSA:   return "sha512WithRSAEncryption";
    case X509_SIG_ECDSA_SHA256: return "ecdsa-with-SHA256";
    case X509_SIG_ECDSA_SHA384: return "ecdsa-with-SHA384";
    case X509_SIG_RSA_PSS_SHA256: return "rsa_pss_rsae_sha256";
    default:                    return "(unknown)";
    }
}

const char* x509_pkey_alg_name(uint8_t e) {
    switch (e) {
    case X509_PKEY_RSA: return "rsaEncryption";
    case X509_PKEY_EC:  return "ecPublicKey";
    default:            return "(unknown)";
    }
}
