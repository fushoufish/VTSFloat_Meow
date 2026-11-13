#include "updater.h"

#include <iostream>
#include <stdexcept>
#include <vector>

using namespace vtsfloat::update;

namespace {

void Expect(bool condition, const char* description) {
    if (!condition) throw std::runtime_error(description);
}

const std::string kRelease = R"json({
    "tag_name":"v1.3","draft":false,"prerelease":false,
    "assets":[
        {"name":"readme.txt"},
        {"name":"VTSFloat_Meow.exe","state":"uploaded","size":2082816,
         "digest":"sha256:fd7f68930ce8e2bdbac0c9d53d0bf7e3f1fde4630feaa9bc9181c158b007b7f6",
         "browser_download_url":"https://github.com/fushoufish/VTSFloat_Meow/releases/download/v1.3/VTSFloat_Meow.exe"}
    ]
})json";

void TestVersions() {
    Expect(ParseVersion(L"1.2.2").value() < ParseVersion(L"1.3").value(), "1.2.2 < 1.3");
    Expect(ParseVersion(L"v1.3") == ParseVersion(L"1.3.0"), "missing patch is zero");
    Expect(ParseVersion(L"1.3.2").value() > ParseVersion(L"1.3").value(), "1.3.2 > 1.3");
    Expect(ParseVersion(L"1.2.10").value() > ParseVersion(L"1.2.9").value(), "numeric patch");
    for (const auto* invalid : {L"", L"1", L"1.2.", L"1..2", L"v-1.2",
                                L"1.2.3.4", L"65536.0", L"1.2.3-rc1", L"ae5f65f"}) {
        Expect(!ParseVersion(invalid), "reject invalid or prerelease version");
    }
}

void TestReleaseAndRoutes() {
    const auto release = ParseRelease(kRelease);
    Expect(release && release->version == Version{1, 3, 0} &&
        release->size == 2082816, "parse stable Release attachment");
    Expect(!ParseRelease("<html>Rate limited</html>"), "reject error page");
    auto otherRepo = kRelease;
    otherRepo.replace(otherRepo.find("fushoufish"), 10, "another");
    Expect(!ParseRelease(otherRepo), "reject different download repository");
    auto draft = kRelease;
    draft.replace(draft.find("false"), 5, "true");
    Expect(!ParseRelease(draft), "reject draft Release");

    std::atomic<bool> cancelled{false};
    std::vector<bool> calls;
    auto result = LookupLatest([&](bool proxy) {
        calls.push_back(proxy);
        return kRelease;
    }, cancelled);
    Expect(result.release && !result.proxy && calls == std::vector<bool>{false},
           "stop after valid direct response");

    calls.clear();
    result = LookupLatest([&](bool proxy) {
        calls.push_back(proxy);
        if (!proxy) throw std::runtime_error("connection timeout");
        return kRelease;
    }, cancelled);
    Expect(result.release && result.proxy && calls == std::vector<bool>{false, true},
           "one proxy attempt after direct failure");

    calls.clear();
    result = LookupLatest([&](bool proxy) {
        calls.push_back(proxy);
        return std::string("<html>unavailable</html>");
    }, cancelled);
    Expect(!result.release && calls == std::vector<bool>{false, true},
           "both invalid responses exhaust the two routes");
    Expect(RouteUrl(release->url, true) == L"https://gh-proxy.com/" + release->url,
           "proxy preserves exact release asset URL");
}

} // namespace

int main() {
    try {
        TestVersions();
        TestReleaseAndRoutes();
        std::cout << "Updater version and route checks passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
