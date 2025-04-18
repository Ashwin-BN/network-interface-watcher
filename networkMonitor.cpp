/**
 * @file networkMonitor.cpp
 * @brief Network interface monitoring system using Unix domain sockets
 * @details This program creates a monitoring server that manages multiple network interfaces
 *          through child processes and communicates with clients via Unix domain sockets.
 */

 #include <iostream>
 #include <signal.h>
 #include <string.h>
 #include <sys/socket.h>
 #include <sys/types.h>
 #include <sys/un.h>
 #include <sys/wait.h>
 #include <unistd.h>
 #include <vector>
 
 // Constants
 const char* SOCKET_PATH = "/tmp/networkMonitor";
 const int BUFFER_SIZE = 256;
 const int MAX_CONNECTIONS = 2;
 
 // Global state management
 bool g_isRunning = true;             // Flag indicating server operation status
 std::vector<pid_t> g_childProcesses; // Container for tracking child process IDs
 
 /**
  * @brief Creates a Unix domain socket server for IPC communication
  * @return File descriptor of created socket, -1 on error
  */
 int createServerSocket() {
     struct sockaddr_un addr;
     int serverFd = socket(AF_UNIX, SOCK_STREAM, 0);
     
     if (serverFd < 0) {
         std::cerr << "!!! networkMonitor.cpp !!!- Error creating monitor socket: " 
                   << strerror(errno) << std::endl;
         return -1;
     }
 
     // Initialize socket address structure
     memset(&addr, 0, sizeof(addr));
     addr.sun_family = AF_UNIX;
     strncpy(addr.sun_path, SOCKET_PATH, sizeof(addr.sun_path) - 1);
     unlink(SOCKET_PATH);
 
     if (bind(serverFd, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
         std::cerr << "!!! networkMonitor.cpp !!!- Error binding server socket: " 
                   << strerror(errno) << std::endl;
         close(serverFd);
         return -1;
     }
 
     return serverFd;
 }
 
 /**
  * @brief Spawns a new process to monitor a network interface
  * @param interface Network interface name to monitor
  * @return Process ID of spawned process, -1 on error
  */
 pid_t spawnInterfaceMonitor(const std::string& interface) {
     pid_t pid = fork();
     
     if (pid == 0) {
         // Child process - execute interface monitor
         if (execlp("./intfMonitor", "./intfMonitor", interface.c_str(), nullptr) == -1) {
             std::cerr << "!!! networkMonitor.cpp !!!- Failed to execute intfMonitor for interface '" 
                       << interface << "': " << strerror(errno) << std::endl;
             exit(-1);
         }
     } else if (pid < 0) {
         // Fork failed
         std::cerr << "!!! networkMonitor.cpp !!!- Failed to fork process for interface '" 
                   << interface << "': " << strerror(errno) << std::endl;
         return -1;
     }
     
     return pid;
 }
 
 /**
  * @brief Initializes monitoring for multiple network interfaces
  * @param interfaces Vector of interface names to monitor
  * @param processIds Reference to vector storing child process IDs
  */
 void startMonitoring(const std::vector<std::string>& interfaces, 
                     std::vector<pid_t>& processIds) {
     for (const auto& interface : interfaces) {
         pid_t pid = spawnInterfaceMonitor(interface);
         if (pid > 0) {
             processIds.push_back(pid);
         }
     }
 }
 
 /**
  * @brief Handles new client connections to the monitoring server
  * @param serverFd Server socket file descriptor
  * @param masterSet Master file descriptor set
  * @param maxFd Maximum file descriptor value
  * @param clientFds Array of client file descriptors
  * @param activeClients Number of active client connections
  */
 void handleNewConnection(int serverFd, fd_set& masterSet, int& maxFd, 
                         int clientFds[], int& activeClients) {
     char buffer[BUFFER_SIZE];
     clientFds[activeClients] = accept(serverFd, nullptr, nullptr);
     
     if (clientFds[activeClients] < 0) {
         std::cerr << "!!! networkMonitor.cpp !!!- Error accepting connection: " 
                   << strerror(errno) << std::endl;
         return;
     }
 
     FD_SET(clientFds[activeClients], &masterSet);
 
     int bytesRead = read(clientFds[activeClients], buffer, BUFFER_SIZE - 1);
     if (bytesRead < 0) {
         std::cerr << "!!! networkMonitor.cpp !!!- Error reading from interface monitor: "
                   << strerror(errno) << std::endl;
         close(clientFds[activeClients]);
         return;
     }
 
     buffer[bytesRead] = '\0';
 
     // Verify connection handshake
     if (strcmp(buffer, "ready_to_monitor") == 0) {
         snprintf(buffer, BUFFER_SIZE, "start_monitoring");
         
         if (write(clientFds[activeClients], buffer, strlen(buffer) + 1) == -1) {
             std::cerr << "!!! networkMonitor.cpp !!!- Error writing to interface monitor: "
                       << strerror(errno) << std::endl;
             close(clientFds[activeClients]);
             return;
         }
     } else {
         std::cerr << "!!! networkMonitor.cpp !!!- Unexpected message from interface monitor: "
                   << buffer << std::endl;
         close(clientFds[activeClients]);
         return;
     }
 
     maxFd = std::max(maxFd, clientFds[activeClients]);
     ++activeClients;
 }
 
 /**
  * @brief Processes incoming data from interface monitors
  * @param activeClients Number of active clients
  * @param clientFds Array of client file descriptors
  * @param readSet File descriptor set for reading
  */
 void processMonitorData(int activeClients, int clientFds[], fd_set& readSet) {
     char buffer[BUFFER_SIZE];
     
     for (int i = 0; i < activeClients; ++i) {
         if (FD_ISSET(clientFds[i], &readSet)) {
             bzero(buffer, BUFFER_SIZE);
             int bytesRead = read(clientFds[i], buffer, BUFFER_SIZE);
             
             if (bytesRead > 0) {
                 std::cout << "Monitor [" << i << "] - Data received:\n" 
                          << buffer << std::endl;
             } else if (bytesRead == 0) {
                 std::cerr << "Monitor [" << i << "] has closed the connection." 
                          << std::endl;
                 close(clientFds[i]);
                 clientFds[i] = -1;
             }
         }
     }
 }
 
 /**
  * @brief Cleans up system resources during program termination
  * @param serverFd Server socket file descriptor
  * @param activeClients Number of active clients
  * @param clientFds Array of client file descriptors
  * @param masterSet Master file descriptor set
  * @param processIds Vector of child process IDs
  */
 void cleanup(int serverFd, int activeClients, int clientFds[], fd_set& masterSet, 
              std::vector<pid_t>& processIds) {
     // Signal child processes to terminate
     for (pid_t pid : processIds) {
         if (kill(pid, SIGUSR1) == -1) {
             std::cerr << "!!! networkMonitor.cpp !!!- Failed to send SIGUSR1 to process " 
                       << pid << ": " << strerror(errno) << std::endl;
         }
     }
 
     // Wait for child processes to exit
     int status;
     pid_t childPid;
     while ((childPid = wait(&status)) > 0) {
         std::cout << "Child process has exited (PID: " << childPid << ")" << std::endl;
     }
 
     // Close client connections
     for (int i = 0; i < activeClients; ++i) {
         FD_CLR(clientFds[i], &masterSet);
         close(clientFds[i]);
     }
 
     // Clean up server socket
     close(serverFd);
     if (unlink(SOCKET_PATH) == -1) {
         std::cerr << "!!! networkMonitor.cpp !!!- Failed to unlink socket path: "
                   << strerror(errno) << std::endl;
     } else {
         std::cout << "Socket path unlinked successfully." << std::endl;
     }
 }
 
 /**
  * @brief Signal handler for graceful program termination
  * @param signal Received signal number
  */
 static void handleSignal(int signal) {
     if (signal == SIGINT) {
         std::cout << "\nNetwork Monitor Shutting down..." << std::endl;
         g_isRunning = false;
     } else {
         std::cout << "\n!!! networkMonitor.cpp !!!- Undefined signal" << std::endl;
     }
 }
 
 int main() {
     int numInterfaces;
     std::cout << "Enter number of interfaces to monitor: ";
     std::cin >> numInterfaces;
 
     std::vector<std::string> interfaceNames(numInterfaces);
     for (int i = 0; i < numInterfaces; ++i) {
         std::cout << "Interface " << i + 1 << ": ";
         std::cin >> interfaceNames[i];
     }
 
     // Set up signal handling
     struct sigaction sa;
     sa.sa_handler = handleSignal;
     sigemptyset(&sa.sa_mask);
     sa.sa_flags = 0;
 
     if(sigaction(SIGINT, &sa, nullptr) == -1) {
         std::cerr << "!!! networkMonitor.cpp !!!- Error setting up signal handler: "
                   << strerror(errno) << std::endl;
         return EXIT_FAILURE;
     };
 
     // Initialize server
     int serverFd = createServerSocket();
     if (serverFd < 0) {
         return EXIT_FAILURE;
     }
 
     int maxFd = serverFd;
     fd_set masterSet, readSet;
     FD_ZERO(&masterSet);
     FD_SET(serverFd, &masterSet);
 
     int clientFds[MAX_CONNECTIONS];
     int activeClients = 0;
 
     // Start interface monitoring
     startMonitoring(interfaceNames, g_childProcesses);
 
     if (listen(serverFd, MAX_CONNECTIONS) == -1) {
         std::cerr << "!!! networkMonitor.cpp !!!- Error starting listener: " 
                   << strerror(errno) << std::endl;
         cleanup(serverFd, activeClients, clientFds, masterSet, g_childProcesses);
         return EXIT_FAILURE;
     }
 
     // Main server loop
     while (g_isRunning) {
         readSet = masterSet;
         int result = select(maxFd + 1, &readSet, nullptr, nullptr, nullptr);
 
         if (result < 0) {
             if (errno == EINTR) continue;
             std::cerr << "!!! networkMonitor.cpp !!!- Error in select: " 
                       << strerror(errno) << std::endl;
             break;
         }
 
         if (FD_ISSET(serverFd, &readSet)) {
             handleNewConnection(serverFd, masterSet, maxFd, clientFds, activeClients);
         } else {
             processMonitorData(activeClients, clientFds, readSet);
         }
     }
 
     // Cleanup and exit
     cleanup(serverFd, activeClients, clientFds, masterSet, g_childProcesses);
     return EXIT_SUCCESS;
 }