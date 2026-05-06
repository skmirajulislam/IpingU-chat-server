#include "../include/handlers.h"
#include "../include/models.h"
#include "../include/utils.h"
#include <iostream>
#include <fstream>
#include <algorithm>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/socket.h>

using namespace std;

// ─── Session key + Template cache ───

static string _cachedTemplate;
static bool _templateLoaded = false;

// Generate 128-bit random session key (hex) from /dev/urandom
static string generateSessionKey()
{
    const char hex[] = "0123456789abcdef";
    string key(32, '0');
    ifstream rng("/dev/urandom", ios::binary);
    if (rng)
    {
        unsigned char buf[16];
        rng.read((char *)buf, 16);
        for (int i = 0; i < 16; i++)
        {
            key[i * 2] = hex[buf[i] >> 4];
            key[i * 2 + 1] = hex[buf[i] & 0xf];
        }
    }
    return key;
}

const string &loadHTMLTemplate()
{
    if (!_templateLoaded)
    {
        ifstream file("templates/index.html");
        if (file.is_open())
        {
            file.seekg(0, ios::end);
            size_t size = file.tellg();
            file.seekg(0, ios::beg);
            _cachedTemplate.resize(size);
            file.read(&_cachedTemplate[0], size);

            // Inject random session key for transparent encryption
            string sessionKey = generateSessionKey();
            size_t pos = _cachedTemplate.find("__SESSION_KEY__");
            if (pos != string::npos)
                _cachedTemplate.replace(pos, 15, sessionKey);

            cerr << "[INFO] Session encryption key generated\n";
        }
        else
        {
            cerr << "[ERROR] Could not open templates/index.html\n";
            _cachedTemplate = "<html><body>Error loading template</body></html>";
        }
        _templateLoaded = true;
    }
    return _cachedTemplate;
}

// ─── HTTP Response builders (string concat, no stringstream) ───

static const char *HTTP_200 =
    "HTTP/1.1 200 OK\r\n"
    "Connection: close\r\n"
    "Access-Control-Allow-Origin: *\r\n";

static const char *HTTP_400 =
    "HTTP/1.1 400 Bad Request\r\n"
    "Content-Type: application/json; charset=utf-8\r\n"
    "Connection: close\r\n"
    "Access-Control-Allow-Origin: *\r\n";

string buildHTTPResponse(const string &body, const string &contentType)
{
    string lenStr = to_string(body.length());
    string resp;
    resp.reserve(200 + body.length());
    resp += HTTP_200;
    resp += "Content-Type: ";
    resp += contentType;
    resp += "; charset=utf-8\r\nContent-Length: ";
    resp += lenStr;
    resp += "\r\n\r\n";
    resp += body;
    return resp;
}

string buildErrorResponse(const string &message)
{
    string json = "{\"success\":false,\"message\":\"" + JSON::escape(message) + "\"}";
    string lenStr = to_string(json.length());
    string resp;
    resp.reserve(200 + json.length());
    resp += HTTP_400;
    resp += "Content-Length: ";
    resp += lenStr;
    resp += "\r\n\r\n";
    resp += json;
    return resp;
}

string buildJSONResponse(const string &json)
{
    string lenStr = to_string(json.length());
    string resp;
    resp.reserve(200 + json.length());
    resp += HTTP_200;
    resp += "Content-Type: application/json; charset=utf-8\r\nContent-Length: ";
    resp += lenStr;
    resp += "\r\n\r\n";
    resp += json;
    return resp;
}

// ─── API Handlers ───

string handleJoin(const string &body)
{
    lock_guard<mutex> lock(stateMutex);

    string username = getJSONValue(body, "username");
    if (username.empty() || username.length() > 20)
        return buildErrorResponse("Invalid username");

    // Reject duplicate usernames
    for (const auto &pair : connectedUsers)
    {
        if (pair.second.username == username)
            return buildErrorResponse("Username already taken");
    }

    string userId = generateUserID();
    connectedUsers[userId] = {userId, username, getCurrentTimestamp(), time(nullptr)};

    string json = "{\"success\":true,\"userId\":\"" + userId + "\"}";
    return buildJSONResponse(json);
}

string handleMessage(const string &body)
{
    lock_guard<mutex> lock(stateMutex);

    string userId = getJSONValue(body, "userId");
    string text = getJSONValue(body, "text");

    if (connectedUsers.find(userId) == connectedUsers.end())
        return buildErrorResponse("User not found");

    if (text.empty() || text.length() > 1024)
        return buildErrorResponse("Invalid message");

    const string &username = connectedUsers[userId].username;
    messageHistory.push_back({username, text, getCurrentTimestamp()});

    // Cap message history to prevent unbounded memory growth
    if (messageHistory.size() > MAX_MESSAGE_HISTORY)
        messageHistory.erase(messageHistory.begin());

    return buildJSONResponse("{\"success\":true}");
}

