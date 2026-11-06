#pragma once

#include <stddef.h>
#include <string.h>

namespace CloudQueue {

template <typename Entry>
inline bool earlier(const Entry& left, const Entry& right) {
  // Logger timestamps are fixed-width UTC ISO strings. Empty timestamps sort
  // first; the path breaks ties independently of SD directory enumeration.
  const int timestamp = strcmp(left.timestamp.c_str(), right.timestamp.c_str());
  return timestamp < 0 ||
         (timestamp == 0 && strcmp(left.path.c_str(), right.path.c_str()) < 0);
}

template <typename Entry, size_t Capacity>
inline void selectOldest(Entry (&entries)[Capacity], size_t& count,
                         size_t maximum, const Entry& candidate) {
  const size_t limit = maximum < Capacity ? maximum : Capacity;
  size_t position = 0;
  while (position < count && !earlier(candidate, entries[position])) ++position;
  if (position >= limit) return;
  if (count < limit) ++count;
  for (size_t i = count - 1; i > position; --i) entries[i] = entries[i - 1];
  entries[position] = candidate;
}

}  // namespace CloudQueue
