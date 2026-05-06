#ifndef MODELS_H
#define MODELS_H

#include <string>
#include <vector>
#include <map>
#include <mutex>
#include <atomic>
#include <ctime>

using namespace std;

// Limits
const size_t MAX_MESSAGE_HISTORY = 200;
const int HEARTBEAT_TIMEOUT_SECONDS = 10;
const int MAX_CONNECTIONS = 200;

// User model
struct User {
    string id;
    string username;
    string joinedAt;
    time_t lastSeen;
};

// Message model
struct Message {
    string username;
    string text;
    string timestamp;
};

// Global state management
extern map<string, User> connectedUsers;
extern vector<Message> messageHistory;
extern mutex stateMutex;
extern atomic<int> userCounter;

#endif
