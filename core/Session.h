#pragma once
#include "CopyCatalog.h"
#include "Protocol.h"

struct Transition { SessionState state; std::optional<SessionEvent> event; QString error; };
Transition applyCommand(const SessionState& state, const PeerId& actor, const Command& command,
                        CopyCatalog& copy, const std::function<int(int)>& choose);
SessionState applyPresence(const SessionState& state, const PeerId& peer, bool reachable);
double projectX(double x, double width, double petWidth, double margin);
