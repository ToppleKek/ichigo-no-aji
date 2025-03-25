/*
    Ichigo! A simple, from scratch, minimal dependency game engine for 2D side scrolling games.

    Commonly used types and macros.

    Author:      Braeden Hong
    Last edited: 2025/03/24
*/

#pragma once

#include "bana.hpp"

#define BEGIN_TIMED_BLOCK(NAME) f64 NAME##_TIMED_BLOCK = Ichigo::Internal::platform_get_current_time()
#define END_TIMED_BLOCK(NAME)   ICHIGO_INFO("Timed block \"" #NAME "\" took %fms!", (Ichigo::Internal::platform_get_current_time() - NAME##_TIMED_BLOCK) * 1000.0)

#ifndef ICHIGO_DEBUG
#undef BEGIN_TIMED_BLOCK
#undef END_TIMED_BLOCK
#define BEGIN_TIMED_BLOCK(...)
#define END_TIMED_BLOCK(...)

#undef ICHIGO_INFO
#undef ICHIGO_ERROR
#undef VK_ASSERT_OK
#define ICHIGO_INFO(...)
#define ICHIGO_ERROR(...)
#define VK_ASSERT_OK(...)
#endif

// TODO: Make these configurable?
#define PIXELS_PER_METRE 32
#define AUDIO_SAMPLE_RATE 44100
#define AUDIO_CHANNEL_COUNT 2
