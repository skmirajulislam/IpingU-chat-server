#include "../include/models.h"
#include <mutex>

// Global state
map<string, User> connectedUsers;
vector<Message> messageHistory;
mutex stateMutex;
int userCounter = 0;
