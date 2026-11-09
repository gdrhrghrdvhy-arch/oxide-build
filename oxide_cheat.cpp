// ================================================================
//  OXIDE: SURVIVAL ISLAND - ESP + AIMBOT + WALLHACK + IMGUI MENU
//  Версия offsets: 1.13.11940 (ARM64)
//  Сборка: Android NDK (arm64-v8a)
// ================================================================

#include <jni.h>
#include <android/log.h>
#include <dlfcn.h>
#include <pthread.h>
#include <unistd.h>
#include <cstring>
#include <cstdio>
#include <cstdint>
#include <cmath>
#include <atomic>
#include <vector>
#include <string>

// ImGui (упрощённое подключение; в реальном проекте нужен полный фреймворк)
#include "imgui.h"
#include "imgui_impl_android.h"

#define LOG_TAG "OxideCheat"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)

typedef uint64_t u64;
typedef uint32_t u32;
typedef float f32;

// ===== СМЕЩЕНИЯ (ВЕРСИЯ 1.13.11940) =====
struct Offsets {
    u64 gworld = 0x693f904;          // Player shit (базовый указатель)
    u64 player_update = 0x6938128;   // player update
    u64 player_destroy = 0xbfd8d94;  // player destroy
    u64 ulevel = 0x30;
    u64 actors = 0x20;
    u64 actor_count = 0x28;
    u64 local_player = 0x28;
    u64 player_controller = 0x38;
    u64 pawn = 0x4A8;
    u64 health = 0x1A0;
    u64 team_id = 0x1F4;
    u64 position = 0x150;
    u64 root_component = 0x1D0;
    u64 last_render_time = 0x360;
    u64 camera_manager = 0x458;
    u64 camera_cache = 0x1B80;
    u64 mesh = 0x3D0;
};
Offsets off;

// ===== НАСТРОЙКИ =====
std::atomic<bool> g_esp{true};
std::atomic<bool> g_wallhack{true};
std::atomic<bool> g_aimbot{true};
std::atomic<float> g_smooth{0.15f};
std::atomic<float> g_fov{30.0f};

// ===== ПАМЯТЬ =====
int mem_fd = -1;
bool init_mem() {
    mem_fd = open("/proc/self/mem", O_RDWR);
    return mem_fd >= 0;
}
template<typename T> T read_mem(u64 addr) {
    T v = T(0);
    if (mem_fd < 0 || !addr) return v;
    pread(mem_fd, &v, sizeof(T), (off_t)addr);
    return v;
}
template<typename T> void write_mem(u64 addr, T v) {
    if (mem_fd < 0 || !addr) return;
    pwrite(mem_fd, &v, sizeof(T), (off_t)addr);
}

// ===== БАЗА libUE4.so =====
u64 get_base(const char* name = "libUE4.so") {
    FILE* f = fopen("/proc/self/maps", "r");
    if (!f) return 0;
    char line[1024]; u64 b = 0;
    while (fgets(line, sizeof(line), f))
        if (strstr(line, name)) { b = strtoull(line, nullptr, 16); break; }
    fclose(f);
    return b;
}

// ===== ЛОКАЛЬНЫЙ ИГРОК =====
u64 get_local_pawn(u64 gworld) {
    if (!gworld) return 0;
    u64 gi = read_mem<u64>(gworld + 0x38);
    if (!gi) return 0;
    u64 lp = read_mem<u64>(gi + off.local_player);
    if (!lp) return 0;
    u64 pc = read_mem<u64>(lp + off.player_controller);
    if (!pc) return 0;
    return read_mem<u64>(pc + off.pawn);
}

// ===== СПИСОК АКТОРОВ =====
std::vector<u64> get_actors(u64 gworld) {
    std::vector<u64> v;
    if (!gworld) return v;
    u64 ul = read_mem<u64>(gworld + off.ulevel);
    if (!ul) return v;
    u64 arr = read_mem<u64>(ul + off.actors);
    u32 n = read_mem<u32>(ul + off.actor_count);
    if (!arr || n > 1000) return v;
    for (u32 i = 0; i < n; i++) {
        u64 a = read_mem<u64>(arr + i * 8);
        if (a) v.push_back(a);
    }
    return v;
}

