#include "../include/utils.h"
#include <ctime>
#include <cstring>
#include <atomic>

// JSON escape — pre-allocates to avoid repeated reallocation
string JSON::escape(const string &s)
{
    string result;
    result.reserve(s.size() + 16); // most strings need few escapes
    for (char c : s)
    {
        switch (c)
        {
        case '"':  result += "\\\""; break;
        case '\\': result += "\\\\"; break;
        case '\n': result += "\\n";  break;
        case '\r': result += "\\r";  break;
        case '\t': result += "\\t";  break;
        default:   result += c;
        }
    }
    return result;
}

// Get current timestamp — uses strftime directly instead of stringstream
string getCurrentTimestamp()
{
    time_t now = time(nullptr);
    char buf[16];
    struct tm tm_buf;
    strftime(buf, sizeof(buf), "%H:%M:%S", localtime_r(&now, &tm_buf));
    return string(buf);
}

// Generate unique user ID
string generateUserID()
{
    extern atomic<int> userCounter;
    return "user_" + to_string(userCounter.fetch_add(1) + 1);
}

// Simple JSON value extractor for flat JSON objects
string getJSONValue(const string &body, const string &key)
{
    string pattern = "\"" + key + "\"";
    size_t p = body.find(pattern);
    if (p == string::npos)
        return string();
    size_t colon = body.find(':', p + pattern.length());
    if (colon == string::npos)
        return string();
    size_t quote = body.find('"', colon);
    if (quote == string::npos)
        return string();
    quote++; // move past opening quote
    size_t end = body.find('"', quote);
    if (end == string::npos)
        return string();
    return body.substr(quote, end - quote);
}
