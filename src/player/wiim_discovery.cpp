#include "wiim_discovery.h"

#include <ESPmDNS.h>

namespace wiim {

std::vector<Device> discover() {
  std::vector<Device> found;
  int n = MDNS.queryService("linkplay", "tcp");
  for (int i = 0; i < n; i++) found.push_back({MDNS.hostname(i), MDNS.IP(i).toString()});
  return found;
}

}  // namespace wiim
