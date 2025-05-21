# 🧠 Academic Management System using Socket Programming

This project implements a **multi-threaded server-client system** using **sockets in C**. The server manages academic data stored in files and handles multiple clients concurrently using **POSIX threads** and **mutex locks**.

## 🔗 Socket Programming Overview

- The **server** sets up a socket on **port 8080**, binds, listens, and accepts incoming connections.
- Each **client connection is handled in a separate detached thread**, enabling multiple users to interact simultaneously.
- Communication is done through **bidirectional messaging over sockets**.

## 🔐 Concurrency Control

### 🧵 Threads
- Each client runs in its own **detached pthread**, ensuring automatic resource cleanup and concurrency.

### 🔒 Mutexes
Three **pthread mutexes** are used to synchronize access to shared files:
- `Student Mutex`: Protects `students.dat` for read/write operations.
- `Faculty Mutex`: Secures `faculty.dat` during updates.
- `Course Mutex`: Synchronizes course enrollments, deletions, and updates.

### ⚠️ Semaphores
- `semaphore.h` is included for future concurrency enhancements, but not used in the current version.

## 💾 File-Based Database

The server maintains persistent data using flat files and system calls (`open`, `read`, `write`, `lseek`):
- `admin.dat`: Admin credentials
- `students.dat`: Student records
- `faculty.dat`: Faculty and course details

### Signal Handling
- The server handles `SIGINT` (Ctrl+C) gracefully, ensuring all mutexes are destroyed and no resources are leaked.

## 👤 User Roles & Features

### 🛠 Admin
- Add New Student
- Add Faculty
- Activate/Deactivate Student
- Update Student/Faculty Info
- Exit

### 🎓 Student
- Enroll in Course
- Unenroll from Course
- View Enrolled Courses
- Change Password
- Exit

### 👨‍🏫 Faculty
- Add/Remove Courses
- View Enrollments
- Change Password
- Exit

## 🗂 Data Structures

### 1. `Student`
- ID, Username, Password
- Active Status
- Enrolled Courses (Array), Course Count

### 2. `Faculty`
- ID, Username, Password
- Offered Courses (Array)
- Seats per Course, Course Count

### 3. `Admin`
- Username, Password

## 🧪 How to Run

### Compile the Server and Client

```bash
gcc server.c -o server -lpthread
gcc client.c -o client
```

### Run the Server

```bash
./server
```

### Connect a Client

```bash
./client
```

## 📝 Notes
- Make sure to run the server before starting clients.
- Data files (`students.dat`, `faculty.dat`, `admin.dat`) should be in the same directory as the server.

---
