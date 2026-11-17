// Copyright 2026 The Nagi Authors.
// Use of this source code is governed by the BSD 3-Clause license in LICENSE.

// The Zenz runtime is only set up on macOS. Other platforms get a client that
// never returns a result.

#include <cstdint>
#include <string_view>

#include "zenz/zenz_client.h"

namespace mozc {
namespace zenz {
namespace {

class NullZenzClient : public ZenzClientInterface {
 public:
  void Request(uint32_t id, std::string_view reading,
               std::string_view left_context) override {}
  ZenzResult Take(uint32_t id) override { return ZenzResult(); }
  void Cancel() override {}
  void WarmUp() override {}
};

}  // namespace

ZenzClientInterface* CreateDefaultZenzClient() { return new NullZenzClient(); }

}  // namespace zenz
}  // namespace mozc
