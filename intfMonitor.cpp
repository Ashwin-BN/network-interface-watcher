/**
 * @file intfMonitor.cpp
 * @brief Network interface monitoring utility
 * @details This program monitors network interface statistics and reports them
 *          to a parent process via UNIX domain socket.
 */
#include <cstring>
#include <fcntl.h>
#include <fstream>
#include <iostream>
#include <net/if.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <unistd.h>

// Constants
const char* SOCKET_PATH = "/tmp/networkMonitor";
const int BUFFER_SIZE = 256;
const int MAX_IFACE_NAME = 32;

// Global variables
bool g_isActive = true;
std::string g_interfaceStats;
std::string g_lastState;  // Track last known interface state

/**
 * @brief Establishes a connection to the parent process via UNIX domain socket
 * @return File descriptor of the established connection
 * @throws runtime_error on connection failure
 */
int establishConnection() {
    struct sockaddr_un addr;
    int sock = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sock < 0) {
        throw std::runtime_error("!!! intfMonitor.cpp !!!- Socket creation failed: " + std::string(strerror(errno)));
    }
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, SOCKET_PATH, sizeof(addr.sun_path) - 1);
    addr.sun_path[sizeof(addr.sun_path) - 1] = '\0';
    if (connect(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        close(sock);
        throw std::runtime_error("!!! intfMonitor.cpp !!!- Connection failed: " + std::string(strerror(errno)));
    }
    return sock;
}

/**
 * @brief Restores network interface to operational state
 * @param interface Name of the network interface to restore
 * @return 0 on success, EXIT_FAILURE on failure
 */
int restoreInterface(const char* interface) {
    struct ifreq ifr;
    memset(&ifr, 0, sizeof(ifr));
    strncpy(ifr.ifr_name, interface, IFNAMSIZ);
    ifr.ifr_name[IFNAMSIZ - 1] = '\0';
    ifr.ifr_flags = IFF_UP;
    int socketFd = socket(AF_INET, SOCK_DGRAM, 0);
    if (socketFd < 0) {
        throw std::runtime_error("!!! intfMonitor.cpp !!!- Socket creation failed: " + std::string(strerror(errno)));
    }
    int result = ioctl(socketFd, SIOCSIFFLAGS, &ifr);
    if (result < 0) {
        throw std::runtime_error("!!! intfMonitor.cpp !!!- Failed to bring interface up: '" + std::string(interface) +
                                "' - " + std::string(strerror(errno)));
    }
    close(socketFd);
    return result;
}

/**
 * @brief Collects network interface statistics
 * @param interface Name of the interface to monitor
 * @param data Reference to string that will contain the formatted statistics
 */
void gatherStats(const char* interface, std::string& data) {
    std::string state;
    int upCount = 0, downCount = 0;
    int txBytes = 0, rxBytes = 0;
    int rxDropped = 0, rxErrors = 0;
    int txPackets = 0, rxPackets = 0;
    int txDropped = 0, txErrors = 0;
    char path[BUFFER_SIZE];
    std::ifstream file;

    // Read interface state
    snprintf(path, sizeof(path), "/sys/class/net/%s/operstate", interface);
    file.open(path);
    if (file.is_open()) {
        file >> state;
        file.close();
    }

    // Read carrier counts
    snprintf(path, sizeof(path), "/sys/class/net/%s/carrier_up_count", interface);
    file.open(path);
    if (file.is_open()) {
        file >> upCount;
        file.close();
    }
    snprintf(path, sizeof(path), "/sys/class/net/%s/carrier_down_count", interface);
    file.open(path);
    if (file.is_open()) {
        file >> downCount;
        file.close();
    }

    // Read transmit statistics
    snprintf(path, sizeof(path), "/sys/class/net/%s/statistics/tx_bytes", interface);
    file.open(path);
    if (file.is_open()) {
        file >> txBytes;
        file.close();
    }

    // Read receive statistics
    snprintf(path, sizeof(path), "/sys/class/net/%s/statistics/rx_bytes", interface);
    file.open(path);
    if (file.is_open()) {
        file >> rxBytes;
        file.close();
    }

    // Read dropped packets
    snprintf(path, sizeof(path), "/sys/class/net/%s/statistics/rx_dropped", interface);
    file.open(path);
    if (file.is_open()) {
        file >> rxDropped;
        file.close();
    }

    // Read errors
    snprintf(path, sizeof(path), "/sys/class/net/%s/statistics/rx_errors", interface);
    file.open(path);
    if (file.is_open()) {
        file >> rxErrors;
        file.close();
    }

    // Read packet counts
    snprintf(path, sizeof(path), "/sys/class/net/%s/statistics/tx_packets", interface);
    file.open(path);
    if (file.is_open()) {
        file >> txPackets;
        file.close();
    }
    snprintf(path, sizeof(path), "/sys/class/net/%s/statistics/tx_dropped", interface);
    file.open(path);
    if (file.is_open()) {
        file >> txDropped;
        file.close();
    }
    snprintf(path, sizeof(path), "/sys/class/net/%s/statistics/tx_errors", interface);
    file.open(path);
    if (file.is_open()) {
        file >> txErrors;
        file.close();
    }
    snprintf(path, sizeof(path), "/sys/class/net/%s/statistics/rx_packets", interface);
    file.open(path);
    if (file.is_open()) {
        file >> rxPackets;
        file.close();
    }

    // Check interface state and restore if down
    if (state == "down" && g_lastState != "down") {
        std::cout << "!!! Interface " << interface
                  << " is DOWN - attempting to restore !!!" << std::endl << std::endl;
        restoreInterface(interface);
        g_lastState = state;
    } else if (state != "down") {
        g_lastState = state;
    }

    // Format statistics
    data = "Interface: " + std::string(interface) + " state: " + state +
           " up_count: " + std::to_string(upCount) +
           " down_count: " + std::to_string(downCount) + "\n" +
           "rx_bytes: " + std::to_string(rxBytes) +
           " rx_dropped: " + std::to_string(rxDropped) +
           " rx_errors: " + std::to_string(rxErrors) +
           " rx_packets: " + std::to_string(rxPackets) + "\n" +
           "tx_bytes: " + std::to_string(txBytes) +
           " tx_dropped: " + std::to_string(txDropped) +
           " tx_errors: " + std::to_string(txErrors) +
           " tx_packets: " + std::to_string(txPackets) + "\n";
}

