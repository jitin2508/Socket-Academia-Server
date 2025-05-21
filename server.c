/**
 * Server implementation for Academia Portal
 * Course Registration System
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>
#include <fcntl.h>
#include <semaphore.h>
#include <sys/stat.h>
#include <signal.h>

#define PORT 8080
#define MAX_CLIENTS 100
#define BUFFER_SIZE 1024
#define MAX_COURSES 50
#define MAX_SEATS 100

// Structures
typedef struct {
    int id;
    char username[50];
    char password[50];
    int active;
    char courses[MAX_COURSES][50]; // Enrolled courses
    int course_count;
} Student;

typedef struct {
    int id;
    char username[50];
    char password[50];
    char courses[MAX_COURSES][50]; // Offered courses
    int seats[MAX_COURSES];        // Available seats for each course
    int initial_seats[MAX_COURSES]; // <--- Add this line
    int course_count;
} Faculty;

typedef struct {
    char username[50];
    char password[50];
} Admin;

// Global variables
pthread_mutex_t student_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t faculty_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t course_mutex = PTHREAD_MUTEX_INITIALIZER;

// Function declarations
void handle_client(int client_socket);
void admin_menu(int client_socket);
void student_menu(int client_socket, int student_id);
void faculty_menu(int client_socket, int faculty_id);
int authenticate_user(char *username, char *password, char *role);
void add_student(int client_socket);
void add_faculty(int client_socket);
void toggle_student_status(int client_socket);
void update_details(int client_socket);
void enroll_course(int client_socket, int student_id);
void unenroll_course(int client_socket, int student_id);
void view_enrolled_courses(int client_socket, int student_id);
void change_password(int client_socket, char *role, int id);
void add_course(int client_socket, int faculty_id);
void remove_course(int client_socket, int faculty_id);
void view_enrollments(int client_socket, int faculty_id);
int check_course_exists(char *course_name);
void initialize_files();

void signal_handler(int sig) {
    // Clean up and exit gracefully
    pthread_mutex_destroy(&student_mutex);
    pthread_mutex_destroy(&faculty_mutex);
    pthread_mutex_destroy(&course_mutex);
    exit(0);
}

// Main function
int main() {
    int server_fd, client_socket;
    struct sockaddr_in address;
    int addrlen = sizeof(address);
    int opt = 1;
    pthread_t thread_id;
    
    // Set up signal handler
    signal(SIGINT, signal_handler);
    
    // Initialize files if they don't exist
    initialize_files();
    
    // Create socket
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }
    
    // Set socket options
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt))) {
        perror("Setsockopt failed");
        exit(EXIT_FAILURE);
    }
    
    // Prepare the sockaddr_in structure
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);
    
    // Bind the socket
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("Bind failed");
        exit(EXIT_FAILURE);
    }
    
    // Listen for connections
    if (listen(server_fd, MAX_CLIENTS) < 0) {
        perror("Listen failed");
        exit(EXIT_FAILURE);
    }
    
    printf("Server started on port %d\n", PORT);
    
    // Accept connections and create threads for each client
    while (1) {
        if ((client_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t *)&addrlen)) < 0) {            perror("Accept failed");
            continue;
        }
        
        printf("New client connected\n");
        
        // Create a new thread for the client
        if (pthread_create(&thread_id, NULL, (void *)handle_client, (void *)(intptr_t)client_socket) != 0) {
            perror("Thread creation failed");
            close(client_socket);
        } else {
            // Detach the thread so it cleans itself up when finished
            pthread_detach(thread_id);
        }
    }
    
    // Clean up
    close(server_fd);
    pthread_mutex_destroy(&student_mutex);
    pthread_mutex_destroy(&faculty_mutex);
    pthread_mutex_destroy(&course_mutex);
    
    return 0;
}

// Initialize files if they don't exist
void initialize_files() {
    int fd;
    Admin admin = {"admin", "admin123"};
    
    // Create admin file if it doesn't exist
    if ((fd = open("admin.dat", O_RDWR | O_CREAT | O_EXCL, 0644)) != -1) {
        write(fd, &admin, sizeof(Admin));
        close(fd);
        printf("Admin file created and initialized\n");
    }
    
    // Create students file if it doesn't exist
    if ((fd = open("students.dat", O_RDWR | O_CREAT, 0644)) == -1) {
        perror("Error creating students file");
    } else {
        close(fd);
    }
    
    // Create faculty file if it doesn't exist
    if ((fd = open("faculty.dat", O_RDWR | O_CREAT, 0644)) == -1) {
        perror("Error creating faculty file");
    } else {
        close(fd);
    }
}

// Handle client connections
void handle_client(int client_socket) {
    char buffer[BUFFER_SIZE] = {0};
    char username[50], password[50], role[10];
    int choice, authenticated = 0, user_id = -1;
    
    // Send welcome message
    char *welcome_msg = "Welcome to Academia Portal\n1. Admin\n2. Faculty\n3. Student\nEnter your choice: ";
    write(client_socket, welcome_msg, strlen(welcome_msg));
    
    // Read role choice
    read(client_socket, buffer, BUFFER_SIZE);
    choice = atoi(buffer);
    
    // Ask for username and password
    write(client_socket, "Enter username: ", strlen("Enter username: "));
    memset(buffer, 0, BUFFER_SIZE);
    read(client_socket, buffer, BUFFER_SIZE);
    strcpy(username, buffer);
    
    write(client_socket, "Enter password: ", strlen("Enter password: "));
    memset(buffer, 0, BUFFER_SIZE);
    read(client_socket, buffer, BUFFER_SIZE);
    strcpy(password, buffer);
    
    // Set role based on choice
    switch (choice) {
        case 1:
            strcpy(role, "admin");
            break;
        case 2:
            strcpy(role, "faculty");
            break;
        case 3:
            strcpy(role, "student");
            break;
        default:
            write(client_socket, "Invalid choice\n", strlen("Invalid choice\n"));
            close(client_socket);
            return;
    }
    
    // Authenticate user
    user_id = authenticate_user(username, password, role);
    if (user_id >= 0) {
        authenticated = 1;
        write(client_socket, "Login successful\n", strlen("Login successful\n"));
    } else {
        write(client_socket, "Login failed\n", strlen("Login failed\n"));
        close(client_socket);
        return;
    }
    
    // If authenticated, redirect to respective menu
    if (authenticated) {
        if (strcmp(role, "admin") == 0) {
            admin_menu(client_socket);
        } else if (strcmp(role, "faculty") == 0) {
            faculty_menu(client_socket, user_id);
        } else if (strcmp(role, "student") == 0) {
            student_menu(client_socket, user_id);
        }
    }
    
    // Close the connection
    close(client_socket);
}

// Admin menu
void admin_menu(int client_socket) {
    char buffer[BUFFER_SIZE];
    int choice = 0;
    
    while (1) {
        // Display admin menu
        char *menu = "\n===== ADMIN MENU =====\n1. Add Student\n2. Add Faculty\n3. Activate/Deactivate Student\n4. Update Student/Faculty details\n5. Exit\nEnter your choice: ";
        write(client_socket, menu, strlen(menu));
        
        // Read choice
        memset(buffer, 0, BUFFER_SIZE);
        read(client_socket, buffer, BUFFER_SIZE);
        choice = atoi(buffer);
        
        switch (choice) {
            case 1:
                add_student(client_socket);
                break;
            case 2:
                add_faculty(client_socket);
                break;
            case 3:
                toggle_student_status(client_socket);
                break;
            case 4:
                update_details(client_socket);
                break;
            case 5:
                write(client_socket, "Goodbye!\n", strlen("Goodbye!\n"));
                return;
            default:
                write(client_socket, "Invalid choice\n", strlen("Invalid choice\n"));
        }
    }
}

// Student menu
void student_menu(int client_socket, int student_id) {
    char buffer[BUFFER_SIZE];
    int choice = 0;
    
    while (1) {
        // Display student menu
        char *menu = "\n===== STUDENT MENU =====\n1. Enroll to new Courses\n2. Unenroll from already enrolled Courses\n3. View enrolled Courses\n4. Password Change\n5. Exit\nEnter your choice: ";
        write(client_socket, menu, strlen(menu));
        
        // Read choice
        memset(buffer, 0, BUFFER_SIZE);
        read(client_socket, buffer, BUFFER_SIZE);
        choice = atoi(buffer);
        
        switch (choice) {
            case 1:
                enroll_course(client_socket, student_id);
                break;
            case 2:
                unenroll_course(client_socket, student_id);
                break;
            case 3:
                view_enrolled_courses(client_socket, student_id);
                break;
            case 4:
                change_password(client_socket, "student", student_id);
                break;
            case 5:
                write(client_socket, "Goodbye!\n", strlen("Goodbye!\n"));
                return;
            default:
                write(client_socket, "Invalid choice\n", strlen("Invalid choice\n"));
        }
    }
}

// Faculty menu
void faculty_menu(int client_socket, int faculty_id) {
    char buffer[BUFFER_SIZE];
    int choice = 0;
    
    while (1) {
        // Display faculty menu
        char *menu = "\n===== FACULTY MENU =====\n1. Add new Course\n2. Remove offered Course\n3. View enrollments in Courses\n4. Password Change\n5. Exit\nEnter your choice: ";
        write(client_socket, menu, strlen(menu));
        
        // Read choice
        memset(buffer, 0, BUFFER_SIZE);
        read(client_socket, buffer, BUFFER_SIZE);
        choice = atoi(buffer);
        
        switch (choice) {
            case 1:
                add_course(client_socket, faculty_id);
                break;
            case 2:
                remove_course(client_socket, faculty_id);
                break;
            case 3:
                view_enrollments(client_socket, faculty_id);
                break;
            case 4:
                change_password(client_socket, "faculty", faculty_id);
                break;
            case 5:
                write(client_socket, "Goodbye!\n", strlen("Goodbye!\n"));
                return;
            default:
                write(client_socket, "Invalid choice\n", strlen("Invalid choice\n"));
        }
    }
}

// Authenticate user
int authenticate_user(char *username, char *password, char *role) {
    if (strcmp(role, "admin") == 0) {
        int fd = open("admin.dat", O_RDONLY);
        if (fd == -1) {
            perror("Error opening admin file");
            return -1;
        }
        
        Admin admin;
        read(fd, &admin, sizeof(Admin));
        close(fd);
        
        if (strcmp(admin.username, username) == 0 && strcmp(admin.password, password) == 0) {
            return 0; // Success for admin
        }
    } else if (strcmp(role, "faculty") == 0) {
        int fd = open("faculty.dat", O_RDONLY);
        if (fd == -1) {
            perror("Error opening faculty file");
            printf("hello\n");
            return -1;
        }
        
        Faculty faculty;
        int found = 0, id = 0;
        while (read(fd, &faculty, sizeof(Faculty)) > 0) {
            if (strcmp(faculty.username, username) == 0 && strcmp(faculty.password, password) == 0) {
                found = 1;
                break;
            }
            id++;
        }
        
        close(fd);
        
        if (found) {
            return id; // Return faculty ID
        }
    } else if (strcmp(role, "student") == 0) {
        int fd = open("students.dat", O_RDONLY);
        if (fd == -1) {
            perror("Error opening students file");
            return -1;
        }
        
        Student student;
        int found = 0, id = 0;
        
        while (read(fd, &student, sizeof(Student)) > 0) {
            if (strcmp(student.username, username) == 0 && strcmp(student.password, password) == 0 && student.active) {
                found = 1;
                break;
            }
            id++;
        }
        
        close(fd);
        
        if (found) {
            return id; // Return student ID
        }
    }
    
    return -1; // Authentication failed
}

// Add student (Admin function)
void add_student(int client_socket) {
    char buffer[BUFFER_SIZE];
    Student new_student;
    
    // Get student details
    write(client_socket, "Enter student username: ", strlen("Enter student username: "));
    memset(buffer, 0, BUFFER_SIZE);
    read(client_socket, buffer, BUFFER_SIZE);
    strcpy(new_student.username, buffer);
    
    write(client_socket, "Enter student password: ", strlen("Enter student password: "));
    memset(buffer, 0, BUFFER_SIZE);
    read(client_socket, buffer, BUFFER_SIZE);
    strcpy(new_student.password, buffer);
    
    // Initialize other fields
    new_student.active = 1;
    new_student.course_count = 0;
    
    // Acquire lock for students file
    pthread_mutex_lock(&student_mutex);
    
    // Open students file and find next available ID
    int fd = open("students.dat", O_RDWR | O_APPEND);
    if (fd == -1) {
        perror("Error opening students file");
        pthread_mutex_unlock(&student_mutex);
        write(client_socket, "Failed to add student\n", strlen("Failed to add student\n"));
        return;
    }
    
    // Determine student ID (count records in file)
    struct stat st;
    fstat(fd, &st);
    new_student.id = st.st_size / sizeof(Student);
    
    // Write new student to file
    write(fd, &new_student, sizeof(Student));
    close(fd);
    
    // Release lock
    pthread_mutex_unlock(&student_mutex);
    
    char response[100];
    sprintf(response, "Student added successfully with ID: %d\n", new_student.id);
    write(client_socket, response, strlen(response));
}

// Add faculty (Admin function)
void add_faculty(int client_socket) {
    char buffer[BUFFER_SIZE];
    Faculty new_faculty;
    
    // Get faculty details
    write(client_socket, "Enter faculty username: ", strlen("Enter faculty username: "));
    memset(buffer, 0, BUFFER_SIZE);
    read(client_socket, buffer, BUFFER_SIZE);
    strcpy(new_faculty.username, buffer);
    
    write(client_socket, "Enter faculty password: ", strlen("Enter faculty password: "));
    memset(buffer, 0, BUFFER_SIZE);
    read(client_socket, buffer, BUFFER_SIZE);
    strcpy(new_faculty.password, buffer);

    // Initialize other fields
    new_faculty.course_count = 0;
    
    // Acquire lock for faculty file
    pthread_mutex_lock(&faculty_mutex);
    
    // Open faculty file and find next available ID
    int fd = open("faculty.dat", O_RDWR | O_APPEND);
    if (fd == -1) {
        perror("Error opening faculty file");
        pthread_mutex_unlock(&faculty_mutex);
        write(client_socket, "Failed to add faculty\n", strlen("Failed to add faculty\n"));
        return;
    }
    
    // Determine faculty ID (count records in file)
    struct stat st;
    fstat(fd, &st);
    new_faculty.id = st.st_size / sizeof(Faculty);
    
    // Write new faculty to file
    write(fd, &new_faculty, sizeof(Faculty));
    close(fd);
    
    // Release lock
    pthread_mutex_unlock(&faculty_mutex);
    
    char response[100];
    sprintf(response, "Faculty added successfully with ID: %d\n", new_faculty.id);
    write(client_socket, response, strlen(response));
}

// Activate/Deactivate student (Admin function)
void toggle_student_status(int client_socket) {
    char buffer[BUFFER_SIZE];
    int student_id;
    
    // Get student ID
    write(client_socket, "Enter student ID: ", strlen("Enter student ID: "));
    memset(buffer, 0, BUFFER_SIZE);
    read(client_socket, buffer, BUFFER_SIZE);
    student_id = atoi(buffer);
    
    // Acquire lock for students file
    pthread_mutex_lock(&student_mutex);
    
    // Open students file
    int fd = open("students.dat", O_RDWR);
    if (fd == -1) {
        perror("Error opening students file");
        pthread_mutex_unlock(&student_mutex);
        write(client_socket, "Failed to toggle student status\n", strlen("Failed to toggle student status\n"));
        return;
    }
    
    // Find the student by ID
    Student student;
    lseek(fd, student_id * sizeof(Student), SEEK_SET);
    if (read(fd, &student, sizeof(Student)) != sizeof(Student)) {
        close(fd);
        pthread_mutex_unlock(&student_mutex);
        write(client_socket, "Student not found\n", strlen("Student not found\n"));
        return;
    }
    
    // Toggle active status
    student.active = !student.active;
    
    // Write back to file
    lseek(fd, student_id * sizeof(Student), SEEK_SET);
    write(fd, &student, sizeof(Student));
    close(fd);
    
    // Release lock
    pthread_mutex_unlock(&student_mutex);
    
    char status[20] = {0};
    strcpy(status, student.active ? "activated" : "deactivated");
    
    char response[100];
    sprintf(response, "Student %s successfully\n", status);
    write(client_socket, response, strlen(response));
}

// Update student/faculty details (Admin function)
void update_details(int client_socket) {
    char buffer[BUFFER_SIZE];
    int choice, id;
    
    // Ask for role to update
    write(client_socket, "Update: 1. Student 2. Faculty\nEnter choice: ", strlen("Update: 1. Student 2. Faculty\nEnter choice: "));
    memset(buffer, 0, BUFFER_SIZE);
    read(client_socket, buffer, BUFFER_SIZE);
    choice = atoi(buffer);
    
    // Ask for ID
    write(client_socket, "Enter ID: ", strlen("Enter ID: "));
    memset(buffer, 0, BUFFER_SIZE);
    read(client_socket, buffer, BUFFER_SIZE);
    id = atoi(buffer);
    
    if (choice == 1) { // Update student
        // Acquire lock for students file
        pthread_mutex_lock(&student_mutex);
        
        // Open students file
        int fd = open("students.dat", O_RDWR);
        if (fd == -1) {
            perror("Error opening students file");
            pthread_mutex_unlock(&student_mutex);
            write(client_socket, "Failed to update student\n", strlen("Failed to update student\n"));
            return;
        }
        
        // Find the student by ID
        Student student;
        lseek(fd, id * sizeof(Student), SEEK_SET);
        if (read(fd, &student, sizeof(Student)) != sizeof(Student)) {
            close(fd);
            pthread_mutex_unlock(&student_mutex);
            write(client_socket, "Student not found\n", strlen("Student not found\n"));
            return;
        }
        
        // Get new details
        write(client_socket, "Enter new username (or . to keep current): ", strlen("Enter new username (or . to keep current): "));
        memset(buffer, 0, BUFFER_SIZE);
        read(client_socket, buffer, BUFFER_SIZE);
        if (strcmp(buffer, ".") != 0) {
            strcpy(student.username, buffer);
        }
        
        write(client_socket, "Enter new password (or . to keep current): ", strlen("Enter new password (or . to keep current): "));
        memset(buffer, 0, BUFFER_SIZE);
        read(client_socket, buffer, BUFFER_SIZE);
        if (strcmp(buffer, ".") != 0) {
            strcpy(student.password, buffer);
        }
        
        // Write back to file
        lseek(fd, id * sizeof(Student), SEEK_SET);
        write(fd, &student, sizeof(Student));
        close(fd);
        
        // Release lock
        pthread_mutex_unlock(&student_mutex);
        
        write(client_socket, "Student details updated successfully\n", strlen("Student details updated successfully\n"));
    } else if (choice == 2) { // Update faculty
        // Acquire lock for faculty file
        pthread_mutex_lock(&faculty_mutex);
        
        // Open faculty file
        int fd = open("faculty.dat", O_RDWR);
        if (fd == -1) {
            perror("Error opening faculty file");
            pthread_mutex_unlock(&faculty_mutex);
            write(client_socket, "Failed to update faculty\n", strlen("Failed to update faculty\n"));
            return;
        }
        
        // Find the faculty by ID
        Faculty faculty;
        lseek(fd, id * sizeof(Faculty), SEEK_SET);
        if (read(fd, &faculty, sizeof(Faculty)) != sizeof(Faculty)) {
            close(fd);
            pthread_mutex_unlock(&faculty_mutex);
            write(client_socket, "Faculty not found\n", strlen("Faculty not found\n"));
            return;
        }
        
        // Get new details
        write(client_socket, "Enter new username (or . to keep current): ", strlen("Enter new username (or . to keep current): "));
        memset(buffer, 0, BUFFER_SIZE);
        read(client_socket, buffer, BUFFER_SIZE);
        if (strcmp(buffer, ".") != 0) {
            strcpy(faculty.username, buffer);
        }
        
        write(client_socket, "Enter new password (or . to keep current): ", strlen("Enter new password (or . to keep current): "));
        memset(buffer, 0, BUFFER_SIZE);
        read(client_socket, buffer, BUFFER_SIZE);
        if (strcmp(buffer, ".") != 0) {
            strcpy(faculty.password, buffer);
        }
        
        // Write back to file
        lseek(fd, id * sizeof(Faculty), SEEK_SET);
        write(fd, &faculty, sizeof(Faculty));
        close(fd);
        
        // Release lock
        pthread_mutex_unlock(&faculty_mutex);
        
        write(client_socket, "Faculty details updated successfully\n", strlen("Faculty details updated successfully\n"));
    } else {
        write(client_socket, "Invalid choice\n", strlen("Invalid choice\n"));
    }
}

// Enroll in a course (Student function)
void enroll_course(int client_socket, int student_id) {
    char buffer[BUFFER_SIZE], course_list[BUFFER_SIZE];
    char course_name[50];
    
    // Acquire read lock for courses (to display available courses)
    pthread_mutex_lock(&course_mutex);
    
    // Open faculty file to get available courses
    int fd = open("faculty.dat", O_RDONLY);
    if (fd == -1) {
        perror("Error opening faculty file");
        pthread_mutex_unlock(&course_mutex);
        write(client_socket, "Failed to get available courses\n", strlen("Failed to get available courses\n"));
        return;
    }
    
    // Build list of available courses with seats
    Faculty faculty;
    strcpy(course_list, "Available Courses:\n");
    
    while (read(fd, &faculty, sizeof(Faculty)) > 0) {
        for (int i = 0; i < faculty.course_count; i++) {
            if (faculty.seats[i] > 0) {
                char course_info[100];
                sprintf(course_info, "- %s (Available seats: %d)\n", faculty.courses[i], faculty.seats[i]);
                strcat(course_list, course_info);
            }
        }
    }
    close(fd);
    
    // Release read lock
    pthread_mutex_unlock(&course_mutex);
    
    // Send course list to client
    write(client_socket, course_list, strlen(course_list));
    
    // Ask for course to enroll
    write(client_socket, "Enter course name to enroll: ", strlen("Enter course name to enroll: "));
    memset(buffer, 0, BUFFER_SIZE);
    read(client_socket, buffer, BUFFER_SIZE);
    strcpy(course_name, buffer);
    
    // Acquire write lock for course enrollment
    pthread_mutex_lock(&course_mutex);
    pthread_mutex_lock(&student_mutex);
    
    // Check if student already enrolled in this course
    fd = open("students.dat", O_RDWR);
    if (fd == -1) {
        perror("Error opening students file");
        pthread_mutex_unlock(&student_mutex);
        pthread_mutex_unlock(&course_mutex);
        write(client_socket, "Failed to enroll in course\n", strlen("Failed to enroll in course\n"));
        return;
    }
    
    Student student;
    lseek(fd, student_id * sizeof(Student), SEEK_SET);
    if (read(fd, &student, sizeof(Student)) != sizeof(Student)) {
        close(fd);
        pthread_mutex_unlock(&student_mutex);
        pthread_mutex_unlock(&course_mutex);
        write(client_socket, "Student not found\n", strlen("Student not found\n"));
        return;
    }
    
    // Check if already enrolled
    for (int i = 0; i < student.course_count; i++) {
        if (strcmp(student.courses[i], course_name) == 0) {
            close(fd);
            pthread_mutex_unlock(&student_mutex);
            pthread_mutex_unlock(&course_mutex);
            write(client_socket, "Already enrolled in this course\n", strlen("Already enrolled in this course\n"));
            return;
        }
    }
    
    // Check course availability and reduce seats
    int faculty_fd = open("faculty.dat", O_RDWR);
    if (faculty_fd == -1) {
        perror("Error opening faculty file");
        close(fd);
        pthread_mutex_unlock(&student_mutex);
        pthread_mutex_unlock(&course_mutex);
        write(client_socket, "Failed to enroll in course\n", strlen("Failed to enroll in course\n"));
        return;
    }
    
    int course_found = 0, faculty_id = -1;
    int course_index = -1;
    
    while (read(faculty_fd, &faculty, sizeof(Faculty)) > 0) {
        for (int i = 0; i < faculty.course_count; i++) {
            if (strcmp(faculty.courses[i], course_name) == 0 && faculty.seats[i] > 0) {
                course_found = 1;
                faculty_id = faculty.id;
                course_index = i;
                faculty.seats[i]--; // Reduce available seats
                break;
            }
        }
        if (course_found) break;
    }
    
    if (!course_found) {
        close(fd);
        close(faculty_fd);
        pthread_mutex_unlock(&student_mutex);
        pthread_mutex_unlock(&course_mutex);
        write(client_socket, "Course not found or no seats available\n", strlen("Course not found or no seats available\n"));
        return;
    }
    
    // Update faculty file with reduced seats
    lseek(faculty_fd, faculty_id * sizeof(Faculty), SEEK_SET);
    write(faculty_fd, &faculty, sizeof(Faculty));
    close(faculty_fd);
    
    // Add course to student's enrolled courses
    strcpy(student.courses[student.course_count], course_name);
    student.course_count++;
    
    // Update student file
    lseek(fd, student_id * sizeof(Student), SEEK_SET);
    write(fd, &student, sizeof(Student));
    close(fd);
    
    // Release locks
    pthread_mutex_unlock(&student_mutex);
    pthread_mutex_unlock(&course_mutex);
    
    write(client_socket, "Successfully enrolled in course\n", strlen("Successfully enrolled in course\n"));
}

// Unenroll from a course (Student function)
void unenroll_course(int client_socket, int student_id) {
    char buffer[BUFFER_SIZE], enrolled_courses[BUFFER_SIZE];
    char course_name[50];
    
    // Acquire read lock for student courses
    pthread_mutex_lock(&student_mutex);
    
    // Open students file to get enrolled courses
    int fd = open("students.dat", O_RDONLY);
    if (fd == -1) {
        perror("Error opening students file");
        pthread_mutex_unlock(&student_mutex);
        write(client_socket, "Failed to get enrolled courses\n", strlen("Failed to get enrolled courses\n"));
        return;
    }
    
    Student student;
    lseek(fd, student_id * sizeof(Student), SEEK_SET);
    if (read(fd, &student, sizeof(Student)) != sizeof(Student)) {
        close(fd);
        pthread_mutex_unlock(&student_mutex);
        write(client_socket, "Student not found\n", strlen("Student not found\n"));
        return;
    }
    close(fd);
    
    // Build list of enrolled courses
    strcpy(enrolled_courses, "Your enrolled courses:\n");
    if (student.course_count == 0) {
        strcat(enrolled_courses, "No courses enrolled\n");
    } else {
        for (int i = 0; i < student.course_count; i++) {
            char course_info[100];
            sprintf(course_info, "- %s\n", student.courses[i]);
            strcat(enrolled_courses, course_info);
        }
    }
    
    // Release read lock
    pthread_mutex_unlock(&student_mutex);
    
    // Send enrolled courses to client
    write(client_socket, enrolled_courses, strlen(enrolled_courses));
    
    if (student.course_count == 0) {
        return;
    }
    
    // Ask for course to unenroll
    write(client_socket, "Enter course name to unenroll: ", strlen("Enter course name to unenroll: "));
    memset(buffer, 0, BUFFER_SIZE);
    read(client_socket, buffer, BUFFER_SIZE);
    strcpy(course_name, buffer);
    
    // Acquire write lock for unenrollment
    pthread_mutex_lock(&course_mutex);
    pthread_mutex_lock(&student_mutex);
    
    // Open students file
    fd = open("students.dat", O_RDWR);
    if (fd == -1) {
        perror("Error opening students file");
        pthread_mutex_unlock(&student_mutex);
        pthread_mutex_unlock(&course_mutex);
        write(client_socket, "Failed to unenroll from course\n", strlen("Failed to unenroll from course\n"));
        return;
    }
    
    // Read student record again
    lseek(fd, student_id * sizeof(Student), SEEK_SET);
    if (read(fd, &student, sizeof(Student)) != sizeof(Student)) {
        close(fd);
        pthread_mutex_unlock(&student_mutex);
        pthread_mutex_unlock(&course_mutex);
        write(client_socket, "Student not found\n", strlen("Student not found\n"));
        return;
    }
    
    // Find and remove the course
    int course_found = 0;
    for (int i = 0; i < student.course_count; i++) {
        if (strcmp(student.courses[i], course_name) == 0) {
            course_found = 1;
            // Shift remaining courses
            for (int j = i; j < student.course_count - 1; j++) {
                strcpy(student.courses[j], student.courses[j + 1]);
            }
            student.course_count--;
            break;
        }
    }
    
    if (!course_found) {
        close(fd);
        pthread_mutex_unlock(&student_mutex);
        pthread_mutex_unlock(&course_mutex);
        write(client_socket, "Course not found in your enrolled courses\n", strlen("Course not found in your enrolled courses\n"));
        return;
    }
    
    // Update student file
    lseek(fd, student_id * sizeof(Student), SEEK_SET);
    write(fd, &student, sizeof(Student));
    close(fd);
    
    // Increase available seats for the course
    int faculty_fd = open("faculty.dat", O_RDWR);
    if (faculty_fd == -1) {
        perror("Error opening faculty file");
        pthread_mutex_unlock(&student_mutex);
        pthread_mutex_unlock(&course_mutex);
        write(client_socket, "Warning: Failed to update course seats\n", strlen("Warning: Failed to update course seats\n"));
        return;
    }
    
    Faculty faculty;
    int faculty_id = -1;
    int course_index = -1;
    
    while (read(faculty_fd, &faculty, sizeof(Faculty)) > 0) {
        for (int i = 0; i < faculty.course_count; i++) {
            if (strcmp(faculty.courses[i], course_name) == 0) {
                faculty_id = faculty.id;
                course_index = i;
                faculty.seats[i]++; // Increase available seats
                break;
            }
        }
        if (faculty_id != -1) break;
    }
    
    if (faculty_id != -1) {
        lseek(faculty_fd, faculty_id * sizeof(Faculty), SEEK_SET);
        write(faculty_fd, &faculty, sizeof(Faculty));
    }
    
    close(faculty_fd);
    
    // Release locks
    pthread_mutex_unlock(&student_mutex);
    pthread_mutex_unlock(&course_mutex);
    
    write(client_socket, "Successfully unenrolled from course\n", strlen("Successfully unenrolled from course\n"));
}

// View enrolled courses (Student function)
void view_enrolled_courses(int client_socket, int student_id) {
    // Acquire read lock for students file
    pthread_mutex_lock(&student_mutex);
    
    // Open students file
    int fd = open("students.dat", O_RDONLY);
    if (fd == -1) {
        perror("Error opening students file");
        pthread_mutex_unlock(&student_mutex);
        write(client_socket, "Failed to view enrolled courses\n", strlen("Failed to view enrolled courses\n"));
        return;
    }
    
    // Read student record
    Student student;
    lseek(fd, student_id * sizeof(Student), SEEK_SET);
    if (read(fd, &student, sizeof(Student)) != sizeof(Student)) {
        close(fd);
        pthread_mutex_unlock(&student_mutex);
        write(client_socket, "Student not found\n", strlen("Student not found\n"));
        return;
    }
    
    close(fd);
    
    // Release lock
    pthread_mutex_unlock(&student_mutex);
    
    // Build and send course list
    char course_list[BUFFER_SIZE];
    strcpy(course_list, "\n=== Your Enrolled Courses ===\n");
    
    if (student.course_count == 0) {
        strcat(course_list, "You are not enrolled in any courses.\n");
    } else {
        char count_info[50];
        sprintf(count_info, "Total courses enrolled: %d\n\n", student.course_count);
        strcat(course_list, count_info);
        
        for (int i = 0; i < student.course_count; i++) {
            char course_info[100];
            sprintf(course_info, "%d. %s\n", i + 1, student.courses[i]);
            strcat(course_list, course_info);
        }
    }
    
    write(client_socket, course_list, strlen(course_list));
}

// Change password (Common function for all roles)
void change_password(int client_socket, char *role, int id) {
    char buffer[BUFFER_SIZE];
    char old_password[50], new_password[50], confirm_password[50];
    
    // Get old password
    write(client_socket, "Enter old password: ", strlen("Enter old password: "));
    memset(buffer, 0, BUFFER_SIZE);
    read(client_socket, buffer, BUFFER_SIZE);
    strcpy(old_password, buffer);
    
    // Get new password
    write(client_socket, "Enter new password: ", strlen("Enter new password: "));
    memset(buffer, 0, BUFFER_SIZE);
    read(client_socket, buffer, BUFFER_SIZE);
    strcpy(new_password, buffer);
    
    // Confirm new password
    write(client_socket, "Confirm new password: ", strlen("Confirm new password: "));
    memset(buffer, 0, BUFFER_SIZE);
    read(client_socket, buffer, BUFFER_SIZE);
    strcpy(confirm_password, buffer);
    
    // Check if new passwords match
    if (strcmp(new_password, confirm_password) != 0) {
        write(client_socket, "New passwords do not match\n", strlen("New passwords do not match\n"));
        return;
    }
    
    if (strcmp(role, "student") == 0) {
        pthread_mutex_lock(&student_mutex);
        
        int fd = open("students.dat", O_RDWR);
        if (fd == -1) {
            perror("Error opening students file");
            pthread_mutex_unlock(&student_mutex);
            write(client_socket, "Failed to change password\n", strlen("Failed to change password\n"));
            return;
        }
        
        Student student;
        lseek(fd, id * sizeof(Student), SEEK_SET);
        if (read(fd, &student, sizeof(Student)) != sizeof(Student)) {
            close(fd);
            pthread_mutex_unlock(&student_mutex);
            write(client_socket, "Student not found\n", strlen("Student not found\n"));
            return;
        }
        
        // Verify old password
        if (strcmp(student.password, old_password) != 0) {
            close(fd);
            pthread_mutex_unlock(&student_mutex);
            write(client_socket, "Incorrect old password\n", strlen("Incorrect old password\n"));
            return;
        }
        
        // Update password
        strcpy(student.password, new_password);
        lseek(fd, id * sizeof(Student), SEEK_SET);
        write(fd, &student, sizeof(Student));
        close(fd);
        
        pthread_mutex_unlock(&student_mutex);
    } else if (strcmp(role, "faculty") == 0) {
        pthread_mutex_lock(&faculty_mutex);
        
        int fd = open("faculty.dat", O_RDWR);
        if (fd == -1) {
            perror("Error opening faculty file");
            pthread_mutex_unlock(&faculty_mutex);
            write(client_socket, "Failed to change password\n", strlen("Failed to change password\n"));
            return;
        }
        
        Faculty faculty;
        lseek(fd, id * sizeof(Faculty), SEEK_SET);
        if (read(fd, &faculty, sizeof(Faculty)) != sizeof(Faculty)) {
            close(fd);
            pthread_mutex_unlock(&faculty_mutex);
            write(client_socket, "Faculty not found\n", strlen("Faculty not found\n"));
            return;
        }
        
        // Verify old password
        if (strcmp(faculty.password, old_password) != 0) {
            close(fd);
            pthread_mutex_unlock(&faculty_mutex);
            write(client_socket, "Incorrect old password\n", strlen("Incorrect old password\n"));
            return;
        }
        
        // Update password
        strcpy(faculty.password, new_password);
        lseek(fd, id * sizeof(Faculty), SEEK_SET);
        write(fd, &faculty, sizeof(Faculty));
        close(fd);
        
        pthread_mutex_unlock(&faculty_mutex);
    }
    
    write(client_socket, "Password changed successfully\n", strlen("Password changed successfully\n"));
}

// Add a new course (Faculty function)
void add_course(int client_socket, int faculty_id) {
    char buffer[BUFFER_SIZE];
    char course_name[50];
    int seats;

    // Get course details
    write(client_socket, "Enter course name: ", strlen("Enter course name: "));
    memset(buffer, 0, BUFFER_SIZE);
    read(client_socket, buffer, BUFFER_SIZE);
    strcpy(course_name, buffer);

    // Check if course already exists
    if (check_course_exists(course_name)) {
        write(client_socket, "Course already exists\n", strlen("Course already exists\n"));
        return;
    }

    write(client_socket, "Enter number of seats: ", strlen("Enter number of seats: "));
    memset(buffer, 0, BUFFER_SIZE);
    read(client_socket, buffer, BUFFER_SIZE);
    seats = atoi(buffer);

    if (seats <= 0 || seats > MAX_SEATS) {
        write(client_socket, "Invalid number of seats\n", strlen("Invalid number of seats\n"));
        return;
    }

    // Acquire lock for faculty file
    pthread_mutex_lock(&faculty_mutex);

    // Open faculty file
    int fd = open("faculty.dat", O_RDWR);
    if (fd == -1) {
        perror("Error opening faculty file");
        pthread_mutex_unlock(&faculty_mutex);
        write(client_socket, "Failed to add course\n", strlen("Failed to add course\n"));
        return;
    }

    // Read faculty record
    Faculty faculty;
    lseek(fd, faculty_id * sizeof(Faculty), SEEK_SET);
    if (read(fd, &faculty, sizeof(Faculty)) != sizeof(Faculty)) {
        close(fd);
        pthread_mutex_unlock(&faculty_mutex);
        write(client_socket, "Faculty not found\n", strlen("Faculty not found\n"));
        return;
    }

    // Check if faculty can add more courses
    if (faculty.course_count >= MAX_COURSES) {
        close(fd);
        pthread_mutex_unlock(&faculty_mutex);
        write(client_socket, "Maximum courses limit reached\n", strlen("Maximum courses limit reached\n"));
        return;
    }

    // Add course to faculty's course list
    // Add course to faculty's course list
    strcpy(faculty.courses[faculty.course_count], course_name);
    faculty.seats[faculty.course_count] = seats; // Available seats
    faculty.initial_seats[faculty.course_count] = seats; // Store initial seats
    faculty.course_count++; // Increment the course count

    // Update faculty file
    lseek(fd, faculty_id * sizeof(Faculty), SEEK_SET);
    write(fd, &faculty, sizeof(Faculty));
    close(fd);

    // Release lock
    pthread_mutex_unlock(&faculty_mutex);

    write(client_socket, "Course added successfully\n", strlen("Course added successfully\n"));
}
// Remove an offered course (Faculty function)
void remove_course(int client_socket, int faculty_id) {
    char buffer[BUFFER_SIZE], course_list[BUFFER_SIZE];
    char course_name[50];
    
    // Acquire lock for faculty file
    pthread_mutex_lock(&faculty_mutex);
    
    // Open faculty file
    int fd = open("faculty.dat", O_RDWR);
    if (fd == -1) {
        perror("Error opening faculty file");
        pthread_mutex_unlock(&faculty_mutex);
        write(client_socket, "Failed to get offered courses\n", strlen("Failed to get offered courses\n"));
        return;
    }
    
    // Read faculty record
    Faculty faculty;
    lseek(fd, faculty_id * sizeof(Faculty), SEEK_SET);
    if (read(fd, &faculty, sizeof(Faculty)) != sizeof(Faculty)) {
        close(fd);
        pthread_mutex_unlock(&faculty_mutex);
        write(client_socket, "Faculty not found\n", strlen("Faculty not found\n"));
        return;
    }
    
    // Build list of offered courses
    strcpy(course_list, "Your offered courses:\n");
    if (faculty.course_count == 0) {
        strcat(course_list, "No courses offered\n");
        close(fd);
        pthread_mutex_unlock(&faculty_mutex);
        write(client_socket, course_list, strlen(course_list));
        return;
    }
    
    for (int i = 0; i < faculty.course_count; i++) {
        char course_info[100];
        sprintf(course_info, "- %s (Seats: %d)\n", faculty.courses[i], faculty.seats[i]);
        strcat(course_list, course_info);
    }
    
    // Send course list to client
    write(client_socket, course_list, strlen(course_list));
    
    // Ask for course to remove
    write(client_socket, "Enter course name to remove: ", strlen("Enter course name to remove: "));
    memset(buffer, 0, BUFFER_SIZE);
    read(client_socket, buffer, BUFFER_SIZE);
    strcpy(course_name, buffer);
    
    // Find and remove the course
    int course_found = 0;
    for (int i = 0; i < faculty.course_count; i++) {
        if (strcmp(faculty.courses[i], course_name) == 0) {
            course_found = 1;
            // Shift remaining courses
            for (int j = i; j < faculty.course_count - 1; j++) {
                strcpy(faculty.courses[j], faculty.courses[j + 1]);
                faculty.seats[j] = faculty.seats[j + 1];
            }
            faculty.course_count--;
            break;
        }
    }
    
    if (!course_found) {
        close(fd);
        pthread_mutex_unlock(&faculty_mutex);
        write(client_socket, "Course not found in your offered courses\n", strlen("Course not found in your offered courses\n"));
        return;
    }
    
    // Update faculty file
    lseek(fd, faculty_id * sizeof(Faculty), SEEK_SET);
    write(fd, &faculty, sizeof(Faculty));
    close(fd);
    
    // Release lock
    pthread_mutex_unlock(&faculty_mutex);
    
    // Now remove course from all enrolled students
    pthread_mutex_lock(&student_mutex);
    
    fd = open("students.dat", O_RDWR);
    if (fd == -1) {
        perror("Error opening students file");
        pthread_mutex_unlock(&student_mutex);
        write(client_socket, "Course removed, but warning: failed to update student enrollments\n", strlen("Course removed, but warning: failed to update student enrollments\n"));
        return;
    }
    
    // Update all students who have this course
    Student student;
    int student_count = 0;
    while (read(fd, &student, sizeof(Student)) > 0) {
        int modified = 0;
        for (int i = 0; i < student.course_count; i++) {
            if (strcmp(student.courses[i], course_name) == 0) {
                // Remove course from student
                for (int j = i; j < student.course_count - 1; j++) {
                    strcpy(student.courses[j], student.courses[j + 1]);
                }
                student.course_count--;
                modified = 1;
                break;
            }
        }
        
        if (modified) {
            // Write back updated student record
            lseek(fd, student_count * sizeof(Student), SEEK_SET);
            write(fd, &student, sizeof(Student));
        }
        student_count++;
    }
    
    close(fd);
    pthread_mutex_unlock(&student_mutex);
    
    write(client_socket, "Course removed successfully\n", strlen("Course removed successfully\n"));
}

// View enrollments in courses (Faculty function)
void view_enrollments(int client_socket, int faculty_id) {
    char enrollment_list[BUFFER_SIZE];
    
    // Acquire read lock for faculty file
    pthread_mutex_lock(&faculty_mutex);
    
    // Open faculty file
    int fd = open("faculty.dat", O_RDONLY);
    if (fd == -1) {
        perror("Error opening faculty file");
        pthread_mutex_unlock(&faculty_mutex);
        write(client_socket, "Failed to view enrollments\n", strlen("Failed to view enrollments\n"));
        return;
    }
    
    // Read faculty record
    Faculty faculty;
    lseek(fd, faculty_id * sizeof(Faculty), SEEK_SET);
    if (read(fd, &faculty, sizeof(Faculty)) != sizeof(Faculty)) {
        close(fd);
        pthread_mutex_unlock(&faculty_mutex);
        write(client_socket, "Faculty not found\n", strlen("Faculty not found\n"));
        return;
    }
    
    close(fd);
    pthread_mutex_unlock(&faculty_mutex);
    
    // Build enrollment list
    strcpy(enrollment_list, "\n=== Course Enrollments ===\n");
    
    if (faculty.course_count == 0) {
        strcat(enrollment_list, "You have not offered any courses.\n");
        write(client_socket, enrollment_list, strlen(enrollment_list));
        return;
    }
    
    // For each course, show enrolled students
    for (int i = 0; i < faculty.course_count; i++) {
        char course_info[200];
        int enrolled_count = faculty.initial_seats[i] - faculty.seats[i]; // Calculate enrolled students
        sprintf(course_info, "\nCourse: %s\nEnrolled Students: %d/%d\n", faculty.courses[i], enrolled_count, faculty.initial_seats[i]); // Use initial_seats
        strcat(enrollment_list, course_info);
        
        // Get list of enrolled students
        if (enrolled_count > 0) {
            strcat(enrollment_list, "Students enrolled:\n");
            
            // Acquire read lock for students file
            pthread_mutex_lock(&student_mutex);
            
            fd = open("students.dat", O_RDONLY);
            if (fd != -1) {
                Student student;
                while (read(fd, &student, sizeof(Student)) > 0) {
                    for (int j = 0; j < student.course_count; j++) {
                        if (strcmp(student.courses[j], faculty.courses[i]) == 0) {
                            char student_info[100];
                            sprintf(student_info, "  - %s (ID: %d)\n", student.username, student.id);
                            strcat(enrollment_list, student_info);
                            break;
                        }
                    }
                }
                close(fd);
            }
            
            pthread_mutex_unlock(&student_mutex);
        } else {
            strcat(enrollment_list, "No students enrolled yet.\n");
        }
        
        strcat(enrollment_list, "------------------------\n");
    }
    
    write(client_socket, enrollment_list, strlen(enrollment_list));
}

// Check if a course exists (Helper function)
int check_course_exists(char *course_name) {
    pthread_mutex_lock(&faculty_mutex);
    
    int fd = open("faculty.dat", O_RDONLY);
    if (fd == -1) {
        pthread_mutex_unlock(&faculty_mutex);
        return 0;
    }
    
    Faculty faculty;
    int exists = 0;
    
    while (read(fd, &faculty, sizeof(Faculty)) > 0) {
        for (int i = 0; i < faculty.course_count; i++) {
            if (strcmp(faculty.courses[i], course_name) == 0) {
                exists = 1;
                break;
            }
        }
        if (exists) break;
    }
    
    close(fd);
    pthread_mutex_unlock(&faculty_mutex);
    
    return exists;
}