// ===== AIMBOT =====
void do_aimbot(u64 gworld, u64 local_pawn) {
    if (!g_aimbot.load() || !local_pawn) return;
    auto actors = get_actors(gworld);
    u32 my_team = read_mem<u32>(local_pawn + off.team_id);
    u64 best = 0; float best_dist = 9999;
    for (u64 a : actors) {
        if (a == local_pawn) continue;
        f32 hp = read_mem<f32>(a + off.health);
        if (hp <= 0 || hp > 1000) continue;
        u32 team = read_mem<u32>(a + off.team_id);
        if (team == my_team) continue;
        u64 root = read_mem<u64>(a + off.root_component);
        if (!root) continue;
        f32 x = read_mem<f32>(root + off.position);
        f32 y = read_mem<f32>(root + off.position + 4);
        f32 z = read_mem<f32>(root + off.position + 8);
        u64 cam = read_mem<u64>(local_pawn + off.camera_manager);
        if (!cam) continue;
        u64 pov = cam + off.camera_cache;
        f32 cx = read_mem<f32>(pov), cy = read_mem<f32>(pov + 4), cz = read_mem<f32>(pov + 8);
        f32 dx = x - cx, dy = y - cy, dz = z - cz;
        f32 dist = sqrtf(dx*dx + dy*dy + dz*dz);
        if (dist < best_dist) { best_dist = dist; best = a; }
    }
    if (best) {
        u64 cam = read_mem<u64>(local_pawn + off.camera_manager);
        if (cam) {
            u64 pov = cam + off.camera_cache;
            u64 root = read_mem<u64>(best + off.root_component);
            if (root) {
                f32 x = read_mem<f32>(root + off.position);
                f32 y = read_mem<f32>(root + off.position + 4);
                f32 z = read_mem<f32>(root + off.position + 8);
                f32 cx = read_mem<f32>(pov), cy = read_mem<f32>(pov + 4), cz = read_mem<f32>(pov + 8);
                f32 dx = x - cx, dy = y - cy, dz = z - cz;
                f32 dist = sqrtf(dx*dx + dy*dy + dz*dz);
                if (dist > 1.0f) {
                    f32 pitch = asinf(dz / dist) * 57.2958f;
                    f32 yaw = atan2f(dy, dx) * 57.2958f;
                    f32 cur_p = read_mem<f32>(pov + 0x30);
                    f32 cur_y = read_mem<f32>(pov + 0x34);
                    f32 s = g_smooth.load();
                    write_mem<f32>(pov + 0x30, cur_p + (pitch - cur_p) * s);
                    write_mem<f32>(pov + 0x34, cur_y + (yaw - cur_y) * s);
                }
            }
        }
    }
}

// ===== ESP + WALLHACK =====
void do_esp(u64 gworld, u64 local_pawn) {
    if (!g_esp.load() && !g_wallhack.load()) return;
    auto actors = get_actors(gworld);
    u32 my_team = read_mem<u32>(local_pawn + off.team_id);
    for (u64 a : actors) {
        if (a == local_pawn) continue;
        f32 hp = read_mem<f32>(a + off.health);
        if (hp <= 0 || hp > 1000) continue;
        u32 team = read_mem<u32>(a + off.team_id);
        if (team == my_team) continue;
        if (g_wallhack.load()) write_mem<f32>(a + off.last_render_time, 999.0f);
        if (g_esp.load()) {
            u64 root = read_mem<u64>(a + off.root_component);
            if (root) {
                f32 x = read_mem<f32>(root + off.position);
                f32 y = read_mem<f32>(root + off.position + 4);
                f32 z = read_mem<f32>(root + off.position + 8);
                LOGI("ESP: HP=%.0f X=%.1f Y=%.1f Z=%.1f", hp, x, y, z);
            }
        }
    }
}

// ===== ПОТОК ЧИТА =====
void* cheat_thread(void*) {
    u64 base = get_base();
    if (!base) { LOGE("libUE4.so not found"); return nullptr; }
    LOGI("libUE4.so base: 0x%llX", (unsigned long long)base);
    while (true) {
        u64 gworld = read_mem<u64>(base + off.gworld);
        if (gworld) {
            u64 pawn = get_local_pawn(gworld);
            if (pawn) {
                do_aimbot(gworld, pawn);
                do_esp(gworld, pawn);
            }
        }
        usleep(16000);
    }
    return nullptr;
}

// ===== IMGUI MENU (УПРОЩЁННО) =====
void render_menu() {
    ImGui::Begin("Oxide Cheat", nullptr, ImGuiWindowFlags_AlwaysAutoResize);
    ImGui::Checkbox("ESP", (bool*)&g_esp);
    ImGui::Checkbox("Wallhack", (bool*)&g_wallhack);
    ImGui::Checkbox("Aimbot", (bool*)&g_aimbot);
    ImGui::SliderFloat("Smooth", (float*)&g_smooth, 0.01f, 1.0f);
    ImGui::SliderFloat("FOV", (float*)&g_fov, 5.0f, 180.0f);
    ImGui::End();
}

// ===== СТАРТ =====
extern "C" JNIEXPORT void JNICALL
Java_com_oxide_cheat_MainActivity_startCheat(JNIEnv*, jobject) {
    if (!init_mem()) { LOGE("mem init failed"); return; }
    pthread_t t;
    pthread_create(&t, nullptr, cheat_thread, nullptr);
    pthread_detach(t);
    LOGI("Cheat started");
}