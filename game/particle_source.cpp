#include "particle_source.hpp"
#include "ichiaji.hpp"

#define MAX_PARTICLE_SOURCES 512
#define MAX_PARTICLE_DENSITY 128

struct Particle {
    Vec2<f32> pos;
    Vec2<f32> velocity;
    Vec2<f32> acceleration;
    f32 t_left;
};

struct ParticleGenerator {
    // i32 particle_density;
    f32 t_left;
    f32 t_particle; // How long each particle spawned lives
    f32 t_to_next_spawn;
    f32 t_between_spawns;
    Particle particles[MAX_PARTICLE_DENSITY];
};

// TODO: @robustness: I can't really think of a good way of allowing generators to have infinite particles in flight. But that might not be a problem.
static Bana::BucketArray<ParticleGenerator> particle_generators;

static void render(Ichigo::Entity *e) {
    Bana::BucketLocator bl = *((Bana::BucketLocator *) (&e->user_data_i64));
    ParticleGenerator &gen = particle_generators[bl];
    Ichigo::Texture &tex   = Ichigo::Internal::textures[e->sprite.sheet.texture];

    MAKE_STACK_ARRAY(rects, TexturedRect, ARRAY_LEN(gen.particles));

    for (usize i = 0; i < ARRAY_LEN(gen.particles); ++i) {
        if (gen.particles[i].t_left > 0.0f) {
            TexturedRect r = {
                {gen.particles[i].pos, pixels_to_metres(tex.width), pixels_to_metres(tex.height)},
                {{0, 0}, tex.width, tex.height}
            };

            rects.append(r);
        }
    }

    Ichigo::world_render_rect_list(e->col.pos, rects, e->sprite.sheet.texture);
}

static void update(Ichigo::Entity *e) {
    if (Ichiaji::program_state != Ichiaji::GAME) {
        return;
    }

    Bana::BucketLocator bl = *((Bana::BucketLocator *) (&e->user_data_i64));
    ParticleGenerator &gen = particle_generators[bl];

    i32 spawn_idx = -1;
    for (usize i = 0; i < ARRAY_LEN(gen.particles); ++i) {
        if (gen.particles[i].t_left <= 0.0f) {
            if (spawn_idx < 0) spawn_idx = i;
        } else {
            gen.particles[i].pos = 0.5f * gen.particles[i].acceleration * (Ichigo::Internal::dt * Ichigo::Internal::dt) + gen.particles[i].velocity * Ichigo::Internal::dt + gen.particles[i].pos;
            // TODO: drag? gravity?
            gen.particles[i].velocity = gen.particles[i].acceleration * Ichigo::Internal::dt + gen.particles[i].velocity;
            gen.particles[i].t_left -= Ichigo::Internal::dt;
        }
    }

    gen.t_to_next_spawn -= Ichigo::Internal::dt;

    // FIXME: @fpsdep
    if (gen.t_to_next_spawn <= 0.0f && spawn_idx >= 0) {
        gen.particles[spawn_idx].pos.x        = rand_range_f32(-0.5f, 0.5f); // TODO: Consider particle gen width/height.
        gen.particles[spawn_idx].pos.y        = rand_range_f32(-0.5f, 0.5f);
        gen.particles[spawn_idx].velocity.x   = rand_range_f32(-4.0f, 4.0f);
        gen.particles[spawn_idx].velocity.y   = rand_range_f32(-2.0f, -4.0f);
        gen.particles[spawn_idx].acceleration = {0.0f, 9.8f};
        gen.particles[spawn_idx].t_left       = gen.t_particle;

        gen.t_to_next_spawn = gen.t_between_spawns;
    }

    gen.t_left -= Ichigo::Internal::dt;

    if (gen.t_left <= 0.0f) {
        Ichigo::kill_entity_deferred(e);
    }
}

static void on_kill(Ichigo::Entity *e) {
    Bana::BucketLocator bl = *((Bana::BucketLocator *) (&e->user_data_i64));
    particle_generators.remove(bl);
}

void ParticleSource::init() {
    particle_generators = make_bucket_array<ParticleGenerator>(128, Ichigo::Internal::perm_allocator);
}

void ParticleSource::spawn(const Ichigo::EntityDescriptor &descriptor, Ichigo::TextureID texture_id, f32 t_between_spawns, f32 t_particle, f32 total_t) {
    // FIXME: @speed: we are copying a zeroed array on every spawn.
    ParticleGenerator pg = {};
    // pg.particle_density  = particle_density;
    pg.t_left            = total_t;
    pg.t_to_next_spawn   = 0.0f;
    pg.t_particle        = t_particle;
    pg.t_between_spawns  = t_between_spawns;


    Bana::BucketLocator bl = particle_generators.insert(pg);
    Ichigo::Texture &tex = Ichigo::Internal::textures[texture_id];
    Ichigo::Sprite sprite = {
        {0.0f, 0.0f},
        pixels_to_metres(tex.width),
        pixels_to_metres(tex.height),
        {tex.width, tex.height, texture_id},
        {},
        0,
        0.0f
    };

    Ichigo::Entity *e = Ichigo::spawn_entity();

    std::strcpy(e->name, "particle_source");

    e->col           = {descriptor.pos, descriptor.custom_width, descriptor.custom_height};
    e->render_proc   = render;
    e->update_proc   = update;
    e->kill_proc     = on_kill;
    e->user_data_i64 = *((i64 *) &bl);
    e->sprite        = sprite;

    SET_FLAG(e->flags, Ichigo::EF_NO_COLLIDE);
}
