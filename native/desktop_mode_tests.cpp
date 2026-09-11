#include "desktop_mode_policy.h"
#include <iostream>
int main() {
    struct Case { const char* name; RECT window; RECT monitor; bool expected; };
    const RECT primary{0, 0, 1920, 1080};
    const Case cases[] = {
        {"exact fullscreen", primary, primary, true},
        {"maximized with taskbar", {0, 0, 1920, 1040}, primary, false},
        {"ordinary window", {100, 100, 900, 700}, primary, false},
        {"one pixel rounding", {1, 1, 1919, 1079}, primary, true},
        {"not fullscreen", {2, 0, 1920, 1080}, primary, false},
        {"oversized fullscreen", {-8, -8, 1928, 1088}, primary, true},
        {"negative monitor origin", {-1920, 0, 0, 1080}, {-1920, 0, 0, 1080}, true},
        {"different monitor", {-1920, 0, 0, 1080}, primary, false},
        {"invalid monitor", {0, 0, 1920, 1080}, {0, 0, 0, 0}, false},
        {"portrait monitor", {1920, -400, 3000, 1520}, {1920, -400, 3000, 1520}, true},
    };
    int failed = 0;
    for (const auto& test : cases) {
        if (vtsfloat::CoversMonitor(test.window, test.monitor) != test.expected) {
            std::cerr << "FAIL: " << test.name << '\n'; ++failed;
        }
    }
    std::cout << "Desktop fullscreen bounds: " << (sizeof(cases) / sizeof(cases[0])) << " cases, " << failed << " failures\n";
    return failed ? 1 : 0;
}
