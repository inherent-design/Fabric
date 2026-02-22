#pragma once

#include <algorithm>
#include <cstddef>
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace fabric {

// Ordered middleware chain with context passing and short-circuit support.
// Handlers are sorted by priority (lower runs first; stable within equal priority).
// Each handler receives the context and a next() function. Calling next()
// proceeds to the next handler; skipping it short-circuits the pipeline.
template <typename Context>
class Pipeline {
public:
  using Handler = std::function<void(Context&, std::function<void()> next)>;

  void addHandler(Handler handler, int priority = 0) {
    entries_.push_back(Entry{"", std::move(handler), priority, insertOrder_++});
    dirty_ = true;
  }

  void addHandler(std::string name, Handler handler, int priority = 0) {
    entries_.push_back(
        Entry{std::move(name), std::move(handler), priority, insertOrder_++});
    dirty_ = true;
  }

  bool removeHandler(const std::string& name) {
    auto it = std::remove_if(entries_.begin(), entries_.end(),
                             [&](const Entry& e) { return e.name == name; });
    if (it == entries_.end())
      return false;
    entries_.erase(it, entries_.end());
    dirty_ = true;
    return true;
  }

  void execute(Context& ctx) {
    ensureSorted();
    executeAt(0, ctx);
  }

  size_t handlerCount() const { return entries_.size(); }

private:
  struct Entry {
    std::string name;
    Handler handler;
    int priority;
    size_t order; // insertion order for stable sorting
  };

  void ensureSorted() {
    if (!dirty_)
      return;
    std::stable_sort(entries_.begin(), entries_.end(),
                     [](const Entry& a, const Entry& b) {
                       if (a.priority != b.priority)
                         return a.priority < b.priority;
                       return a.order < b.order;
                     });
    dirty_ = false;
  }

  void executeAt(size_t index, Context& ctx) {
    if (index >= entries_.size())
      return;
    auto& entry = entries_[index];
    entry.handler(ctx, [this, index, &ctx]() { executeAt(index + 1, ctx); });
  }

  std::vector<Entry> entries_;
  size_t insertOrder_ = 0;
  bool dirty_ = false;
};

} // namespace fabric
