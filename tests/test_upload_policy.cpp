#include <assert.h>

#include <initializer_list>

#include "../examples/M5Core2CloudLogger/UploadPolicy.h"

namespace {
struct FakeClient {
  bool connected = true;
  unsigned stopCalls = 0;
  void stop() {
    connected = false;
    ++stopCalls;
  }
};

struct FakeRequest {
  FakeClient& client;
  unsigned endCalls = 0;
  unsigned receiveProbes = 0;
  explicit FakeRequest(FakeClient& connection) : client(connection) {}
  void end() {
    if (client.connected) ++receiveProbes;
    ++endCalls;
  }
};
}  // namespace

int main() {
  using CloudUpload::RedirectAction;
  assert(CloudUpload::redirectAction(301) == RedirectAction::KeepMethod);
  assert(CloudUpload::redirectAction(302) == RedirectAction::Get);
  assert(CloudUpload::redirectAction(303) == RedirectAction::Get);
  assert(CloudUpload::redirectAction(307) == RedirectAction::KeepMethod);
  assert(CloudUpload::redirectAction(308) == RedirectAction::KeepMethod);
  for (int status : {-1, 200, 300, 304, 400, 500})
    assert(CloudUpload::redirectAction(status) == RedirectAction::Stop);
  assert(!CloudUpload::responseAccepted(199, true));
  assert(CloudUpload::responseAccepted(200, true));
  assert(CloudUpload::responseAccepted(204, true));
  assert(CloudUpload::responseAccepted(299, true));
  assert(!CloudUpload::responseAccepted(300, true));
  assert(!CloudUpload::responseAccepted(200, false));
  assert(!CloudUpload::responseAccepted(-1, true));
  for (bool connected : {true, false}) {
    FakeClient client;
    client.connected = connected;
    FakeRequest request(client);
    CloudUpload::endRequest(client, request);
    assert(!client.connected);
    assert(client.stopCalls == 1);
    assert(request.endCalls == 1);
    assert(request.receiveProbes == 0);
  }
  return 0;
}
