#include <re2/re2.h>
#include <array>
#include <string_view>

#include <cctype>
#include <cerrno>
#include <netdb.h>
#include <netinet/in.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/un.h>
#include <pwd.h>
#include <unistd.h>
#include <algorithm>
#include <csignal>
#include <ranges>
#include <optional>
#include <charconv>

#include <iostream>
#include <string>
#include <print>
#include <fstream>
#include <vector>
#include <filesystem>
#include <cstdarg>
#include <hyprutils/string/String.hpp>
#include <hyprutils/memory/Casts.hpp>
using namespace Hyprutils::String;
using namespace Hyprutils::Memory;

#include "Strings.hpp"
#include "hyprpaper/Hyprpaper.hpp"

#include <readline/readline.h>
#include <readline/history.h>

std::string instanceSignature;
bool        quiet = false;

struct SInstanceData {
    std::string id;
    uint64_t    time;
    uint64_t    pid;
    std::string wlSocket;
};

void log(const std::string_view str) {
    if (quiet)
        return;

    std::println("{}", str);
}

static bool writeAll(const int fd, std::string_view data) {
    size_t totalWritten = 0;
    while (totalWritten < data.size()) {
        const auto written = write(fd, data.data() + totalWritten, data.size() - totalWritten);
        if (written > 0) {
            totalWritten += sc<size_t>(written);
            continue;
        }

        if (written < 0 && errno == EINTR)
            continue;

        return false;
    }

    return true;
}

static int getUID() {
    const auto UID   = getuid();
    const auto PWUID = getpwuid(UID);
    return PWUID ? PWUID->pw_uid : UID;
}

std::string getRuntimeDir() {
    const auto XDG = getenv("XDG_RUNTIME_DIR");

    if (!XDG) {
        const std::string USERID = std::to_string(getUID());
        return "/run/user/" + USERID + "/hypr";
    }

    return std::string{XDG} + "/hypr";
}

static std::optional<uint64_t> toUInt64(const std::string_view str) {
    uint64_t value       = 0;
    const auto [ptr, ec] = std::from_chars(str.data(), str.data() + str.size(), value);
    if (ec != std::errc() || ptr != str.data() + str.size())
        return std::nullopt;
    return value;
}

static std::optional<SInstanceData> parseInstance(const std::filesystem::directory_entry& entry) {
    if (!entry.is_directory())
        return std::nullopt;

    const auto    lockPath = entry.path() / "hyprland.lock";
    std::ifstream ifs(lockPath);
    if (!ifs.is_open())
        return std::nullopt;

    SInstanceData data;
    data.id = entry.path().filename().string();

    const auto first = std::string_view{data.id}.find_first_of('_');
    const auto last  = std::string_view{data.id}.find_last_of('_');
    if (first == std::string_view::npos || last == std::string_view::npos || last <= first)
        return std::nullopt;

    auto time = toUInt64(std::string_view{data.id}.substr(first + 1, last - first - 1));
    if (!time)
        return std::nullopt;
    data.time = *time;

    std::string line;
    if (!std::getline(ifs, line))
        return std::nullopt;

    auto pid = toUInt64(std::string_view{line});
    if (!pid)
        return std::nullopt;
    data.pid = *pid;

    if (!std::getline(ifs, data.wlSocket))
        return std::nullopt;

    if (std::getline(ifs, line) && !line.empty())
        return std::nullopt; // more lines than expected

    return data;
}

std::vector<SInstanceData> instances() {
    std::vector<SInstanceData> result;

    std::error_code            ec;
    const auto                 runtimeDir = getRuntimeDir();
    if (!std::filesystem::exists(runtimeDir, ec) || ec)
        return result;

    std::filesystem::directory_iterator it(runtimeDir, std::filesystem::directory_options::skip_permission_denied, ec);
    if (ec)
        return result;

    for (const auto& el : it) {
        if (auto instance = parseInstance(el))
            result.emplace_back(std::move(*instance));
    }

    std::erase_if(result, [](const auto& el) { return kill(el.pid, 0) != 0 && errno == ESRCH; });

    std::ranges::sort(result, {}, &SInstanceData::time);

    return result;
}

