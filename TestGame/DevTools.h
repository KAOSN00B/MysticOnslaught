#pragma once

// MO_DEV_TOOLS is the single build-level authority for every editor, debug
// panel, cheat shortcut, and hidden entry point in the game (authoring engine
// design doc, Project 1: Build Profiles And Developer Gating).
//
// Developer-class builds (the existing Debug and Release configurations) leave
// this undefined, so it defaults to 1 and every tool stays available exactly
// as before. The PublicDemo configuration defines MO_DEV_TOOLS=0 in its
// preprocessor settings, which compiles the entry points OUT of the binary --
// the tools are unavailable in that build, not merely hidden behind a flag.
#ifndef MO_DEV_TOOLS
#define MO_DEV_TOOLS 1
#endif
