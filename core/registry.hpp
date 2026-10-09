#pragma once
// Exhibit registry.
//
// Exhibits register themselves at load time with ZOO_REGISTER_EXHIBIT. The
// runner only ever talks to the registry, never to a concrete exhibit type, so
// adding an exhibit never requires touching the runner.

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "sim.hpp"

namespace zoo {

using SimFactory = std::function<std::unique_ptr<Simulation>()>;

// Register a factory under `name`. Returns true. Registering the same name
// twice keeps the first and returns false (a soft guard against two exhibits
// claiming one name).
bool register_sim(const std::string& name, SimFactory factory);

// All registered exhibit names, sorted.
std::vector<std::string> sim_names();

// Instantiate an exhibit, or nullptr if the name is unknown.
std::unique_ptr<Simulation> make_sim(const std::string& name);

} // namespace zoo

// Place near the top of an exhibit .cpp, after the class definition.
#define ZOO_REGISTER_EXHIBIT(TYPE, NAME)                                        \
    namespace {                                                                 \
    const bool zoo_registered_##TYPE = ::zoo::register_sim(                      \
        (NAME), []() -> std::unique_ptr<::zoo::Simulation> {                    \
            return std::unique_ptr<::zoo::Simulation>(new TYPE());             \
        });                                                                     \
    }
