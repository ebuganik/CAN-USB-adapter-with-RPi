#include <algorithm>
#include <fstream>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <vector>
#include <iterator>
constexpr const char *NETDEVICE_FILE = "/proc/net/dev";

const std::string WILDCARD = "*";

// This is what /proc/net/dev looks like:
/*
Inter-|   Receive                                                |  Transmit
 face |bytes    packets errs drop fifo frame compressed multicast|bytes    packets errs drop fifo colls carrier compressed
    lo:  445828     389    0    0    0     0          0         0   445828     389    0    0    0     0       0          0
enp4s0: 218610654  196129    0    0    0     0          0         4  4101718   38253    0    0    0     0       0          0
wlp5s0: 5831565   31037    0    0    0     0          0         0     7830      91    0    0    0     0       0          0
virbr0:       0       0    0    0    0     0          0         0        0       0    0    0    0     0       0          0
*/
const int IFACE_COLUMN = 0;
const int RECV_BYTES_COLUMN = 1;
const int SENT_BYTES_COLUMN = 9;

std::ifstream netdev(NETDEVICE_FILE);

struct InterfaceBandwidthStats {
  std::string iface;
  unsigned long long received_bytes;
  unsigned long long transmitted_bytes;
};

// Splits a whitespace-separated string into a vector.
std::vector<std::string> split(std::string const &input) {
  using namespace std;

  istringstream iss(input);
  vector<string> parts{istream_iterator<string>{iss},
                       istream_iterator<string>{}};
  return parts;
}

std::vector<struct InterfaceBandwidthStats>
read_network_stats(std::string iface) {
  netdev.clear();
  netdev.seekg(std::ios_base::beg);
  if (!netdev.good()) {
    return {};
  }

  std::vector<struct InterfaceBandwidthStats> results;

  std::string line;
  // skip the first two lines (headers)
  std::getline(netdev, line);
  std::getline(netdev, line);

  do {
    std::getline(netdev, line);
    std::vector<std::string> parts = split(line);
    if (parts.size() == 0) {
      continue;
    }

    auto candidate = parts.at(IFACE_COLUMN);
    // remove the trailing ':' so we can compare against the provided interface
    candidate.pop_back();

    if (WILDCARD.compare(iface) == 0 || candidate.compare(iface) == 0) {
      auto recv_bytes = std::stoull(parts.at(RECV_BYTES_COLUMN));
      auto sent_bytes = std::stoull(parts.at(SENT_BYTES_COLUMN));

      results.push_back({.iface = candidate,
                         .received_bytes = recv_bytes,
                         .transmitted_bytes = sent_bytes});
    }
  } while (netdev.good());

  return results;
}

int main(int argc, char **argv) {
  if (argc != 2) {
    std::cerr << "Usage: " << argv[0] << " <interface name>" << std::endl;
    return 2;
  }

  std::string iface(argv[1]);
  auto stats = read_network_stats(iface);
  if (stats.empty()) {
    std::cerr << "Unknown interface: " << iface << std::endl;
    return 2;
  }

  for (auto bw : stats) {
    std::cerr << bw.iface << " "
              << "︁ " << (bw.transmitted_bytes)
              << " total bytes ︁ " << (bw.received_bytes)
              << " total bytes" << std::endl;
  }

  return 0;
}