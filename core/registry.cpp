#include "registry.hpp"

#include <algorithm>
#include <map>

namespace zoo {

namespace {
std::map<std::string, SimFactory>& table() {
    static std::map<std::string, SimFactory> t;
    return t;
}
} // namespace

bool register_sim(const std::string& name, SimFactory factory) {
    auto& t = table();
    auto it = t.find(name);
    if (it != t.end()) return false;
    t.emplace(name, std::move(factory));
    return true;
}

std::vector<std::string> sim_names() {
    std::vector<std::string> names;
    names.reserve(table().size());
    for (const auto& kv : table()) names.push_back(kv.first);
    return names; // std::map iterates in sorted order
}

std::unique_ptr<Simulation> make_sim(const std::string& name) {
    auto it = table().find(name);
    if (it == table().end()) return nullptr;
    return it->second();
}

} // namespace zoo
