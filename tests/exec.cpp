#include "core.hpp"

int main() {
    const char* home = ::getenv("HOME");
    if (home == nullptr) {
        cm::os::exec("echo", {"how", "?"});
    }
    // replace current process with this
    cm::os::exec("kitty", {home});
    ::exit(1);
}
