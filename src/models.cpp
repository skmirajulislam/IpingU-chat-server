#include "../include/models.h"
#include <mutex>
#include <atomic>

// Global state
map<string, User> connectedUsers;
vector<Message> messageHistory;
mutex stateMutex;
atomic<int> userCounter{0};
