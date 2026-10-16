#pragma once
#include "Session.h"

inline SessionState connectedSession()
{
    SessionState state;
    state.epoch = "epoch-1";
    PeerState a; a.id = "a"; a.reachable = true; a.x = 0.25;
    PeerState b; b.id = "b"; b.reachable = true; b.x = 0.75;
    state.peers.insert(a.id, a);
    state.peers.insert(b.id, b);
    return state;
}
inline Command command(Action action, QString target = "b", QString text = {}, bool busy = false)
{
    return {"request-1", action, target, text, busy};
}
inline int chooseFirst(int) { return 0; }
