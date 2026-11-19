#pragma once

#include <vector>
#include <cstdlib>
#include <cmath>
#include <glad/glad.h>

namespace ParticleSystem {

struct Particle {
    float x, y, z;
    float vx, vy, vz;
    float r, g, b, a;
    float size;
    float gravity;
    float age;
    float maxAge;
};

inline std::vector<Particle>& getParticles() {
    static std::vector<Particle> particles;
    return particles;
}

inline float rand01() {
    return static_cast<float>(std::rand()) / static_cast<float>(RAND_MAX);
}

inline float randSigned() {
    return rand01() * 2.0f - 1.0f;
}

inline void spawnBlockBreak(float bx, float by, float bz, float r, float g, float b) {
    auto& ps = getParticles();
    if (ps.size() > 600) return;
    for (int i = 0; i < 12; ++i) {
        Particle p;
        p.x = bx + 0.15f + rand01() * 0.7f;
        p.y = by + 0.15f + rand01() * 0.7f;
        p.z = bz + 0.15f + rand01() * 0.7f;
        p.vx = randSigned() * 2.2f;
        p.vy = 1.5f + rand01() * 2.8f;
        p.vz = randSigned() * 2.2f;
        float shade = 0.75f + rand01() * 0.25f;
        p.r = r * shade;
        p.g = g * shade;
        p.b = b * shade;
        p.a = 1.0f;
        p.size = 0.06f + rand01() * 0.06f;
        p.gravity = 14.0f;
        p.age = 0.0f;
        p.maxAge = 0.35f + rand01() * 0.35f;
        ps.push_back(p);
    }
}

inline void spawnTorchSmoke(float x, float y, float z) {
    auto& ps = getParticles();
    if (ps.size() > 600) return;
    Particle p;
    p.x = x + randSigned() * 0.04f;
    p.y = y;
    p.z = z + randSigned() * 0.04f;
    p.vx = randSigned() * 0.12f;
    p.vy = 0.45f + rand01() * 0.35f;
    p.vz = randSigned() * 0.12f;
    bool flame = (rand01() < 0.35f);
    if (flame) {
        p.r = 1.0f; p.g = 0.65f + rand01() * 0.25f; p.b = 0.1f; p.a = 0.9f;
        p.size = 0.045f;
    } else {
        float c = 0.25f + rand01() * 0.2f;
        p.r = c; p.g = c; p.b = c; p.a = 0.65f;
        p.size = 0.055f;
    }
    p.gravity = -0.15f;
    p.age = 0.0f;
    p.maxAge = 0.45f + rand01() * 0.35f;
    ps.push_back(p);
}

inline void spawnFurnaceSmoke(float x, float y, float z) {
    auto& ps = getParticles();
    if (ps.size() > 600) return;
    Particle p;
    p.x = x + randSigned() * 0.15f;
    p.y = y;
    p.z = z + randSigned() * 0.15f;
    p.vx = randSigned() * 0.15f;
    p.vy = 0.55f + rand01() * 0.4f;
    p.vz = randSigned() * 0.15f;
    float c = 0.2f + rand01() * 0.2f;
    p.r = c; p.g = c; p.b = c; p.a = 0.7f;
    p.size = 0.07f + rand01() * 0.04f;
    p.gravity = -0.2f;
    p.age = 0.0f;
    p.maxAge = 0.6f + rand01() * 0.4f;
    ps.push_back(p);
}

inline void spawnExplosion(float x, float y, float z) {
    auto& ps = getParticles();
    for (int i = 0; i < 28; ++i) {
        Particle p;
        p.x = x + randSigned() * 0.4f;
        p.y = y + randSigned() * 0.4f;
        p.z = z + randSigned() * 0.4f;
        p.vx = randSigned() * 6.5f;
        p.vy = randSigned() * 6.5f + 1.5f;
        p.vz = randSigned() * 6.5f;
        bool fire = (rand01() < 0.45f);
        if (fire) {
            p.r = 1.0f; p.g = 0.45f + rand01() * 0.45f; p.b = 0.08f; p.a = 0.95f;
            p.size = 0.22f + rand01() * 0.18f;
        } else {
            float c = 0.35f + rand01() * 0.45f;
            p.r = c; p.g = c; p.b = c; p.a = 0.85f;
            p.size = 0.28f + rand01() * 0.22f;
        }
        p.gravity = 2.0f;
        p.age = 0.0f;
        p.maxAge = 0.45f + rand01() * 0.5f;
        ps.push_back(p);
    }
}

inline void tick(float dt) {
    auto& ps = getParticles();
    for (size_t i = 0; i < ps.size();) {
        Particle& p = ps[i];
        p.age += dt;
        if (p.age >= p.maxAge) {
            ps[i] = ps.back();
            ps.pop_back();
            continue;
        }
        p.vy -= p.gravity * dt;
        p.x += p.vx * dt;
        p.y += p.vy * dt;
        p.z += p.vz * dt;
        ++i;
    }
}

inline void render(float yawDeg, float pitchDeg) {
    auto& ps = getParticles();
    if (ps.empty()) return;

    float yaw = yawDeg * 3.14159265f / 180.0f;
    float pitch = pitchDeg * 3.14159265f / 180.0f;

    // Camera right and up vectors for billboarding
    float rx = std::cos(yaw);
    float ry = 0.0f;
    float rz = std::sin(yaw);

    float ux = -std::sin(yaw) * std::sin(pitch);
    float uy = std::cos(pitch);
    float uz = std::cos(yaw) * std::sin(pitch);

    glDisable(GL_TEXTURE_2D);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glBegin(GL_QUADS);
    for (const auto& p : ps) {
        float lifeFrac = 1.0f - (p.age / p.maxAge);
        float alpha = p.a * lifeFrac;
        float s = p.size;
        glColor4f(p.r, p.g, p.b, alpha);

        float x0 = p.x - (rx + ux) * s;
        float y0 = p.y - (ry + uy) * s;
        float z0 = p.z - (rz + uz) * s;

        float x1 = p.x + (rx - ux) * s;
        float y1 = p.y + (ry - uy) * s;
        float z1 = p.z + (rz - uz) * s;

        float x2 = p.x + (rx + ux) * s;
        float y2 = p.y + (ry + uy) * s;
        float z2 = p.z + (rz + uz) * s;

        float x3 = p.x - (rx - ux) * s;
        float y3 = p.y - (ry - uy) * s;
        float z3 = p.z - (rz - uz) * s;

        glVertex3f(x0, y0, z0);
        glVertex3f(x1, y1, z1);
        glVertex3f(x2, y2, z2);
        glVertex3f(x3, y3, z3);
    }
    glEnd();
    glDisable(GL_BLEND);
    glEnable(GL_TEXTURE_2D);
    glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
}

} // namespace ParticleSystem