static volatile bool sigintReceived = false;
void                 intHandler(int sig) {
    sigintReceived = true;
    std::println("[hyprctl] SIGINT received, closing connection");
}

int rollingRead(const int socket) {
    sigintReceived = false;
    signal(SIGINT, intHandler);

    constexpr size_t              BUFFER_SIZE = 8192;
    std::array<char, BUFFER_SIZE> buffer      = {0};
    long                          sizeWritten = 0;
    std::println("[hyprctl] reading from socket following up log:");
    while (!sigintReceived) {
        sizeWritten = read(socket, buffer.data(), BUFFER_SIZE);
        if (sizeWritten < 0 && errno != EAGAIN) {
            if (errno != EINTR)
                std::println("Couldn't read (5): {}: {}", strerror(errno), errno);
            close(socket);
            return 5;
        }

        if (sizeWritten == 0)
            break;

        if (sizeWritten > 0) {
            std::println("{}", std::string(buffer.data(), sizeWritten));
            buffer.fill('\0');
        }

        usleep(100000);
    }
    close(socket);
    return 0;
}

int request(std::string_view arg, int minArgs = 0, bool needRoll = false) {
    const auto SERVERSOCKET = socket(AF_UNIX, SOCK_STREAM, 0);

    if (SERVERSOCKET < 0) {
        log("Couldn't open a socket (1)");
        return 1;
    }

    auto t = timeval{.tv_sec = 5, .tv_usec = 0};
    if (setsockopt(SERVERSOCKET, SOL_SOCKET, SO_RCVTIMEO, &t, sizeof(struct timeval)) < 0) {
        log("Couldn't set socket timeout (2)");
        return 2;
    }

    const auto ARGS = std::count(arg.begin(), arg.end(), ' ');

    if (ARGS < minArgs) {
        log(std::format("Not enough arguments in '{}', expected at least {}", arg, minArgs));
        return -1;
    }

    if (instanceSignature.empty()) {
        log("HYPRLAND_INSTANCE_SIGNATURE was not set! (Is Hyprland running?) (3)");
        return 3;
    }

    sockaddr_un serverAddress = {0};
    serverAddress.sun_family  = AF_UNIX;

    std::string socketPath = getRuntimeDir() + "/" + instanceSignature + "/.socket.sock";

    strncpy(serverAddress.sun_path, socketPath.c_str(), sizeof(serverAddress.sun_path) - 1);

    if (connect(SERVERSOCKET, rc<sockaddr*>(&serverAddress), SUN_LEN(&serverAddress)) < 0) {
        log("Couldn't connect to " + socketPath + ". (4)");
        return 4;
    }

    if (!writeAll(SERVERSOCKET, arg)) {
        log("Couldn't write (5)");
        return 5;
    }

    if (needRoll)
        return rollingRead(SERVERSOCKET);

    std::string      reply               = "";
    constexpr size_t BUFFER_SIZE         = 8192;
    char             buffer[BUFFER_SIZE] = {0};

    // read all data until server closes the connection
    // this handles partial writes on the server side under high load
    while (true) {
        const auto sizeRead = read(SERVERSOCKET, buffer, BUFFER_SIZE);

        if (sizeRead < 0) {
            if (errno == EINTR)
                continue;
            if (errno == EWOULDBLOCK)
                log("Hyprland IPC didn't respond in time\n");
            log("Couldn't read (6)");
            return 6;
        }

        if (sizeRead == 0) {
            // server closed connection, we're done
            break;
        }

        reply.append(buffer, sc<size_t>(sizeRead));
    }

    close(SERVERSOCKET);

    log(reply);

    if (reply.starts_with("error:"))
        return 7;

    return 0;
}

