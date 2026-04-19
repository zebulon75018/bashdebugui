# BashDb GUI (Qt5)

A modern graphical frontend for **bashdb** (a GDB-like debugger for Bash scripts), built with **C++** and **Qt 5 Widgets**.

This project provides a user-friendly interface to debug shell scripts with features similar to traditional IDE debuggers (breakpoints, stepping, variables, call stack, etc.).

---

## 🚀 Features

* 📂 Load and debug Bash scripts (`.sh`)
* 🧠 Integrated code editor with:

  * Syntax highlighting (basic Bash support)
  * Line numbers
  * Current execution line highlighting
  * Breakpoint margin (click to toggle)
* ▶️ Debug controls:

  * Run / Continue
  * Step / Next
  * Run to cursor
  * Stop / Quit
* 🧵 Call stack (backtrace)
* 📊 Variables view (locals / globals)
* 🧱 Breakpoints manager
* 💻 Interactive debug console:

  * Displays bashdb output
  * Accepts manual commands
* 🎨 Dark theme UI (Qt Widgets)
* ⚡ Asynchronous communication with bashdb via `QProcess`
* 🪵 Logging system for debugging

---

## 🧰 Requirements

* Linux (tested on Ubuntu/Debian)
* C++17 compatible compiler
* Qt 5 (Widgets module)
* CMake (>= 3.10) or qmake
* **bashdb** installed on your system

Install bashdb:

```bash
sudo apt install bashdb
```

---

## 🏗️ Build Instructions

### 🔧 Using CMake

```bash
mkdir build
cd build
cmake ..
make -j$(nproc)
```

Run:

```bash
./BashDbGui
```

---

### 🔧 Using qmake (.pro)

```bash
qmake BashDbGui.pro
make -j$(nproc)
```

Run:

```bash
./BashDbGui
```

---

## ▶️ Usage

### Launch without script

```bash
./BashDbGui
```

Then load a script using the GUI.

---

### Launch with a script

```bash
./BashDbGui my_script.sh
```

The script will:

* be automatically loaded into the editor
* be ready for debugging

---

## 🧩 Architecture Overview

### Core Components

* **MainWindow**

  * Manages UI layout and user interactions
* **CodeEditor**

  * Custom editor with line numbers and breakpoint margin
* **BashDbDebugger**

  * Handles communication with bashdb via `QProcess`
  * Sends commands and parses output
* **BashDbCommand / Factory**

  * Parses user input into structured command objects
  * Supports extensible command handling

---

## 🔄 Debugger Integration

The application launches **bashdb** as a child process:

```cpp
QProcess *process;
process->start("bashdb", QStringList() << scriptPath);
```

Communication is fully asynchronous using:

* `readyReadStandardOutput`
* `readyReadStandardError`

---

## 🎯 Breakpoints

* Click in the editor margin to toggle breakpoints
* Stored internally and synchronized with bashdb
* Listed in the breakpoints panel

---

## 🧠 ANSI Output Handling

bashdb outputs ANSI color codes.

The project includes:

* ANSI stripping (for plain text)
* Optional ANSI → HTML conversion for colored output

---

## 🪵 Logging

A simple logging system is included:

* Console logging
* File logging (optional)

Useful for debugging bashdb communication.

---

## ⚠️ Error Handling

The application detects:

* Missing bashdb executable
* Invalid script path
* Process failures

Errors are displayed in:

* Console
* Status bar
* Logs

---

## 🎨 UI Layout

Built using `QSplitter`:

* **Left panel**

  * Code editor

* **Right panel (tabs)**

  * Variables
  * Call stack
  * Debug console

---

## 📁 Project Structure

```
project/
├── src/
│   ├── main.cpp
│   ├── MainWindow.h
│   ├── MainWindow.cpp
│   ├── BashDbCommand.h
│   ├── BashDbCommand.cpp
│
├── CMakeLists.txt
├── BashDbGui.pro
├── README.md
```

---

## 🔮 Future Improvements

* Watch expressions
* Conditional breakpoints
* Better bash syntax highlighting
* Multi-file debugging support
* Persistent session state
* GDB/MI-like structured parsing
* Improved ANSI rendering

---

## 🤝 Contributing

Contributions are welcome!

Feel free to:

* open issues
* submit pull requests
* suggest improvements

---

## 📄 License

MIT License (or your preferred license)

---

## 👨‍💻 Author

Built with ❤️ using Qt and C++.

