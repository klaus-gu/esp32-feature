// network.cpp —— modules 层：网络状态接口的注册表

#include "network.h"

namespace {

net::Provider* gProvider = nullptr;

}  // namespace

namespace net {

void setProvider(Provider* provider) {
  gProvider = provider;
}

Provider* provider() {
  return gProvider;
}

}  // namespace net
