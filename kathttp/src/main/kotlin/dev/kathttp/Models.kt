package dev.kathttp

data class KatHttpClientConfig(
    val connectTimeoutMillis: Long = 10_000,
    val requestTimeoutMillis: Long = 30_000,
    val idleTimeoutMillis: Long = 30_000,
    val followRedirects: Boolean = true,
    val maxRedirects: Int = 10,
    val maxBufferedBodyBytes: Long = 16L * 1024 * 1024,
    val caCertificateFile: String? = null,
) {
    init { require(connectTimeoutMillis > 0); require(requestTimeoutMillis > 0); require(idleTimeoutMillis > 0); require(maxRedirects >= 0); require(maxBufferedBodyBytes > 0); require(caCertificateFile == null || caCertificateFile.isNotBlank()) }
}

data class KatHttpHeader(val name: String, val value: String) {
    init { require(name.isNotBlank() && name == name.lowercase() && name.none { it <= ' ' || it == ':' }); require(value.none { it == '\r' || it == '\n' }) }
}

data class KatHttpRequest(val method: String, val url: String, val headers: List<KatHttpHeader> = emptyList(), val body: ByteArray? = null) {
    init { require(method.matches(Regex("[A-Z!#$%&'*+.^_`|~-]+"))); require(url.startsWith("https://")) }
}

data class KatHttpResponse(val status: Int, val headers: List<KatHttpHeader>, val body: ByteArray, val protocol: String = "h3")

sealed class KatHttpException(message: String) : Exception(message) {
    class Dns : KatHttpException("DNS resolution failed")
    class Tls : KatHttpException("TLS handshake or certificate verification failed")
    class Timeout : KatHttpException("Request timed out")
    class Closed : KatHttpException("Client is closed")
    class BodyTooLarge : KatHttpException("Response exceeds configured body limit")
    class Native(val code: Int) : KatHttpException("Native HTTP/3 error: $code")
}
