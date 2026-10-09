// zoo: the runner. It knows only the registry and the Simulation interface, so
// adding an exhibit never requires editing this file.
//
//   zoo list
//   zoo params <exhibit>
//   zoo run <exhibit> [--seed N] [--frames N] [--fps N] [--width W] [--height H]
//                     [--exposure F] [--out DIR] [--param key=value ...]

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <vector>

#include "core/image.hpp"
#include "core/registry.hpp"

using namespace zoo;

namespace {

void usage() {
    std::printf(
        "simulation zoo\n\n"
        "usage:\n"
        "  zoo list\n"
        "  zoo params <exhibit>\n"
        "  zoo run <exhibit> [options]\n\n"
        "options for run:\n"
        "  --seed N          deterministic seed (default 1)\n"
        "  --frames N        number of frames to render (default 300)\n"
        "  --fps N           frames per second used as the step dt (default 60)\n"
        "  --width W         image width in pixels (default 640)\n"
        "  --height H        image height in pixels (default 640)\n"
        "  --exposure F      tone-map exposure for additive exhibits\n"
        "  --out DIR         output directory (default out/<exhibit>)\n"
        "  --param k=v       override an exhibit parameter (repeatable)\n");
}

int cmd_list() {
    auto names = sim_names();
    if (names.empty()) {
        std::printf("no exhibits registered\n");
        return 1;
    }
    std::printf("%-12s %-9s %s\n", "EXHIBIT", "FAMILY", "DESCRIPTION");
    for (const auto& name : names) {
        auto sim = make_sim(name);
        if (!sim) continue;
        const auto& info = sim->info();
        std::printf("%-12s %-9s %s\n", info.name.c_str(), info.category.c_str(),
                    info.blurb.c_str());
    }
    return 0;
}

int cmd_params(const std::string& name) {
    auto sim = make_sim(name);
    if (!sim) {
        std::fprintf(stderr, "unknown exhibit: %s\n", name.c_str());
        return 1;
    }
    const auto& info = sim->info();
    std::printf("%s (%s)\n%s\n\nparameters:\n", info.name.c_str(), info.category.c_str(),
                info.blurb.c_str());
    for (const auto& kv : info.defaults) {
        std::printf("  %-18s %g\n", kv.first.c_str(), kv.second);
    }
    return 0;
}

double parse_double(const char* s, const char* flag) {
    char* end = nullptr;
    double v = std::strtod(s, &end);
    if (end == s) {
        std::fprintf(stderr, "error: %s expects a number, got '%s'\n", flag, s);
        std::exit(2);
    }
    return v;
}

int cmd_run(const std::string& name, int argc, char** argv) {
    auto sim = make_sim(name);
    if (!sim) {
        std::fprintf(stderr, "unknown exhibit: %s\n", name.c_str());
        return 1;
    }
    const auto& info = sim->info();

    uint64_t seed = 1;
    int frames = 300;
    double fps = 60.0;
    int width = 640;
    int height = 640;
    bool have_exposure = false;
    double exposure = 1.0;
    std::string out = "out/" + name;
    Params params = info.defaults;

    for (int i = 0; i < argc; ++i) {
        std::string a = argv[i];
        auto next = [&](const char* flag) -> const char* {
            if (i + 1 >= argc) {
                std::fprintf(stderr, "error: %s needs a value\n", flag);
                std::exit(2);
            }
            return argv[++i];
        };
        if (a == "--seed") seed = static_cast<uint64_t>(parse_double(next("--seed"), "--seed"));
        else if (a == "--frames") frames = static_cast<int>(parse_double(next("--frames"), "--frames"));
        else if (a == "--fps") fps = parse_double(next("--fps"), "--fps");
        else if (a == "--width") width = static_cast<int>(parse_double(next("--width"), "--width"));
        else if (a == "--height") height = static_cast<int>(parse_double(next("--height"), "--height"));
        else if (a == "--exposure") { exposure = parse_double(next("--exposure"), "--exposure"); have_exposure = true; }
        else if (a == "--out") out = next("--out");
        else if (a == "--param") {
            std::string kv = next("--param");
            auto eq = kv.find('=');
            if (eq == std::string::npos) {
                std::fprintf(stderr, "error: --param expects key=value\n");
                return 2;
            }
            params[kv.substr(0, eq)] = std::strtod(kv.substr(eq + 1).c_str(), nullptr);
        } else {
            std::fprintf(stderr, "error: unknown option '%s'\n", a.c_str());
            return 2;
        }
    }

    if (width < 1 || height < 1 || frames < 1) {
        std::fprintf(stderr, "error: width, height and frames must be positive\n");
        return 2;
    }
    if (!have_exposure) {
        auto it = params.find("exposure");
        if (it != params.end()) exposure = it->second;
    }

    sim->init(seed, params);
    Framebuffer fb(width, height);
    std::vector<uint8_t> rgb;

    std::error_code ec;
    std::filesystem::create_directories(out, ec);
    if (ec) {
        std::fprintf(stderr, "error: cannot create %s: %s\n", out.c_str(), ec.message().c_str());
        return 1;
    }

    std::printf("render %s  seed=%llu frames=%d fps=%g size=%dx%d family=%s\n",
                info.name.c_str(), static_cast<unsigned long long>(seed), frames, fps,
                width, height, info.category.c_str());

    const double dt = 1.0 / fps;
    for (int f = 0; f < frames; ++f) {
        sim->render(fb);
        fb.resolve(rgb, static_cast<float>(exposure), info.render == RenderStyle::Additive);
        char path[1024];
        std::snprintf(path, sizeof(path), "%s/frame_%04d.ppm", out.c_str(), f);
        if (!write_ppm(path, width, height, rgb)) {
            std::fprintf(stderr, "error: failed to write %s\n", path);
            return 1;
        }
        sim->step(dt);
    }
    std::printf("wrote %d frames to %s\n", frames, out.c_str());
    return 0;
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        usage();
        return 1;
    }
    std::string cmd = argv[1];
    if (cmd == "list") return cmd_list();
    if (cmd == "params") {
        if (argc < 3) { std::fprintf(stderr, "usage: zoo params <exhibit>\n"); return 1; }
        return cmd_params(argv[2]);
    }
    if (cmd == "run") {
        if (argc < 3) { std::fprintf(stderr, "usage: zoo run <exhibit> [options]\n"); return 1; }
        return cmd_run(argv[2], argc - 3, argv + 3);
    }
    if (cmd == "help" || cmd == "-h" || cmd == "--help") { usage(); return 0; }
    std::fprintf(stderr, "unknown command: %s\n", cmd.c_str());
    usage();
    return 1;
}
