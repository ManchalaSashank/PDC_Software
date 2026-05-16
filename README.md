# PDC_Software
# PDC Monitor System

## Overview
PDC Monitor is a Qt-based desktop application developed for monitoring and visualizing PMU/PDC data in real time. The application provides a graphical interface for viewing incoming phasor data, network communication, and combined dashboard monitoring.

This project is built using:

- Qt 6.11
- MinGW 64-bit
- CMake
- Qt Charts
- Qt Network

---

To Directly Run this software, Navigate to the Setup Files folder which is present and download that folder to run directly on any computer. The setup steps given below mention this 
# Folder Structure

```text
SetupFiles/
│
├── PDC_Monitor.exe              # Main executable
├── Qt6Core.dll
├── Qt6Gui.dll
├── Qt6Widgets.dll
├── Qt6Charts.dll
├── Qt6Network.dll
├── platforms/                   # Qt platform plugins
├── styles/
├── imageformats/
├── iconengines/
├── tls/
└── translations/
```

---

# Requirements

## For Running the Application

You only need:

- Windows 10 or Windows 11
- The complete release folder

No separate Qt installation is required if all DLLs and plugin folders are present.

---

# How to Run the Project

## Method 1: Run the Prebuilt Application (Recommended)

### Step 1
Extract the ZIP file:

```text
Desktop_Qt_6_11_0_MinGW_64_bit-Release_3.zip
```

### Step 2
Open the extracted folder.

### Step 3
Double-click:

```text
PDC_Monitor.exe
```

The application should launch directly.

---

# Important Notes

## Do NOT delete these folders

The following folders are required for Qt applications to work correctly:

```text
platforms/
styles/
imageformats/
iconengines/
tls/
translations/
```

If these folders are missing, the application may fail to start.

---

To Edit the Files and then run and debug and add any other self features follow the below steps in order to do so:
# Building the Project from Source

## Requirements

Install:

- Qt 6.11
- MinGW 64-bit Compiler
- CMake
- Qt Creator (Recommended)

---

## Steps

### 1. Open Qt Creator

### 2. Open the Project

Open the `CMakeLists.txt` file.

### 3. Configure the Kit

Select:

```text
Desktop Qt 6.11.0 MinGW 64-bit
```

### 4. Build the Project

Press:

```text
Ctrl + B
```

### 5. Run the Project

Press:

```text
Ctrl + R
```

---

# Deployment

To create a standalone executable folder:

## Using windeployqt

Open Qt Command Prompt and run:

```bash
windeployqt PDC_Monitor.exe
```

This copies all required Qt dependencies automatically.

---

# Features

- Real-time PMU/PDC monitoring
- Dashboard visualization
- Network-based data communication
- Combined monitoring interface
- Graphical representation of incoming data
- Qt Charts integration
- TCP/Network support

---

# Troubleshooting

## Application closes immediately

- Ensure all DLLs are present
- Ensure plugin folders exist
- Do not move the EXE alone outside the release folder

---

## Firewall Blocking Communication

Allow:

```text
PDC_Monitor.exe
```

through Windows Firewall.

---

# Author

Developed as part of a PMU/PDC Monitoring and Visualization Project.

---

# License

This project is intended for educational and academic purposes.

