#ifndef KATHTPP_TLS_H
#define KATHTPP_TLS_H

#include <ngtcp2/ngtcp2.h>
#include <ngtcp2/ngtcp2_crypto.h>
#include <openssl/ssl.h>

#include <string>

namespace kathttp {

/* BoringSSL TLS 1.3 context for QUIC clients. Wraps an SSL_CTX configured
 * with ngtcp2_crypto_boringssl helpers. */
class TlsClientContext {
public:
  TlsClientContext() = default;
  ~TlsClientContext();

  TlsClientContext(const TlsClientContext &) = delete;
  TlsClientContext &operator=(const TlsClientContext &) = delete;

  bool init(bool verify_cert, const std::string &ca_cert_file,
            const std::string &keylog_file);

  SSL_CTX *native() const { return ssl_ctx_; }

private:
  SSL_CTX *ssl_ctx_ = nullptr;
};

/* A single QUIC connection's TLS session (one SSL object). */
class TlsClientSession {
public:
  TlsClientSession() = default;
  ~TlsClientSession();

  TlsClientSession(const TlsClientSession &) = delete;
  TlsClientSession &operator=(const TlsClientSession &) = delete;

  /* `conn_ref` must outlive the session; it is used by the ngtcp2_crypto
   * callbacks to recover the ngtcp2_conn. */
  bool init(TlsClientContext &ctx, const std::string &server_name,
            bool enable_early_data, ngtcp2_crypto_conn_ref *conn_ref);

  SSL *native() const { return ssl_; }

private:
  SSL *ssl_ = nullptr;
};

} /* namespace kathttp */

#endif /* KATHTPP_TLS_H */
