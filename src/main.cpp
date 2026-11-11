// moving_clouds: Kyubu port of coremaze's Cube-World-Moving-Clouds-Mod.
//
// Clouds in the vanilla game stand still. At 0x4B1903, while a cloud is created, EAX points to a Vector3_Int64 with
// its position (relative to the player). The hook moves it along a slowly drifting wind direction, wraps it so that
// clouds stay near the player, and raises it with the distance. It is a mid-function hook on the two instructions
// `push eax / lea ecx,[esp+0xE50]` (8 bytes), which then run unchanged.
#include "../../common.hpp"

#include <cmath>
#include <cstdlib>
#include <windows.h>

namespace {

struct Vector3Int64 { int64_t x, y, z; };

const int64_t kMaxCloudDistance = 65536LL * 1000;
float g_wind_angle = 45.0f;
int64_t g_offset_x = 0;
int64_t g_offset_y = 0;
uint32_t g_last_ms = 0;

uint32_t now_ms() {
    LARGE_INTEGER f, c;
    QueryPerformanceFrequency(&f);
    QueryPerformanceCounter(&c);
    return static_cast<uint32_t>(c.QuadPart * 1000 / f.QuadPart);
}

float to_radians(float degrees) { return degrees * 3.14159265f / 180.0f; }

void KYUBU_CALL adjust_cloud(KyubuCpuContext* ctx, void*) {
    Vector3Int64* cloud = reinterpret_cast<Vector3Int64*>(ctx->eax);
    if (!cloud) return;

    const uint32_t now = now_ms();
    const uint32_t delta = now - g_last_ms;
    g_last_ms = now;
    g_offset_x += static_cast<int64_t>(static_cast<float>(delta * 500u) * std::cos(to_radians(g_wind_angle)));
    g_offset_y += static_cast<int64_t>(static_cast<float>(delta * 500u) * std::sin(to_radians(g_wind_angle)));

    const int64_t d = static_cast<int64_t>(std::sqrt(static_cast<double>(cloud->x) * cloud->x + static_cast<double>(cloud->y) * cloud->y));
    cloud->z += d / 100;
    cloud->x += g_offset_x;
    cloud->y += g_offset_y;

    // Cloud coordinates are already relative to the player: keep each cloud within +-1000 blocks of it.
    cloud->x = (cloud->x % (kMaxCloudDistance * 2)) - kMaxCloudDistance;
    cloud->y = (cloud->y % (kMaxCloudDistance * 2)) - kMaxCloudDistance;

    // The wind direction wanders a little over time.
    g_wind_angle += (static_cast<float>(std::rand()) / static_cast<float>(RAND_MAX)) * static_cast<float>(delta) / 1000.0f;
}

}  // namespace

struct MovingClouds {
    explicit MovingClouds(kyubu::Mod& m) {
        const uintptr_t site = classic::at(m, 0xB1903);
        static const uint8_t want[] = {0x50, 0x8D, 0x8C, 0x24, 0x50, 0x0E, 0x00, 0x00};
        if (!classic::expect(m, site, want, sizeof want, "moving_clouds")) return;
        g_last_ms = now_ms();
        KyubuId id = 0;
        const KyubuResult r = m.api()->hook_mid(m.handle(), site, &adjust_cloud, nullptr, &id);
        if (r != KY_OK) { m.error("moving_clouds: could not hook cloud creation (result %d)", static_cast<int>(r)); return; }
        m.info("moving_clouds: clouds drift with the wind");
    }
};

KYUBU_MOD("moving_clouds", KY_SIDE_CLIENT, MovingClouds)
