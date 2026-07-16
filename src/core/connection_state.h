#ifndef KATHTTP3_CONNECTION_STATE_H
#define KATHTTP3_CONNECTION_STATE_H

namespace kathttp3 {

enum class ConnectionState { None = 0, Connecting, Active, Draining, Closing, Closed };

inline bool connection_state_accepts_new_jobs(ConnectionState state, bool closed) {
    return !closed && state != ConnectionState::Draining && state != ConnectionState::Closing &&
           state != ConnectionState::Closed;
}

}  // namespace kathttp3

#endif