int requestIPC(std::string_view filename, std::string_view arg) {
    const auto SERVERSOCKET = socket(AF_UNIX, SOCK_STREAM, 0);

    if (SERVERSOCKET < 0) {
        log("Couldn't open a socket (1)");
        return 1;
    }

    if (instanceSignature.empty()) {
        log("HYPRLAND_INSTANCE_SIGNATURE was not set! (Is Hyprland running?)");
        return 2;
    }

    sockaddr_un serverAddress = {0};
    serverAddress.sun_family  = AF_UNIX;

    std::string socketPath = getRuntimeDir() + "/" + instanceSignature + "/" + filename;

    strncpy(serverAddress.sun_path, socketPath.c_str(), sizeof(serverAddress.sun_path) - 1);

    if (connect(SERVERSOCKET, rc<sockaddr*>(&serverAddress), SUN_LEN(&serverAddress)) < 0) {
        log("Couldn't connect to " + socketPath + ". (3)");
        return 3;
    }

    arg = arg.substr(arg.find_first_of('/') + 1); // strip flags
    arg = arg.substr(arg.find_first_of(' ') + 1); // strip "hyprpaper"

    if (!writeAll(SERVERSOCKET, arg)) {
        log("Couldn't write (4)");
        return 4;
    }
    constexpr size_t BUFFER_SIZE         = 8192;
    char             buffer[BUFFER_SIZE] = {0};

    ssize_t          sizeRead = 0;
    do {
        sizeRead = read(SERVERSOCKET, buffer, BUFFER_SIZE);
    } while (sizeRead < 0 && errno == EINTR);

    if (sizeRead < 0) {
        log("Couldn't read (5)");
        return 5;
    }

    close(SERVERSOCKET);

    log(std::string(buffer, sc<size_t>(sizeRead)));

    return 0;
}

int requestHyprsunset(std::string_view arg) {
    return requestIPC(".hyprsunset.sock", arg);
}

void batchRequest(std::string_view arg, bool json) {
    std::string commands(arg.substr(arg.find_first_of(' ') + 1));

    if (json) {
        RE2::GlobalReplace(&commands, ";\\s*", ";j/");
        commands.insert(0, "j/");
    }

    std::string rq = "[[BATCH]]" + commands;
    request(rq);
}

void instancesRequest(bool json) {
    std::string result = "";

    // gather instance data
    std::vector<SInstanceData> inst = instances();

    if (!json) {
        for (auto const& el : inst) {
            result += std::format("instance {}:\n\ttime: {}\n\tpid: {}\n\twl socket: {}\n\n", el.id, el.time, el.pid, el.wlSocket);
        }
    } else {
        result += '[';
        for (auto const& el : inst) {
            result += std::format(R"#(
{{
    "instance": "{}",
    "time": {},
    "pid": {},
    "wl_socket": "{}"
}},)#",
                                  el.id, el.time, el.pid, el.wlSocket);
        }

        result.pop_back();
        result += "\n]";
    }

    log(result + "\n");
}

std::vector<std::string> splitArgs(int argc, char** argv) {
    std::vector<std::string> result;

    for (auto i = 1 /* skip the executable */; i < argc; ++i)
        result.emplace_back(argv[i]);

    return result;
}

// Single-quotes a string for sh.
static std::string shellQuote(const std::string& s) {
    std::string out = "'";
    for (const char c : s) {
        if (c == '\'')
            out += "'\\''";
        else
            out += c;
    }
    return out + "'";
}

// The dotfile sets `hyprctl dots <set> --install` can install. Each one is cloned as it is upstream and nothing in the
// clone is edited. The entries it provides are backed up to ~/.local/state/halcyon/backups, then its own installer runs.
// Every set replaces the desktop's config in ~/.config, so `hyprctl dots plus --install` brings Halcyon back.
struct SDotSet {
    std::string_view name;  // the name hyprctl takes
    std::string_view alias; // a second name, or ""
    std::string_view title;
    std::string_view url;        // the upstream repository, cloned shallow
    std::string_view configDir;  // folder in the repository whose entries go to ~/.config
    std::string_view binDir;     // folder in the repository whose entries go to ~/.local/bin, or ""
    std::string_view install;    // shell command run after the backup, in the clone ($DIR)
    bool             clearFirst; // remove the entries after backing them up, instead of copying over them
    std::string_view note;
};

