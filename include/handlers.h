#ifndef HANDLERS_H
#define HANDLERS_H

#include <string>

using namespace std;

// HTTP Response builders
string buildHTTPResponse(const string &body, const string &contentType = "text/html");
string buildErrorResponse(const string &message);
string buildJSONResponse(const string &json);

// API Handlers
string handleJoin(const string &body);
string handleMessage(const string &body);
string handleStatus(const string &userId);
string handleLeave(const string &userId);

// HTTP Request Parser
void handleClient(int clientSocket);

// Template loader (returns cached reference after first load)
const string &loadHTMLTemplate();

#endif