/**
 * @brief Monitors and reports interface statistics
 * @param interfaceName Name of the interface to monitor
 * @param socket File descriptor of the connection to parent process
 */
void monitorInterface(const char* interfaceName, int socket) {
    char buffer[BUFFER_SIZE];
    memset(buffer, 0, sizeof(buffer));
    gatherStats(interfaceName, g_interfaceStats);
    strncpy(buffer, g_interfaceStats.c_str(), BUFFER_SIZE - 1);
    buffer[BUFFER_SIZE - 1] = '\0';
    if (write(socket, buffer, strlen(buffer)) < 0) {
        std::cerr << "!!! intfMonitor.cpp !!!- Failed to send data: "
                  << strerror(errno) << std::endl;
    }
}

/**
 * @brief Signal handler for process termination
 * @param signal Signal number received
 */
static void handleSignal(int signal) {
    if (signal == SIGUSR1) {
        std::cout << "Interface Monitor Shutting down..." << std::endl;
        g_isActive = false;
    }
}

int main(int argc, char* argv[]) {
    try {
        if (argc < 2) {
            std::cerr << "Usage: " << argv[0] << " <network-interface>" << std::endl;
            return EXIT_FAILURE;
        }
        char interfaceName[MAX_IFACE_NAME];
        strncpy(interfaceName, argv[1], MAX_IFACE_NAME - 1);
        
        // Set up signal handler
        struct sigaction sigAction;
        sigAction.sa_handler = handleSignal;
        sigemptyset(&sigAction.sa_mask);
        sigAction.sa_flags = 0;
        if (sigaction(SIGUSR1, &sigAction, nullptr) < 0) {
            throw std::runtime_error("Failed to set up signal handler: " +
                                    std::string(strerror(errno)));
        }
        
        // Ignore SIGINT
        struct sigaction ignoreSigAction;
        ignoreSigAction.sa_handler = SIG_IGN;
        sigemptyset(&ignoreSigAction.sa_mask);
        ignoreSigAction.sa_flags = 0;
        if (sigaction(SIGINT, &ignoreSigAction, nullptr) < 0) {
            throw std::runtime_error("Failed to block SIGINT: " +
                                    std::string(strerror(errno)));
        }
        
        // Establish connection and initialize monitoring
        int socket = establishConnection();
        write(socket, "ready_to_monitor", 16);
        char buffer[BUFFER_SIZE];
        int bytesRead = read(socket, buffer, BUFFER_SIZE - 1);
        if (bytesRead < 0) {
            throw std::runtime_error("Failed to read data: " +
                                    std::string(strerror(errno)));
        }
        buffer[bytesRead] = '\0';
        if (strcmp(buffer, "start_monitoring") != 0) {
            throw std::runtime_error("Unexpected message received: " +
                                    std::string(buffer));
        }
        
        // Main monitoring loop
        while (g_isActive) {
            monitorInterface(interfaceName, socket);
            sleep(1);
        }
        close(socket);
        return EXIT_SUCCESS;
    }
    catch (const std::exception& e) {
        std::cerr << "!!! intfMonitor.cpp !!!- Error: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }
}