static const std::array<SDotSet, 3> DOT_SETS = {{
    {
        .name       = "jakoolit",
        .alias      = "hyprland-dots",
        .title      = "JaKooLit Hyprland-Dots",
        .url        = "https://github.com/JaKooLit/Hyprland-Dots",
        .configDir  = "config",
        .binDir     = "",
        .install    = "cd \"$DIR\" && chmod +x copy.sh && ./copy.sh",
        .clearFirst = false,
        .note       = "Its copy.sh asks questions in the terminal and makes its own backups. The repository says it was archived "
                      "in March 2026 and is now maintained at https://github.com/LinuxBeginnings/Hyprland-Dots.",
    },
    {
        .name       = "mlfw",
        .alias      = "ml4w",
        .title      = "ML4W OS (mylinuxforwork/dotfiles)",
        .url        = "https://github.com/mylinuxforwork/dotfiles",
        .configDir  = "dotfiles/.config",
        .binDir     = "",
        .install    = "curl -fsSL https://ml4w.com/os/stable -o \"$ROOT/ml4w-install.sh\" && bash \"$ROOT/ml4w-install.sh\"",
        .clearFirst = false,
        .note       = "The installer is ML4W's own, from ml4w.com as its docs say. It installs packages and asks questions in the "
                      "terminal. To read it first: curl -fsSL https://ml4w.com/os/stable | less",
    },
    {
        .name       = "gdots",
        .alias      = "gdot",
        .title      = "gdots (semi710/gdots)",
        .url        = "https://github.com/semi710/gdots",
        .configDir  = ".config",
        .binDir     = "bin",
        .install    = "mkdir -p \"$HOME/.config\" \"$HOME/.local/bin\" && cp -a \"$DIR/.config/.\" \"$HOME/.config/\" && "
                      "cp -a \"$DIR/bin/.\" \"$HOME/.local/bin/\"",
        .clearFirst = true,
        .note       = "Copied rather than stowed, so Halcyon's installer never writes into the clone. It does not install the "
                      "packages its README lists (pywal16, waybar, hyprlock and others).",
    },
}};

// The shell script `hyprctl dots <set> --install` runs: fetch or update the clone, back up what the set would replace,
// optionally clear it, then run the set's install.
static std::string dotSetScript(const SDotSet& set) {
    const std::string NAME = std::string(set.name);
    std::string       s;
    s += "set -e\n";
    s += "ROOT=\"$HOME/.local/share/halcyon/dotsets\"\n";
    s += "DIR=\"$ROOT/" + NAME + "\"\n";
    s += "STATE=\"${XDG_STATE_HOME:-$HOME/.local/state}/halcyon\"\n";
    s += "command -v git >/dev/null 2>&1 || { echo 'git is needed to fetch the dotfiles. Install it, then run this again.'; exit 1; }\n";
    s += "mkdir -p \"$ROOT\"\n";
    s += "if [ -d \"$DIR/.git\" ]; then git -C \"$DIR\" pull --ff-only; else git clone --depth 1 " + shellQuote(std::string(set.url)) + " \"$DIR\"; fi\n";
    s += "BK=\"$STATE/backups/" + NAME + "-$(date +%Y%m%d-%H%M%S)\"\n";
    s += "mkdir -p \"$BK/config\" \"$BK/bin\"\n";
    s += "entries() { find \"$1\" -mindepth 1 -maxdepth 1 -printf '%f\\n'; }\n";
    s += "entries \"$DIR/" + std::string(set.configDir) +
        "\" | while IFS= read -r n; do "
        "if [ -e \"$HOME/.config/$n\" ] || [ -L \"$HOME/.config/$n\" ]; then cp -a \"$HOME/.config/$n\" \"$BK/config/\"; fi; done\n";
    if (!set.binDir.empty()) {
        s += "entries \"$DIR/" + std::string(set.binDir) +
            "\" | while IFS= read -r n; do "
            "if [ -e \"$HOME/.local/bin/$n\" ] || [ -L \"$HOME/.local/bin/$n\" ]; then cp -a \"$HOME/.local/bin/$n\" \"$BK/bin/\"; fi; done\n";
    }
    if (set.clearFirst) {
        s += "entries \"$DIR/" + std::string(set.configDir) + "\" | while IFS= read -r n; do rm -rf \"$HOME/.config/$n\"; done\n";
        if (!set.binDir.empty())
            s += "entries \"$DIR/" + std::string(set.binDir) + "\" | while IFS= read -r n; do rm -rf \"$HOME/.local/bin/$n\"; done\n";
    }
    s += std::string(set.install) + "\n";
    s += "echo\n";
    s += "echo \"Installed " + std::string(set.title) + ". Your previous settings are in $BK\"\n";
    s += "echo \"To go back to Halcyon, run: hyprctl dots plus --install\"\n";
    return s;
}