string handleStatus(const string &userId)
{
    // Snapshot state under lock (fast), build JSON outside (slow)
    size_t userCount;
    vector<string> usernames;
    vector<Message> messages;

    {
        lock_guard<mutex> lock(stateMutex);
        time_t now = time(nullptr);

        // Update heartbeat
        auto self = connectedUsers.find(userId);
        if (self != connectedUsers.end())
            self->second.lastSeen = now;

        // Prune stale users
        for (auto it = connectedUsers.begin(); it != connectedUsers.end();)
        {
            if (now - it->second.lastSeen > HEARTBEAT_TIMEOUT_SECONDS)
                it = connectedUsers.erase(it);
            else
                ++it;
        }

        // Snapshot
        userCount = connectedUsers.size();
        usernames.reserve(userCount);
        for (const auto &pair : connectedUsers)
            usernames.push_back(pair.second.username);
        messages = messageHistory;
    }
    // Lock released — JSON building below does NOT block other requests

    string json;
    json.reserve(128 + messages.size() * 100);

    json += "{\"userCount\":";
    json += to_string(userCount);
    json += ",\"users\":[";

    for (size_t i = 0; i < usernames.size(); i++)
    {
        if (i > 0) json += ',';
        json += '"';
        json += JSON::escape(usernames[i]);
        json += '"';
    }

    json += "],\"messages\":[";
    for (size_t i = 0; i < messages.size(); i++)
    {
        if (i > 0) json += ',';
        json += "{\"username\":\"";
        json += JSON::escape(messages[i].username);
        json += "\",\"text\":\"";
        json += JSON::escape(messages[i].text);
        json += "\",\"timestamp\":\"";
        json += JSON::escape(messages[i].timestamp);
        json += "\"}";
    }

    json += "]}";
    return buildJSONResponse(json);
}

string handleLeave(const string &userId)
{
    lock_guard<mutex> lock(stateMutex);
    connectedUsers.erase(userId);
    return buildJSONResponse("{\"success\":true}");
}

// ─── HTTP Request Router ───

// Extract query parameter value from a raw HTTP request line
static string extractQueryParam(const string &request, const string &param)
{
    string key = param + "=";
    size_t pos = request.find(key);
    if (pos == string::npos) return "";
    size_t start = pos + key.length();
    size_t end = request.find_first_of(" &\r\n", start);
    return request.substr(start, end - start);
}

// Extract POST body from raw HTTP request
static string extractBody(const string &request)
{
    size_t pos = request.find("\r\n\r\n");
    if (pos == string::npos || pos + 4 >= request.size()) return "";
    return request.substr(pos + 4);
}

void handleClient(int clientSocket)
{
    char buffer[4096];
    ssize_t bytesRead = recv(clientSocket, buffer, sizeof(buffer) - 1, 0);
    if (bytesRead <= 0)
    {
        close(clientSocket);
        return;
    }
    buffer[bytesRead] = '\0';

    string request(buffer, (size_t)bytesRead);

    // Find end of headers
    size_t headersEnd = request.find("\r\n\r\n");
    if (headersEnd == string::npos)
    {
        close(clientSocket);
        return;
    }

    // Read remaining body if Content-Length indicates more data
    size_t clPos = request.find("Content-Length: ");
    if (clPos == string::npos)
        clPos = request.find("content-length: ");

    if (clPos != string::npos && clPos < headersEnd)
    {
        size_t valStart = clPos + 16; // length of "Content-Length: "
        int contentLen = atoi(request.c_str() + valStart);
        if (contentLen > 0 && contentLen < 65536)
        {
            size_t bodyStart = headersEnd + 4;
            size_t have = request.size() > bodyStart ? request.size() - bodyStart : 0;
            while ((int)have < contentLen)
            {
                ssize_t more = recv(clientSocket, buffer, sizeof(buffer) - 1, 0);
                if (more <= 0) break;
                request.append(buffer, (size_t)more);
                have += (size_t)more;
            }
        }
    }

    // Route request
    string response;

    if (request.compare(0, 4, "GET ") == 0)
    {
        if (request.compare(4, 11, "/api/status") == 0)
            response = handleStatus(extractQueryParam(request, "userId"));
        else if (request.compare(4, 10, "/api/leave") == 0)
            response = handleLeave(extractQueryParam(request, "userId"));
        else
            response = buildHTTPResponse(loadHTMLTemplate());
    }
    else if (request.compare(0, 5, "POST ") == 0)
    {
        string body = extractBody(request);
        if (request.compare(5, 9, "/api/join") == 0)
            response = handleJoin(body);
        else if (request.compare(5, 12, "/api/message") == 0)
            response = handleMessage(body);
        else if (request.compare(5, 10, "/api/leave") == 0)
            response = handleLeave(extractQueryParam(request, "userId"));
        else
            response = "HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\nConnection: close\r\n\r\n";
    }
    else
    {
        response = "HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\nConnection: close\r\n\r\n";
    }

    send(clientSocket, response.c_str(), response.length(), 0);
    close(clientSocket);
}
