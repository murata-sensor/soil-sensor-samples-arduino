#include <algorithm>
#include <cassert>
#include <random>
#include <string>
#include <vector>

#include "../examples/M5Core2CloudLogger/QueueOrder.h"

struct Entry {
  std::string timestamp;
  std::string path;
  std::string row;
};

void checkBatch(const std::vector<Entry>& input, size_t maximum) {
  Entry selected[20];
  size_t count = 0;
  for (const Entry& candidate : input) {
    CloudQueue::selectOldest(selected, count, maximum, candidate);
    assert(count <= std::min(maximum, size_t(20)));
  }
  std::vector<Entry> expected = input;
  std::sort(expected.begin(), expected.end(), CloudQueue::earlier<Entry>);
  const size_t expectedCount =
      std::min(input.size(), std::min(maximum, size_t(20)));
  assert(count == expectedCount);
  for (size_t i = 0; i < count; ++i) {
    assert(selected[i].timestamp == expected[i].timestamp);
    assert(selected[i].path == expected[i].path);
    assert(selected[i].row == expected[i].row);
  }
}

int main() {
  std::vector<Entry> input = {
      {"2026-11-06T00:15:30Z", "new.json", "new"},
      {"2026-11-05T14:02:54Z", "z.json", "old-z"},
      {"", "boot-z.json", "untimed-z"},
      {"2026-11-05T14:02:54Z", "a.json", "old-a"},
      {"", "boot-a.json", "untimed-a"},
  };
  std::mt19937 random(1234);
  for (int iteration = 0; iteration < 100; ++iteration) {
    std::shuffle(input.begin(), input.end(), random);
    for (size_t maximum :
         {size_t(0), size_t(1), size_t(3), size_t(10), size_t(20), size_t(21)})
      checkBatch(input, maximum);
  }
  for (int i = 0; i < 1000; ++i) {
    const std::string suffix = std::to_string(i);
    input.push_back({i % 7 == 0 ? "" : "2026-11-05T14:03:00Z",
                     "item-" + suffix + ".json", "payload-" + suffix});
  }
  for (int iteration = 0; iteration < 20; ++iteration) {
    std::shuffle(input.begin(), input.end(), random);
    checkBatch(input, 10);
    checkBatch(input, 20);
  }
  checkBatch({}, 10);

  // Repeated successful batches must drain in the same oldest-first order.
  std::vector<Entry> remaining = input;
  std::vector<Entry> sent;
  while (!remaining.empty()) {
    Entry selected[20];
    size_t count = 0;
    for (const Entry& candidate : remaining)
      CloudQueue::selectOldest(selected, count, 10, candidate);
    for (size_t i = 0; i < count; ++i) {
      sent.push_back(selected[i]);
      remaining.erase(std::remove_if(remaining.begin(), remaining.end(),
                                     [&](const Entry& row) {
                                       return row.path == selected[i].path;
                                     }),
                      remaining.end());
    }
  }
  assert(sent.size() == input.size());
  assert(std::is_sorted(sent.begin(), sent.end(), CloudQueue::earlier<Entry>));
}