static int dotsUsage() {
    std::println("usage: hyprctl dots [plus | jakoolit | mlfw | gdots] [--install]");
    std::println("  hyprctl dots                 list the sets");
    std::println("  hyprctl dots <set>           describe what installing it would do");
    std::println("  hyprctl dots <set> --install clone it, back up the config it replaces, and install it (run in a terminal)");
    return 1;
}

static int dotsList() {
    std::println("hyprctl dots plus [--install]     Halcyon, from its dots checkout (hyprland-plus is already installed)");
    for (const auto& set : DOT_SETS)
        std::println("hyprctl dots {} [--install]{}{}", set.name, std::string(set.name).size() < 9 ? "     " : "  ", set.title);
    return 0;
}

static int describeDotSet(const SDotSet& set) {
    std::println("{}", set.title);
    std::println("  from:     {}", set.url);
    std::println("  clone:    ~/.local/share/halcyon/dotsets/{} (not edited)", set.name);
    std::println("  backup:   the entries it provides, to ~/.local/state/halcyon/backups/{}-<time>", set.name);
    std::println("  install:  {}", set.install);
    std::println("  {}", set.note);
    std::println("This replaces the Halcyon config in ~/.config. \"hyprctl dots plus --install\" brings Halcyon back.");
    std::println("Run \"hyprctl dots {} --install\" in a terminal to install it.", set.name);
    return 0;
}

static int runDotSet(const SDotSet& set, bool install) {
    if (!install)
        return describeDotSet(set);

    const std::string SCRIPT = dotSetScript(set);
    execl("/bin/sh", "sh", "-c", SCRIPT.c_str(), static_cast<char*>(nullptr));
    std::println("could not start the install: {}", std::strerror(errno));
    return 1;
}

// hyprctl dots plus [--install]: the Halcyon dots, from the checkout the dots installer recorded in
// ~/.local/state/halcyon/repo. "plus" means hyprland-plus is already installed, so the dots installer
// does not build it again. --install runs the dots installer non-interactively (setup install --force).
static int runPlus(int argc) {
    const char*       XDG_STATE  = std::getenv("XDG_STATE_HOME");
    const char*       HOME_DIR   = std::getenv("HOME");
    const std::string STATE_HOME = XDG_STATE ? std::string(XDG_STATE) : std::string(HOME_DIR ? HOME_DIR : "") + "/.local/state";
    const std::string REPO_FILE  = STATE_HOME + "/halcyon/repo";

    std::ifstream     repoStream(REPO_FILE);
    std::string       repo;
    if (!repoStream || !std::getline(repoStream, repo) || repo.empty()) {
        std::println("The Halcyon dots are not set up yet. Run ./setup install in the dots repository first.");
        return 1;
    }

    if (argc == 3) {
        std::println("hyprland-plus is installed. Halcyon dots checkout: {}", repo);
        return 0;
    }

    const std::string CMD = "cd " + shellQuote(repo) + " && HALCYON_SKIP_HYPRLAND_PLUS=1 ./setup install --force";
    execl("/bin/sh", "sh", "-c", CMD.c_str(), static_cast<char*>(nullptr));
    std::println("could not start the dots installer: {}", std::strerror(errno));
    return 1;
}

