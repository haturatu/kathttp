#include "dns.h"

#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>

#include "log.h"

namespace kathttp {

std::vector<ResolvedEndpoint> GetAddrInfoResolver::resolve(
    const std::string &host, uint16_t port) {
  std::vector<ResolvedEndpoint> out;
  addrinfo hints{};
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_DGRAM;
  hints.ai_protocol = IPPROTO_UDP;

  std::string port_str = std::to_string(port);
  addrinfo *res = nullptr;
  if (getaddrinfo(host.c_str(), port_str.c_str(), &hints, &res) != 0) {
    KATHTTP_LOG_ERR("getaddrinfo failed for %s:%u\n", host.c_str(), port);
    return out;
  }
  for (addrinfo *rp = res; rp; rp = rp->ai_next) {
    char buf[INET6_ADDRSTRLEN] = {0};
    if (rp->ai_family == AF_INET) {
      auto *sa = reinterpret_cast<sockaddr_in *>(rp->ai_addr);
      inet_ntop(AF_INET, &sa->sin_addr, buf, sizeof(buf));
    } else if (rp->ai_family == AF_INET6) {
      auto *sa = reinterpret_cast<sockaddr_in6 *>(rp->ai_addr);
      inet_ntop(AF_INET6, &sa->sin6_addr, buf, sizeof(buf));
    } else {
      continue;
    }
    out.push_back(ResolvedEndpoint{std::string(buf), port, rp->ai_family});
  }
  freeaddrinfo(res);
  return out;
}

} /* namespace kathttp */
