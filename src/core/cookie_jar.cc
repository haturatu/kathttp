#include "cookie_jar.h"

#include <algorithm>
#include <ctime>
#include <string>

namespace kathttp {

static std::string to_lower(std::string_view s) {
  std::string o(s);
  std::transform(o.begin(), o.end(), o.begin(),
                 [](unsigned char c) { return std::tolower(c); });
  return o;
}

static std::string_view trim(std::string_view s) {
  size_t b = s.find_first_not_of(" \t");
  if (b == std::string_view::npos) return "";
  size_t e = s.find_last_not_of(" \t");
  return s.substr(b, e - b + 1);
}

static bool parse_uint(std::string_view s, uint64_t &out) {
  if (s.empty()) return false;
  uint64_t v = 0;
  for (char c : s) {
    if (c < '0' || c > '9') return false;
    v = v * 10 + (c - '0');
  }
  out = v;
  return true;
}

bool CookieJar::domain_matches(std::string_view cookie_domain,
                               bool host_only, std::string_view host) const {
  std::string h = to_lower(host);
  std::string cd = to_lower(cookie_domain);
  if (cd.empty()) return false;
  if (host_only) return h == cd;
  if (cd.front() == '.') cd.erase(0, 1);
  if (h == cd) return true;
  return h.size() > cd.size() && h[h.size() - cd.size() - 1] == '.' &&
         h.compare(h.size() - cd.size(), cd.size(), cd) == 0;
}

bool CookieJar::path_matches(std::string_view cookie_path,
                             std::string_view req_path) const {
  if (cookie_path.empty()) return true;
  if (req_path.size() < cookie_path.size()) return false;
  if (!std::equal(cookie_path.begin(), cookie_path.end(), req_path.begin()))
    return false;
  return cookie_path.back() == '/' || req_path.size() == cookie_path.size() ||
         req_path[cookie_path.size()] == '/';
}

void CookieJar::store(const Url &url, const HeaderList &headers) {
  for (auto v : headers.get_all("set-cookie")) store(url, v);
}

void CookieJar::store(const Url &url, std::string_view set_cookie) {
  auto semi = set_cookie.find(';');
  std::string_view first = trim(set_cookie.substr(0, semi));
  auto eq = first.find('=');
  if (eq == std::string_view::npos) return;
  Cookie c;
  c.name = std::string(trim(first.substr(0, eq)));
  c.value = std::string(trim(first.substr(eq + 1)));

  std::string_view rest =
      semi == std::string_view::npos ? "" : set_cookie.substr(semi + 1);
  while (!rest.empty()) {
    auto sc = rest.find(';');
    std::string_view attr = trim(rest.substr(0, sc));
    auto aeq = attr.find('=');
    std::string_view an = trim(attr.substr(0, aeq));
    std::string_view av =
        aeq == std::string_view::npos ? "" : trim(attr.substr(aeq + 1));
    std::string anl = to_lower(an);
    if (anl == "domain") {
      c.domain = std::string(trim(av));
      if (!c.domain.empty() && c.domain.front() == '.') c.domain.erase(0, 1);
      c.host_only = false;
    } else if (anl == "path") {
      c.path = av.empty() ? "/" : std::string(av);
    } else if (anl == "secure") {
      c.secure = true;
    } else if (anl == "httponly") {
      c.http_only = true;
    } else if (anl == "max-age") {
      uint64_t secs = 0;
      if (parse_uint(trim(av), secs)) {
        c.persistent = true;
        c.expiry = static_cast<uint64_t>(std::time(nullptr)) + secs;
      }
    } else if (anl == "expires") {
      // Best-effort: rely on max-age if present; otherwise store as
      // persistent and let it expire at process end (not parsing HTTP date
      // fully here).
      c.persistent = true;
    }
    if (sc == std::string_view::npos) break;
    rest = rest.substr(sc + 1);
  }

  if (c.domain.empty()) {
    c.domain = url.host;
    c.host_only = true;
  }
  if (!domain_matches(c.domain, c.host_only, url.host)) return;

  std::lock_guard<std::mutex> lk(mu_);
  // Replace any existing cookie with same name+domain+path.
  for (auto &existing : cookies_) {
    if (existing.name == c.name && existing.domain == c.domain &&
        existing.path == c.path) {
      existing = std::move(c);
      return;
    }
  }
  cookies_.push_back(std::move(c));
}

std::string CookieJar::cookie_header(const Url &url) {
  std::lock_guard<std::mutex> lk(mu_);
  uint64_t now = static_cast<uint64_t>(std::time(nullptr));
  std::vector<std::string> pairs;
  for (auto it = cookies_.begin(); it != cookies_.end();) {
    if (it->persistent && it->expiry && now >= it->expiry) {
      it = cookies_.erase(it);
    } else {
      ++it;
    }
  }
  for (const auto &c : cookies_) {
    if (c.secure && url.scheme != "https") continue;
    if (!domain_matches(c.domain, c.host_only, url.host)) continue;
    if (!path_matches(c.path, url.request_target())) continue;
    pairs.push_back(c.name + "=" + c.value);
  }
  std::string out;
  for (size_t i = 0; i < pairs.size(); ++i) {
    if (i) out += "; ";
    out += pairs[i];
  }
  return out;
}

} /* namespace kathttp */