// hyprctl dots [plus | <set>] [--install]: see dotsUsage().
static int runDotsCommand(int argc, char** argv) {
    const std::string_view NAME    = argc >= 3 ? std::string_view{argv[2]} : std::string_view{};
    const bool             INSTALL = argc == 4 && std::string_view{argv[3]} == "--install";
    if (argc > 4 || (argc == 4 && !INSTALL))
        return dotsUsage();

    if (NAME.empty())
        return dotsList();

    if (NAME == "plus")
        return runPlus(argc);

    for (const auto& set : DOT_SETS) {
        if (NAME == set.name || (!set.alias.empty() && NAME == set.alias))
            return runDotSet(set, INSTALL);
    }

    std::println("unknown dots set: {}", NAME);
    return dotsUsage();
}

int main(int argc, char** argv) {
    bool parseArgs = true;

    if (argc < 2) {
        std::println("{}", USAGE);
        return 1;
    }

    if (std::string_view{argv[1]} == "dots")
        return runDotsCommand(argc, argv);

    std::string fullRequest      = "";
    std::string fullArgs         = "";
    const auto  ARGS             = splitArgs(argc, argv);
    bool        json             = false;
    bool        needRoll         = false;
    std::string overrideInstance = "";

    for (std::size_t i = 0; i < ARGS.size(); ++i) {
        if (ARGS[i] == "--") {
            // Stop parsing arguments after --
            parseArgs = false;
            continue;
        }
        if (parseArgs && (ARGS[i][0] == '-') && !(isNumber(ARGS[i], true) || isNumber(ARGS[i].substr(0, ARGS[i].length() - 1), true)) /* For stuff like -2 or -2, */) {
            // parse
            if (ARGS[i] == "-j" && !fullArgs.contains("j")) {
                fullArgs += "j";
                json = true;
            } else if (ARGS[i] == "-r" && !fullArgs.contains("r")) {
                fullArgs += "r";
            } else if (ARGS[i] == "-a" && !fullArgs.contains("a")) {
                fullArgs += "a";
            } else if ((ARGS[i] == "-c" || ARGS[i] == "--config") && !fullArgs.contains("c")) {
                fullArgs += "c";
            } else if ((ARGS[i] == "-f" || ARGS[i] == "--follow") && !fullArgs.contains("f")) {
                fullArgs += "f";
                needRoll = true;
            } else if (ARGS[i] == "--batch") {
                fullRequest = "--batch ";
            } else if (ARGS[i] == "--instance" || ARGS[i] == "-i") {
                ++i;

                if (i >= ARGS.size()) {
                    std::println("{}", USAGE);
                    return 1;
                }

                overrideInstance = ARGS[i];
            } else if (ARGS[i] == "-q" || ARGS[i] == "--quiet") {
                quiet = true;
            } else if (ARGS[i] == "--help") {
                const std::string& cmd = ARGS[0];

                if (cmd == "hyprpaper") {
                    std::println("{}", HYPRPAPER_HELP);
                } else if (cmd == "hyprsunset") {
                    std::println("{}", HYPRSUNSET_HELP);
                } else if (cmd == "notify") {
                    std::println("{}", NOTIFY_HELP);
                } else if (cmd == "output") {
                    std::println("{}", OUTPUT_HELP);
                } else if (cmd == "plugin") {
                    std::println("{}", PLUGIN_HELP);
                } else if (cmd == "setprop") {
                    std::println("{}", SETPROP_HELP);
                } else if (cmd == "getprop") {
                    std::println("{}", GETPROP_HELP);
                } else if (cmd == "switchxkblayout") {
                    std::println("{}", SWITCHXKBLAYOUT_HELP);
                } else {
                    std::println("{}", USAGE);
                }

                return 1;
            } else {
                std::println("{}", USAGE);
                return 1;
            }

            continue;
        }

        fullRequest += ARGS[i] + " ";
    }

    if (fullRequest.empty()) {
        std::println("{}", USAGE);
        return 1;
    }

    fullRequest.pop_back(); // remove trailing space

    fullRequest = fullArgs + "/" + fullRequest;

    // instances is HIS-independent
    if (fullRequest.contains("/instances")) {
        instancesRequest(json);
        return 0;
    }

    if (needRoll && !fullRequest.contains("/rollinglog")) {
        log("only 'rollinglog' command supports '--follow' option");
        return 1;
    }

    if (overrideInstance.contains("_"))
        instanceSignature = overrideInstance;
    else if (!overrideInstance.empty()) {
        if (!isNumber(overrideInstance, false)) {
            log("instance invalid\n");
            return 1;
        }

        const auto INSTANCENO = std::stoi(overrideInstance);

        const auto INSTANCES = instances();

        if (INSTANCENO < 0 || sc<std::size_t>(INSTANCENO) >= INSTANCES.size()) {
            log("no such instance\n");
            return 1;
        }

        instanceSignature = INSTANCES[INSTANCENO].id;
    } else {
        const auto ISIG = getenv("HYPRLAND_INSTANCE_SIGNATURE");

        if (!ISIG) {
            log("HYPRLAND_INSTANCE_SIGNATURE not set! (is hyprland running?)\n");
            return 1;
        }

        instanceSignature = ISIG;
    }

    int exitStatus = 0;

    if (fullRequest.contains("/--batch"))
        batchRequest(fullRequest, json);
    else if (fullRequest.contains("/hyprpaper")) {
        auto result = Hyprpaper::makeHyprpaperRequest(fullRequest);
        if (!result)
            log(std::format("error: {}", result.error()));
        exitStatus = !result;
    } else if (fullRequest.contains("/hyprsunset"))
        exitStatus = requestHyprsunset(fullRequest);
    else if (fullRequest.contains("/switchxkblayout"))
        exitStatus = request(fullRequest, 2);
    else if (fullRequest.contains("/seterror"))
        exitStatus = request(fullRequest, 1);
    else if (fullRequest.contains("/setprop"))
        exitStatus = request(fullRequest, 3);
    else if (fullRequest.contains("/plugin"))
        exitStatus = request(fullRequest, 1);
    else if (fullRequest.contains("/dismissnotify"))
        exitStatus = request(fullRequest, 0);
    else if (fullRequest.contains("/notify"))
        exitStatus = request(fullRequest, 2);
    else if (fullRequest.contains("/output"))
        exitStatus = request(fullRequest, 2);
    else if (fullRequest.contains("/setcursor"))
        exitStatus = request(fullRequest, 1);
    else if (fullRequest.contains("/dispatch"))
        exitStatus = request(fullRequest, 1);
    else if (fullRequest.contains("/keyword"))
        exitStatus = request(fullRequest, 2);
    else if (fullRequest.contains("/decorations"))
        exitStatus = request(fullRequest, 1);
    else if (fullRequest.contains("/eval"))
        exitStatus = request(fullRequest, 1);
    else if (fullRequest.contains("/--help"))
        std::println("{}", USAGE);
    else if (fullRequest.contains("/rollinglog") && needRoll)
        exitStatus = request(fullRequest, 0, true);
    else if (fullRequest.contains("/repl")) {
        if (ARGS.size() > 1) {
            // single command with output
            exitStatus = request(fullRequest, 1);
        } else {
            // interactive REPL mode
            char* input = nullptr;
            while ((input = readline("> ")) != nullptr) {
                std::string line(input);
                if (!line.empty()) {
                    exitStatus = request("/repl " + line);
                    add_history(input);
                }
                free(input);
            }
        }
    } else {
        exitStatus = request(fullRequest);
    }

    std::cout << std::flush;
    return exitStatus;
}
