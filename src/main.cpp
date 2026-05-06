#include <iostream>
#include <atomic>
#include <pthread.h>
#include <sys/socket.h>
#include <sys/resource.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <ifaddrs.h>
#include "../include/handlers.h"
#include "../include/models.h"

using namespace std;

// Active connection counter for overload protection
static atomic<int> activeConnections{0};

// Detect local LAN IP address
static string getLocalIP()
{
    struct ifaddrs *ifAddrList = nullptr;
    if (getifaddrs(&ifAddrList) == -1)
        return "unknown";

    string lanIP = "unknown";
    for (struct ifaddrs *ifa = ifAddrList; ifa != nullptr; ifa = ifa->ifa_next)
    {
        if (!ifa->ifa_addr || ifa->ifa_addr->sa_family != AF_INET)
            continue;
        string name(ifa->ifa_name);
        if (name == "lo" || name == "lo0")
            continue;
        if (name.find("en") == 0 || name.find("eth") == 0 ||
            name.find("wlan") == 0 || name.find("wlp") == 0 ||
            name.find("ens") == 0)
        {
            struct sockaddr_in *addr = (struct sockaddr_in *)ifa->ifa_addr;
            lanIP = inet_ntoa(addr->sin_addr);
            break;
        }
    }
    freeifaddrs(ifAddrList);
    return lanIP;
}

// Thread entry point with connection tracking
static void *clientThreadFunc(void *arg)
{
    int clientSocket = (int)(intptr_t)arg;

    // Atomically claim a connection slot; reject if server is overloaded
    int prev = activeConnections.fetch_add(1);
    if (prev >= MAX_CONNECTIONS)
    {
        activeConnections.fetch_sub(1);
        static const char *OVERLOAD_RESP =
            "HTTP/1.1 503 Service Unavailable\r\n"
            "Content-Length: 0\r\nConnection: close\r\n\r\n";
        send(clientSocket, OVERLOAD_RESP, 70, 0);
        close(clientSocket);
        return nullptr;
    }

    handleClient(clientSocket);
    activeConnections.fetch_sub(1);
    return nullptr;
}

int main()
{
    // Raise file descriptor limit for high concurrency
    struct rlimit rl;
    if (getrlimit(RLIMIT_NOFILE, &rl) == 0)
    {
        rl.rlim_cur = min((rlim_t)4096, rl.rlim_max);
        setrlimit(RLIMIT_NOFILE, &rl);
    }

    int serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket < 0)
    {
        cerr << "Socket creation error" << endl;
        return -1;
    }

    int opt = 1;
    setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(8080);

    if (::bind(serverSocket, (struct sockaddr *)&address, sizeof(address)) < 0)
    {
        cerr << "Bind error" << endl;
        close(serverSocket);
        return -1;
    }

    // Backlog of 128 to handle burst connections from many users
    if (listen(serverSocket, 128) < 0)
    {
        cerr << "Listen error" << endl;
        close(serverSocket);
        return -1;
    }

    // Warm the template cache before accepting connections
    loadHTMLTemplate();

    string localIP = getLocalIP();
    string lanURL = "http://" + localIP + ":8080";

    cout << "\n╔════════════════════════════════════════════╗\n";
    cout << "║  C++ LAN Chat Server Started               ║\n";
    cout << "╠════════════════════════════════════════════╣\n";
    cout << "║  Local:  http://localhost:8080              ║\n";
    cout << "║  LAN:    " << lanURL << string(max(0, (int)(34 - lanURL.length())), ' ') << "║\n";
    cout << "║  Port:   8080                              ║\n";
    cout << "║  Max:    " << MAX_CONNECTIONS << " connections" << string(max(0, 23 - (int)to_string(MAX_CONNECTIONS).length()), ' ') << "║\n";
    cout << "╚════════════════════════════════════════════╝\n\n";
    cout << "Server running... (Ctrl+C to stop)\n\n";

    // Pre-configure thread attributes: 64KB stack, auto-detach
    pthread_attr_t threadAttr;
    pthread_attr_init(&threadAttr);
    pthread_attr_setstacksize(&threadAttr, 65536);
    pthread_attr_setdetachstate(&threadAttr, PTHREAD_CREATE_DETACHED);

    while (true)
    {
        struct sockaddr_in clientAddr{};
        socklen_t addrLen = sizeof(clientAddr);

        int clientSocket = accept(serverSocket, (struct sockaddr *)&clientAddr, &addrLen);
        if (clientSocket < 0)
            continue;

        // Disable Nagle's algorithm for faster small-packet responses
        int flag = 1;
        setsockopt(clientSocket, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(flag));

        pthread_t tid;
        if (pthread_create(&tid, &threadAttr, clientThreadFunc, (void *)(intptr_t)clientSocket) != 0)
        {
            close(clientSocket); // thread creation failed, don't leak FD
        }
    }

    pthread_attr_destroy(&threadAttr);
    close(serverSocket);
    return 0;
}
