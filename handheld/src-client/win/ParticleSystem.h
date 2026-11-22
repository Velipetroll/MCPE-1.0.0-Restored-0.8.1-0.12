#pragma once

#include <vector>
#include <cstdlib>
#include <cmath>
#include <algorithm>
#include <glad/glad.h>
#include "client/gui/GuiComponent.h"
#include "TextureGroup.h"
#include "world/level/BlockPos.h"
#include "world/level/BlockSource.h"
#include "util/Math.h"

namespace ParticleSystem {

enum class ParticleType {
    Terrain,
    Flame,
    Smoke,
    Explode,
    Heart
};

struct Particle {
    ParticleType type = ParticleType::Terrain;
    float x = 0.0f, y = 0.0f, z = 0.0f;
    float prevX = 0.0f, prevY = 0.0f, prevZ = 0.0f;
    float vx = 0.0f, vy = 0.0f, vz = 0.0f; // blocks per 20Hz tick
    float r = 1.0f, g = 1.0f, b = 1.0f, a = 1.0f;
    float scale = 1.0f;     // MCPE _scale (rendered half-size is renderScale * 0.1f)
    float initScale = 1.0f; // MCPE field_164
    float gravity = 0.0f;
    int ticksAlive = 0;
    int maxAliveTime = 20;
    bool noclip = false;
    bool onGround = false;
    float u0 = 0.0f, v0 = 0.0f, u1 = 1.0f, v1 = 1.0f;
    bool hasTexture = false;
};

inline std::vector<Particle>& getParticles() {
    static std::vector<Particle> particles;
    return particles;
}

inline float& getTickAccum() {
    static float accum = 0.0f;
    return accum;
}

inline float rand01() {
    return static_cast<float>(std::rand()) / static_cast<float>(RAND_MAX);
}

inline float randSigned() {
    return rand01() * 2.0f - 1.0f;
}

inline void moveParticle(Particle& p, float dx, float dy, float dz, BlockSource* region, bool& hitCeilingOrFloor) {
    hitCeilingOrFloor = false;
    if (p.noclip || !region) {
        p.x += dx;
        p.y += dy;
        p.z += dz;
        p.onGround = false;
        return;
    }

    const float halfSize = 0.1f;

    // Y axis
    float nextY = p.y + dy;
    int checkY = Math::floor(nextY + (dy < 0.0f ? -halfSize : halfSize));
    if (region->isSolidBlockingBlock(BlockPos(Math::floor(p.x), checkY, Math::floor(p.z)))) {
        if (dy < 0.0f) p.onGround = true;
        p.vy = 0.0f;
        hitCeilingOrFloor = true;
    } else {
        p.onGround = false;
        p.y = nextY;
    }

    // X axis
    float nextX = p.x + dx;
    int checkX = Math::floor(nextX + (dx < 0.0f ? -halfSize : halfSize));
    if (region->isSolidBlockingBlock(BlockPos(checkX, Math::floor(p.y), Math::floor(p.z)))) {
        p.vx = 0.0f;
    } else {
        p.x = nextX;
    }

    // Z axis
    float nextZ = p.z + dz;
    int checkZ = Math::floor(nextZ + (dz < 0.0f ? -halfSize : halfSize));
    if (region->isSolidBlockingBlock(BlockPos(Math::floor(p.x), Math::floor(p.y), checkZ))) {
        p.vz = 0.0f;
    } else {
        p.z = nextZ;
    }
}

inline bool stepParticle(Particle& p, BlockSource* region) {
    p.prevX = p.x;
    p.prevY = p.y;
    p.prevZ = p.z;
    if (p.ticksAlive++ >= p.maxAliveTime) {
        return false;
    }

    if (p.type == ParticleType::Terrain) {
        p.vy -= p.gravity * 0.04f;
    } else if (p.type == ParticleType::Smoke || p.type == ParticleType::Explode) {
        p.vy += 0.004f;
    }

    bool hitY = false;
    moveParticle(p, p.vx, p.vy, p.vz, region, hitY);

    if ((p.type == ParticleType::Smoke || p.type == ParticleType::Heart) && hitY) {
        p.vx *= 1.1f;
        p.vz *= 1.1f;
    }

    float drag = 0.98f;
    if (p.type == ParticleType::Flame || p.type == ParticleType::Smoke) {
        drag = 0.96f;
    } else if (p.type == ParticleType::Explode) {
        drag = 0.9f;
    } else if (p.type == ParticleType::Heart) {
        drag = 0.86f;
    }

    p.vx *= drag;
    p.vy *= drag;
    p.vz *= drag;

    if (p.onGround) {
        p.vx *= 0.7f;
        p.vz *= 0.7f;
    }
    return true;
}

inline void initParticleBase(Particle& p, float xPos, float yPos, float zPos, float motX, float motY, float motZ) {
    p.x = p.prevX = xPos;
    p.y = p.prevY = yPos;
    p.z = p.prevZ = zPos;
    p.r = p.g = p.b = p.a = 1.0f;
    p.gravity = 0.0f;
    p.ticksAlive = 0;
    p.noclip = false;
    p.onGround = false;

    p.vx = motX + (rand01() * 2.0f - 1.0f) * 0.4f;
    p.vy = motY + (rand01() * 2.0f - 1.0f) * 0.4f;
    p.vz = motZ + (rand01() * 2.0f - 1.0f) * 0.4f;

    float len = std::sqrt(p.vx * p.vx + p.vy * p.vy + p.vz * p.vz);
    float speed = (rand01() + rand01() + 1.0f) * 0.15f;
    float mult = (len > 1e-6f) ? ((speed * 0.4f) / len) : 0.0f;
    p.vx = p.vx * mult;
    p.vy = p.vy * mult + 0.1f;
    p.vz = p.vz * mult;

    float f = rand01();
    p.scale = (f * 0.5f + 0.5f) * 2.0f;
    p.initScale = p.scale;
    p.maxAliveTime = static_cast<int>(4.0f / (rand01() * 0.9f + 0.1f));
}

inline void spawnBlockBreak(float bx, float by, float bz, float r, float g, float b, BlockSource* region = nullptr,
                            float tu0 = 0.0f, float tv0 = 0.0f, float tu1 = 0.0f, float tv1 = 0.0f, bool hasTex = false) {
    auto& ps = getParticles();
    if (ps.size() > 800) return;
    // Matches MCPE 0.8.1 ParticleEngine::destroy (3x3x3 sub-grid, outward velocity from block center)
    for (int ix = 0; ix < 3; ++ix) {
        for (int iy = 0; iy < 3; ++iy) {
            for (int iz = 0; iz < 3; ++iz) {
                float px = bx + (static_cast<float>(ix) + 0.5f) / 3.0f;
                float py = by + (static_cast<float>(iy) + 0.5f) / 3.0f;
                float pz = bz + (static_cast<float>(iz) + 0.5f) / 3.0f;
                float motX = (px - bx - 0.5f) * 2.0f;
                float motY = (py - by - 0.5f) * 2.0f;
                float motZ = (pz - bz - 0.5f) * 2.0f;

                Particle p;
                initParticleBase(p, px, py, pz, motX, motY, motZ);
                p.type = ParticleType::Terrain;
                p.scale *= 0.5f;
                p.initScale = p.scale;
                p.gravity = 1.0f;
                float shade = 0.65f + rand01() * 0.35f;
                p.r = r * shade;
                p.g = g * shade;
                p.b = b * shade;
                p.a = 1.0f;
                if (hasTex) {
                    float du = (tu1 - tu0) * 0.25f;
                    float dv = (tv1 - tv0) * 0.25f;
                    p.u0 = tu0 + rand01() * 3.0f * du;
                    p.v0 = tv0 + rand01() * 3.0f * dv;
                    p.u1 = p.u0 + du;
                    p.v1 = p.v0 + dv;
                    p.hasTexture = true;
                }
                if (stepParticle(p, region)) {
                    ps.push_back(p);
                }
            }
        }
    }
}

inline void spawnBlockCrack(float bx, float by, float bz, int face, float r, float g, float b, BlockSource* region = nullptr,
                            float tu0 = 0.0f, float tv0 = 0.0f, float tu1 = 0.0f, float tv1 = 0.0f, bool hasTex = false) {
    auto& ps = getParticles();
    if (ps.size() > 800) return;

    float px = bx + rand01() * 0.8f + 0.1f;
    float py = by + rand01() * 0.8f + 0.1f;
    float pz = bz + rand01() * 0.8f + 0.1f;
    switch (face) {
        case 0: py = by - 0.1f; break;
        case 1: py = by + 1.1f; break;
        case 2: pz = bz - 0.1f; break;
        case 3: pz = bz + 1.1f; break;
        case 4: px = bx - 0.1f; break;
        case 5: px = bx + 1.1f; break;
        default: break;
    }

    Particle p;
    initParticleBase(p, px, py, pz, 0.0f, 0.0f, 0.0f);
    // Particle::setPower(0.2f)->scale(0.6f)
    p.vx *= 0.2f;
    p.vy = (p.vy - 0.1f) * 0.2f + 0.1f;
    p.vz *= 0.2f;
    p.scale *= 0.6f;

    p.type = ParticleType::Terrain;
    p.scale *= 0.5f;
    p.initScale = p.scale;
    p.gravity = 1.0f;
    float shade = 0.65f + rand01() * 0.35f;
    p.r = r * shade;
    p.g = g * shade;
    p.b = b * shade;
    p.a = 1.0f;
    if (hasTex) {
        float du = (tu1 - tu0) * 0.25f;
        float dv = (tv1 - tv0) * 0.25f;
        p.u0 = tu0 + rand01() * 3.0f * du;
        p.v0 = tv0 + rand01() * 3.0f * dv;
        p.u1 = p.u0 + du;
        p.v1 = p.v0 + dv;
        p.hasTexture = true;
    }
    if (stepParticle(p, region)) {
        ps.push_back(p);
    }
}

inline void spawnFlame(float x, float y, float z, float motX = 0.0f, float motY = 0.0f, float motZ = 0.0f) {
    auto& ps = getParticles();
    if (ps.size() > 800) return;
    Particle p;
    initParticleBase(p, x, y, z, 0.0f, 0.0f, 0.0f);
    p.type = ParticleType::Flame;
    p.noclip = true;
    p.gravity = 0.0f;
    p.vx = motX + p.vx * 0.01f;
    p.vy = motY + p.vy * 0.01f;
    p.vz = motZ + p.vz * 0.01f;
    p.initScale = p.scale;
    p.r = p.g = p.b = p.a = 1.0f;
    p.maxAliveTime = static_cast<int>(8.0f / (rand01() * 0.8f + 0.2f)) + 4;
    if (stepParticle(p, nullptr)) {
        ps.push_back(p);
    }
}

inline void spawnSmoke(float x, float y, float z, float motX = 0.0f, float motY = 0.0f, float motZ = 0.0f, int scale100 = 100, BlockSource* region = nullptr) {
    auto& ps = getParticles();
    if (ps.size() > 800) return;
    Particle p;
    initParticleBase(p, x, y, z, 0.0f, 0.0f, 0.0f);
    p.type = ParticleType::Smoke;
    p.noclip = false;
    float scaleMul = static_cast<float>(scale100 == 0 ? 100 : scale100) / 100.0f;
    p.vx = motX + p.vx * 0.1f;
    p.vy = motY + p.vy * 0.1f;
    p.vz = motZ + p.vz * 0.1f;
    float c = rand01() * 0.5f;
    p.r = p.g = p.b = c;
    p.a = 1.0f;
    p.scale = (p.scale * 0.75f) * scaleMul;
    p.initScale = p.scale;
    p.maxAliveTime = static_cast<int>((scaleMul * 8.0f) / (rand01() * 0.8f + 0.2f));
    if (stepParticle(p, region)) {
        ps.push_back(p);
    }
}

inline void spawnExplode(float x, float y, float z, float motX, float motY, float motZ, BlockSource* region = nullptr) {
    auto& ps = getParticles();
    if (ps.size() > 1000) return;
    Particle p;
    initParticleBase(p, x, y, z, motX, motY, motZ);
    p.type = ParticleType::Explode;
    p.noclip = false;
    p.vx = motX + (rand01() * 2.0f - 1.0f) * 0.05f;
    p.vy = motY + (rand01() * 2.0f - 1.0f) * 0.05f;
    p.vz = motZ + (rand01() * 2.0f - 1.0f) * 0.05f;
    float c = rand01() * 0.3f + 0.7f;
    p.r = p.g = p.b = c;
    p.a = 1.0f;
    float f = rand01();
    p.scale = (f * rand01() * 6.0f) + 1.0f;
    p.initScale = p.scale;
    p.maxAliveTime = static_cast<int>(16.0f / (rand01() * 0.8f + 0.2f)) + 2;
    if (stepParticle(p, region)) {
        ps.push_back(p);
    }
}

inline void spawnHeart(float x, float y, float z, float motX = 0.0f, float motY = 0.0f, float motZ = 0.0f, BlockSource* region = nullptr) {
    auto& ps = getParticles();
    if (ps.size() > 1000) return;
    Particle p;
    initParticleBase(p, x, y, z, motX, motY, motZ);
    p.type = ParticleType::Heart;
    p.noclip = false;
    p.vx *= 0.01f;
    p.vz *= 0.01f;
    p.vy = p.vy * 0.01f + 0.1f;
    p.scale *= 1.5f;
    p.initScale = p.scale;
    p.maxAliveTime = 16;
    if (stepParticle(p, region)) {
        ps.push_back(p);
    }
}

inline void spawnDeathPoof(float x, float y, float z, float halfWidth, float height, BlockSource* region = nullptr) {
    // Matches MCPE 0.8.1 Mob::baseTick() L534-554 (20 PT_EXPLODE particles inside entity AABB)
    for (int i = 0; i < 20; ++i) {
        float vx = randSigned() * randSigned() * 0.02f;
        float vy = randSigned() * randSigned() * 0.02f;
        float vz = randSigned() * randSigned() * 0.02f;
        float px = x + (rand01() * 2.0f - 1.0f) * halfWidth;
        float py = y + rand01() * height;
        float pz = z + (rand01() * 2.0f - 1.0f) * halfWidth;
        spawnExplode(px, py, pz, vx, vy, vz, region);
    }
}

inline void spawnTorchSmoke(float x, float y, float z, BlockSource* region = nullptr) {
    spawnSmoke(x, y, z, 0.0f, 0.0f, 0.0f, 100, region);
    spawnFlame(x, y, z, 0.0f, 0.0f, 0.0f);
}

inline void spawnRedDust(float x, float y, float z, BlockSource* region = nullptr) {
    auto& ps = getParticles();
    if (ps.size() > 800) return;
    Particle p;
    initParticleBase(p, x, y, z, 0.0f, 0.0f, 0.0f);
    p.type = ParticleType::Smoke;
    p.noclip = false;
    p.vx *= 0.1f;
    p.vy *= 0.1f;
    p.vz *= 0.1f;
    p.r = rand01() * 0.4f + 0.6f;
    p.g = rand01() * 0.05f;
    p.b = rand01() * 0.05f;
    p.a = 1.0f;
    p.scale *= 0.75f;
    p.initScale = p.scale;
    p.maxAliveTime = static_cast<int>(8.0f / (rand01() * 0.8f + 0.2f));
    if (stepParticle(p, region)) {
        ps.push_back(p);
    }
}

inline void spawnFurnaceSmoke(float x, float y, float z, BlockSource* region = nullptr) {
    spawnSmoke(x, y, z, 0.0f, 0.0f, 0.0f, 100, region);
    spawnFlame(x, y, z, 0.0f, 0.0f, 0.0f);
}

inline void spawnExplosion(float x, float y, float z, float radius = 3.5f, BlockSource* region = nullptr) {
    // Matches MCPE 0.8.1 Explosion::finalizeExplosion radial outward velocity
    for (int i = 0; i < 24; ++i) {
        float ox = randSigned() * radius * 0.75f;
        float oy = randSigned() * radius * 0.75f;
        float oz = randSigned() * radius * 0.75f;
        float px = x + ox;
        float py = y + oy;
        float pz = z + oz;
        float dist = std::sqrt(ox * ox + oy * oy + oz * oz);
        if (dist < 0.1f) dist = 0.1f;
        float invDist = 1.0f / dist;
        float nx = ox * invDist;
        float ny = oy * invDist;
        float nz = oz * invDist;
        float speed = (0.5f / (dist / radius + 0.1f)) * (rand01() * rand01() + 0.3f);
        float vx = nx * speed;
        float vy = ny * speed;
        float vz = nz * speed;
        spawnExplode((px + x) * 0.5f, (py + y) * 0.5f, (pz + z) * 0.5f, vx, vy, vz, region);
        spawnSmoke(px, py, pz, vx, vy, vz, 100, region);
    }
}

inline void tick(float dt, BlockSource* region = nullptr) {
    constexpr float TICK_DT = 0.05f;
    float& accum = getTickAccum();
    accum += dt;
    if (accum > 0.25f) accum = 0.25f;

    auto& ps = getParticles();
    while (accum >= TICK_DT) {
        accum -= TICK_DT;
        for (size_t i = 0; i < ps.size();) {
            if (!stepParticle(ps[i], region)) {
                ps[i] = ps.back();
                ps.pop_back();
            } else {
                ++i;
            }
        }
    }
}

inline void render(float yawDeg, float pitchDeg, mce::TextureGroup* texGroup = nullptr) {
    auto& ps = getParticles();
    if (ps.empty()) return;

    constexpr float DEG_TO_RAD = 3.14159265f / 180.0f;
    float yaw = yawDeg * DEG_TO_RAD;
    float pitch = pitchDeg * DEG_TO_RAD;

    // Camera basis matching renderer.cpp:
    // front = (cos(yaw)*cos(pitch), sin(pitch), sin(yaw)*cos(pitch))
    // right = normalize(cross(front, (0,1,0))) = (-sin(yaw), 0, cos(yaw))
    // up    = cross(right, front) = (-cos(yaw)*sin(pitch), cos(pitch), -sin(yaw)*sin(pitch))
    float rx = -std::sin(yaw);
    float ry = 0.0f;
    float rz = std::cos(yaw);

    float ux = -std::cos(yaw) * std::sin(pitch);
    float uy = std::cos(pitch);
    float uz = -std::sin(yaw) * std::sin(pitch);

    float partialTicks = getTickAccum() / 0.05f;
    if (partialTicks < 0.0f) partialTicks = 0.0f;
    if (partialTicks > 1.0f) partialTicks = 1.0f;

    glEnable(GL_ALPHA_TEST);
    glAlphaFunc(GL_GREATER, 0.1f);
    glDisable(GL_BLEND);

    // Pass 1: Textured particles (Flame, Smoke, Explode) using textures/particle/particles.png
    if (GuiComponent::bindTexture("textures/particle/particles.png")) {
        glBegin(GL_QUADS);
        for (const auto& p : ps) {
            if (p.type == ParticleType::Terrain) continue;

            float renderScale = p.scale;
            float u0 = 0.0f, v0 = 0.0f, u1 = 0.0625f, v1 = 0.0625f;

            if (p.type == ParticleType::Flame) {
                float frac = (static_cast<float>(p.ticksAlive) + partialTicks) / static_cast<float>(std::max(1, p.maxAliveTime));
                renderScale = p.initScale * (1.0f - frac * frac * 0.5f);
                u0 = 0.0f;
                v0 = 3.0f / 16.0f;
                u1 = 1.0f / 16.0f;
                v1 = 4.0f / 16.0f;
            } else if (p.type == ParticleType::Smoke) {
                float frac = ((static_cast<float>(p.ticksAlive) + partialTicks) / static_cast<float>(std::max(1, p.maxAliveTime))) * 32.0f;
                frac = std::max(0.0f, std::min(1.0f, frac));
                renderScale = p.initScale * frac;
                int frame = 7 - (8 * p.ticksAlive / std::max(1, p.maxAliveTime));
                frame = std::max(0, std::min(7, frame));
                u0 = static_cast<float>(frame) / 16.0f;
                u1 = static_cast<float>(frame + 1) / 16.0f;
                v0 = 0.0f;
                v1 = 1.0f / 16.0f;
            } else if (p.type == ParticleType::Explode) {
                int frame = 7 - (8 * p.ticksAlive / std::max(1, p.maxAliveTime));
                frame = std::max(0, std::min(7, frame));
                u0 = static_cast<float>(frame) / 16.0f;
                u1 = static_cast<float>(frame + 1) / 16.0f;
                v0 = 0.0f;
                v1 = 1.0f / 16.0f;
            } else if (p.type == ParticleType::Heart) {
                float frac = ((static_cast<float>(p.ticksAlive) + partialTicks) / static_cast<float>(std::max(1, p.maxAliveTime))) * 32.0f;
                frac = std::max(0.0f, std::min(1.0f, frac));
                renderScale = p.initScale * frac;
                u0 = 0.0f;
                u1 = 1.0f / 16.0f;
                v0 = 5.0f / 16.0f;
                v1 = 6.0f / 16.0f;
            }

            float px = p.prevX + (p.x - p.prevX) * partialTicks;
            float py = p.prevY + (p.y - p.prevY) * partialTicks;
            float pz = p.prevZ + (p.z - p.prevZ) * partialTicks;
            float s = renderScale * 0.1f;

            glColor4f(p.r, p.g, p.b, 1.0f);

            float x0 = px - (rx + ux) * s, y0 = py - (ry + uy) * s, z0 = pz - (rz + uz) * s;
            float x1 = px + (rx - ux) * s, y1 = py + (ry - uy) * s, z1 = pz + (rz - uz) * s;
            float x2 = px + (rx + ux) * s, y2 = py + (ry + uy) * s, z2 = pz + (rz + uz) * s;
            float x3 = px - (rx - ux) * s, y3 = py - (ry - uy) * s, z3 = pz - (rz - uz) * s;

            glTexCoord2f(u0, v1); glVertex3f(x0, y0, z0);
            glTexCoord2f(u1, v1); glVertex3f(x1, y1, z1);
            glTexCoord2f(u1, v0); glVertex3f(x2, y2, z2);
            glTexCoord2f(u0, v0); glVertex3f(x3, y3, z3);
        }
        glEnd();
    }

    // Pass 2: Terrain particles (textured from atlas.terrain with alpha test/blend, or colored fallback)
    bool hasAtlas = false;
    if (texGroup) {
        auto atlasTex = texGroup->getTexture(ResourceLocation("atlas.terrain"));
        if (atlasTex.get()) {
            atlasTex.get()->bindTexture(mce::RenderContextImmediate::get(), 0, 0);
            hasAtlas = true;
        }
    }
    if (hasAtlas) {
        glEnable(GL_TEXTURE_2D);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glBegin(GL_QUADS);
        for (const auto& p : ps) {
            if (p.type != ParticleType::Terrain || !p.hasTexture) continue;
            float px = p.prevX + (p.x - p.prevX) * partialTicks;
            float py = p.prevY + (p.y - p.prevY) * partialTicks;
            float pz = p.prevZ + (p.z - p.prevZ) * partialTicks;
            float s = p.scale * 0.1f;

            glColor4f(p.r, p.g, p.b, 1.0f);

            float x0 = px - (rx + ux) * s, y0 = py - (ry + uy) * s, z0 = pz - (rz + uz) * s;
            float x1 = px + (rx - ux) * s, y1 = py + (ry - uy) * s, z1 = pz + (rz - uz) * s;
            float x2 = px + (rx + ux) * s, y2 = py + (ry + uy) * s, z2 = pz + (rz + uz) * s;
            float x3 = px - (rx - ux) * s, y3 = py - (ry - uy) * s, z3 = pz - (rz - uz) * s;

            glTexCoord2f(p.u0, p.v1); glVertex3f(x0, y0, z0);
            glTexCoord2f(p.u1, p.v1); glVertex3f(x1, y1, z1);
            glTexCoord2f(p.u1, p.v0); glVertex3f(x2, y2, z2);
            glTexCoord2f(p.u0, p.v0); glVertex3f(x3, y3, z3);
        }
        glEnd();
        glDisable(GL_BLEND);
    }

    glDisable(GL_TEXTURE_2D);
    glBegin(GL_QUADS);
    for (const auto& p : ps) {
        if (p.type != ParticleType::Terrain || (hasAtlas && p.hasTexture)) continue;
        float px = p.prevX + (p.x - p.prevX) * partialTicks;
        float py = p.prevY + (p.y - p.prevY) * partialTicks;
        float pz = p.prevZ + (p.z - p.prevZ) * partialTicks;
        float s = p.scale * 0.1f;

        glColor4f(p.r, p.g, p.b, 1.0f);

        float x0 = px - (rx + ux) * s, y0 = py - (ry + uy) * s, z0 = pz - (rz + uz) * s;
        float x1 = px + (rx - ux) * s, y1 = py + (ry - uy) * s, z1 = pz + (rz - uz) * s;
        float x2 = px + (rx + ux) * s, y2 = py + (ry + uy) * s, z2 = pz + (rz + uz) * s;
        float x3 = px - (rx - ux) * s, y3 = py - (ry - uy) * s, z3 = pz - (rz - uz) * s;

        glVertex3f(x0, y0, z0);
        glVertex3f(x1, y1, z1);
        glVertex3f(x2, y2, z2);
        glVertex3f(x3, y3, z3);
    }
    glEnd();

    glDisable(GL_ALPHA_TEST);
    glEnable(GL_TEXTURE_2D);
    glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
}

} // namespace ParticleSystem
