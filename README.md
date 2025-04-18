# Network Interface Watcher (networkMonitor)

A UNIX-based C++ project for real-time network interface monitoring using UNIX domain sockets. This tool continuously observes network interface statistics and can automatically bring interfaces back online if they go down.

## 🚀 Overview

This system consists of two key components:

- **networkMonitor**: The main process that initiates and manages `intfMonitor` processes for each network interface to be monitored. It also handles inter-process communication and UI display.
- **intfMonitor**: A subprocess created for each interface. It reads live statistics from `/sys/class/net/<interface>` and communicates with the parent process via UNIX domain sockets.

### Key Features

- Monitors statistics like bytes transferred, packets dropped/errors, and state changes.
- Uses `/sys/class/net/` to extract kernel-level interface data.
- Detects interface failure and attempts automatic recovery.
- Inter-process communication using UNIX domain sockets.
- Modular design using C++ and UNIX system calls.
- Includes a `Makefile` for easy compilation.

---

## 🛠️ Project Structure

```
.
├── intfMonitor.cpp         # Monitors a single interface and reports stats
├── networkMonitor.cpp      # Main process handling multiple interfaces (not shown here)
├── Makefile                # Build instructions
├── README.md               # Project documentation
```

---

## 📦 Build Instructions

Make sure you are on a UNIX-like system (Linux preferred).

```bash
# Clone the repository
git clone https://github.com/your-username/network-monitor-daemon.git
cd network-monitor-daemon

# Build the project using Makefile
make all
```

---

## ✅ Usage

```bash
# Run the network monitor with specific interface(s)
sudo ./networkMonitor
```

The `networkMonitor` will prompt user for:
```bash
Enter number of interfaces to monitor: 2
Interface 1:
Interface 2:
``` 
The `networkMonitor` will launch separate `intfMonitor` processes for each interface.

---

## 📚 How It Works

- Each `intfMonitor`:
  - Connects to the main process via a UNIX domain socket (`/tmp/networkMonitor`)
  - Gathers statistics from the `/sys/class/net/<iface>/` directory
  - Monitors `operstate`, packet errors/drops, and byte traffic
  - If interface is detected as *down*, it attempts to bring it *up* using `ioctl`

- Main `networkMonitor`:
  - Accepts connections from all `intfMonitor`s
  - Displays interface status and aggregates output
  - Manages interface processes lifecycle

---

## 🧹 Cleaning Up

```bash
make clean
```

Removes compiled binaries.

---

## 🔒 Permissions

Make sure your user has the necessary permissions to access `/sys/class/net/` and perform `ioctl` operations to bring interfaces up.

You may need to run as `sudo` depending on your system's configuration.

---

## 👨‍💻 Author

Ashwin B N  
[GitHub](https://github.com/Ashwin-BN)  
[Email](mailto:ashwinbnwork@gmail.com)
