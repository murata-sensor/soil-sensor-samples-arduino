#pragma once

namespace CloudUpload {

enum class RedirectAction { Stop, KeepMethod, Get };

inline RedirectAction redirectAction(int httpStatus) {
  if (httpStatus == 302 || httpStatus == 303) return RedirectAction::Get;
  if (httpStatus == 301 || httpStatus == 307 || httpStatus == 308)
    return RedirectAction::KeepMethod;
  return RedirectAction::Stop;
}

inline bool responseAccepted(int httpStatus, bool bodySaysOk) {
  return httpStatus >= 200 && httpStatus < 300 && bodySaysOk;
}

template <typename Client, typename Request>
inline void endRequest(Client& client, Request& request) {
  // HTTPClient::end probes for unread TLS data. No reuse is intended here;
  // close first, including when a redirect body is deliberately discarded.
  client.stop();
  request.end();
}

}  // namespace CloudUpload
