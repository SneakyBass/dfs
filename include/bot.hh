#pragma once

#include <chrono>
#include <condition_variable>
#include <list>
#include <mutex>

namespace dfs
{
    using now_t =
        std::chrono::time_point<std::chrono::system_clock, std::chrono::duration<long, std::ratio<1, 1000000000>>>;

    struct BotState;

    class BotDescriptor {
      public:
        BotDescriptor(BotState &state, int server_socket);
        BotDescriptor(const BotDescriptor &) = delete;
        BotDescriptor operator=(const BotDescriptor &) = delete;

        /// Wait for an event or the end of the timer (must be called from the bot logic)
        std::unique_lock<std::mutex> WaitForStateUpdate();

        /// Prevents the bot from making progress. Call this from the message handler.
        std::unique_lock<std::mutex> Lock();

        /// Sends a signal to wake the waiters for the `WaitForStateUpdate` function.
        void MarkUpdated(const now_t &wake_at = now_t{});

        /// Resets the state of the bot (actors, collectibles, ...) you may want to call this when
        /// changing maps for example).
        void ClearState();

        /// Sends a move command to the server
        void MoveTo(int cell_id);

        /// Sends a map change command to the server
        void ChangeMap(int map_id);

        /// Interact with an interactive element
        void Interact(int element_id, int skill_instance_uid);

        BotState &GetState() {
            return m_State;
        }

      private:
        void ConfirmMovement();

      private:
        BotState &m_State;
        int m_ServerSock;
        std::list<now_t> m_Timers;
        std::condition_variable m_Condvar;
        std::mutex m_Mutex;
    };
} // namespace dfs
