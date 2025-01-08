/*
    Ichigo! A simple, from scratch, minimal dependency game engine for 2D side scrolling games.

    Commonly used types and macros.

    Author:      Braeden Hong
    Last edited: 2024/12/1
*/

#pragma once

#include "bana.hpp"

#ifndef ICHIGO_DEBUG
#define ICHIGO_INFO(...)
#define ICHIGO_ERROR(...)
#define VK_ASSERT_OK(...)
#endif

// TODO: Make these configurable?
#define SCREEN_TILE_WIDTH 16
#define SCREEN_TILE_HEIGHT 9
#define PIXELS_PER_METRE 32
#define AUDIO_SAMPLE_RATE 44100
#define AUDIO_CHANNEL_COUNT 2
