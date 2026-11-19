#pragma once

#include <vector>
#include <cmath>
#include <glad/glad.h>
#include "world/phys/Vec3.h"
#include "world/phys/AABB.h"
#include "world/level/BlockSource.h"
#include "world/level/block/Block.h"
#include "SoundEngine.h"
#include "ParticleSystem.h"

namespace ProjectileTntSystem {

struct ArrowEntity {
    float x, y, z;
    float vx, vy, vz;
    float yaw = 0.0f;
    float pitch = 0.0f;
    int damage = 4;
    bool fromPlayer = true;
    bool inGround = false;
    float age = 0.0f;
};

inline void updateArrowOrientation(ArrowEntity& a) {
    float horiz = std::sqrt(a.vx * a.vx + a.vz * a.vz);
    if (horiz > 0.0001f || std::abs(a.vy) > 0.0001f) {
        constexpr float radToDeg = 180.0f / 3.14159265f;
        a.yaw = std::atan2(a.vx, a.vz) * radToDeg;
        a.pitch = std::atan2(a.vy, horiz) * radToDeg;
    }
}

struct PrimedTntEntity {
    float x, y, z;
    float vx, vy, vz;
    float fuse = 4.0f;
};

inline std::vector<ArrowEntity>& getArrows() {
    static std::vector<ArrowEntity> arrows;
    return arrows;
}

inline std::vector<PrimedTntEntity>& getTnts() {
    static std::vector<PrimedTntEntity> tnts;
    return tnts;
}

inline void clear() {
    getArrows().clear();
    getTnts().clear();
}

inline void spawnArrow(const Vec3& pos, const Vec3& dir, float speed, int damage, bool fromPlayer) {
    float len = std::sqrt(dir.x * dir.x + dir.y * dir.y + dir.z * dir.z);
    if (len < 0.0001f) return;
    ArrowEntity a;
    a.x = pos.x;
    a.y = pos.y;
    a.z = pos.z;
    a.vx = (dir.x / len) * speed;
    a.vy = (dir.y / len) * speed;
    a.vz = (dir.z / len) * speed;
    updateArrowOrientation(a);
    a.damage = damage;
    a.fromPlayer = fromPlayer;
    a.inGround = false;
    a.age = 0.0f;
    getArrows().push_back(a);
}

inline void spawnPrimedTnt(float x, float y, float z) {
    PrimedTntEntity t;
    t.x = x;
    t.y = y;
    t.z = z;
    t.vx = ((std::rand() % 100) / 100.0f - 0.5f) * 1.2f;
    t.vy = 2.5f;
    t.vz = ((std::rand() % 100) / 100.0f - 0.5f) * 1.2f;
    t.fuse = 4.0f;
    getTnts().push_back(t);
}

inline void renderArrows() {
    auto& arrows = getArrows();
    if (arrows.empty()) return;
    glDisable(GL_TEXTURE_2D);
    glLineWidth(2.5f);
    glBegin(GL_LINES);
    for (const auto& a : arrows) {
        constexpr float degToRad = 3.14159265f / 180.0f;
        float yawRad = a.yaw * degToRad;
        float pitchRad = a.pitch * degToRad;
        float cosP = std::cos(pitchRad);
        float dx = std::sin(yawRad) * cosP;
        float dy = std::sin(pitchRad);
        float dz = std::cos(yawRad) * cosP;
        // Wooden shaft
        glColor3f(0.55f, 0.38f, 0.20f);
        glVertex3f(a.x - dx * 0.35f, a.y - dy * 0.35f, a.z - dz * 0.35f);
        glVertex3f(a.x + dx * 0.25f, a.y + dy * 0.25f, a.z + dz * 0.25f);
        // Iron tip
        glColor3f(0.82f, 0.84f, 0.88f);
        glVertex3f(a.x + dx * 0.25f, a.y + dy * 0.25f, a.z + dz * 0.25f);
        glVertex3f(a.x + dx * 0.38f, a.y + dy * 0.38f, a.z + dz * 0.38f);
    }
    glEnd();
    glLineWidth(1.0f);
    glEnable(GL_TEXTURE_2D);
    glColor3f(1.0f, 1.0f, 1.0f);
}

inline void renderTnts() {
    auto& tnts = getTnts();
    if (tnts.empty()) return;
    glDisable(GL_TEXTURE_2D);
    for (const auto& t : tnts) {
        bool flash = (static_cast<int>(t.fuse * 6.0f) % 2) == 0;
        float s = (t.fuse < 0.35f) ? (0.5f + (0.35f - t.fuse) * 0.6f) : 0.49f;
        glPushMatrix();
        glTranslatef(t.x, t.y + 0.5f, t.z);
        if (flash) {
            glColor3f(1.0f, 1.0f, 1.0f);
        } else {
            glColor3f(0.85f, 0.18f, 0.12f);
        }
        glBegin(GL_QUADS);
        // Front
        glVertex3f(-s, -s, s); glVertex3f(s, -s, s); glVertex3f(s, s, s); glVertex3f(-s, s, s);
        // Back
        glVertex3f(s, -s, -s); glVertex3f(-s, -s, -s); glVertex3f(-s, s, -s); glVertex3f(s, s, -s);
        // Left
        glVertex3f(-s, -s, -s); glVertex3f(-s, -s, s); glVertex3f(-s, s, s); glVertex3f(-s, s, -s);
        // Right
        glVertex3f(s, -s, s); glVertex3f(s, -s, -s); glVertex3f(s, s, -s); glVertex3f(s, s, s);
        // Top
        glVertex3f(-s, s, s); glVertex3f(s, s, s); glVertex3f(s, s, -s); glVertex3f(-s, s, -s);
        // Bottom
        glVertex3f(-s, -s, -s); glVertex3f(s, -s, -s); glVertex3f(s, -s, s); glVertex3f(-s, -s, s);
        glEnd();
        glPopMatrix();
    }
    glEnable(GL_TEXTURE_2D);
    glColor3f(1.0f, 1.0f, 1.0f);
}

} // namespace ProjectileTntSystem
