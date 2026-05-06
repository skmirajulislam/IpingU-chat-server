#ifndef UTILS_H
#define UTILS_H

#include <string>

using namespace std;

// JSON utilities
class JSON
{
public:
    static string escape(const string &s);
};

// Helper functions
string getCurrentTimestamp();
string generateUserID();
string getJSONValue(const string &body, const string &key);

#endif
