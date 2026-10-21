#pragma once

#include "GameTypes.h"
#include "raylib.h"

// How a decision room presents itself: intro banner, shrine prompt and accent
// colour (shrine glow, minimap mark, card title). Implemented in
// EngineRoomEvents.cpp next to each event's options.
struct RoomEventPresentation
{
    const char* title;       // intro banner and card screen title ("OMEN STONE")
    const char* bannerSub;   // intro banner second line
    const char* prompt;      // "[E] <prompt>" over the shrine
    Color       color;
};

const RoomEventPresentation& GetRoomEventPresentation(RoomSpecialType type);

// How many authored options an event draws its three cards from (an event with
// more than three shows a seeded three of them).
int RoomEventOfferCount(RoomSpecialType type);
