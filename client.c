/**
 * Client implementation for Academia Portal
 * Course Registration System
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <termios.h>

#define PORT 8080
#define SERVER_IP "127.0.0.1"
#define BUFFER_SIZE 1024

// Function to get password input without echoing
void get_password(char *password, int max_len) {
    // Read password
    fgets(password, max_len, stdin);
    
    // Remove trailing newline
    password[strcspn(password, "\n")] = 0;
}

// Function to receive and display server response
int receive_response(int socket_fd, char *response_buffer) {
    char buffer[BUFFER_SIZE];
    memset(buffer, 0, BUFFER_SIZE);
    
    int bytes_received = read(socket_fd, buffer, BUFFER_SIZE - 1);
    if (bytes_received > 0) {
        buffer[bytes_received] = '\0';
        printf("%s", buffer);
        
        // Copy to response buffer if provided
        if (response_buffer != NULL) {
            strcpy(response_buffer, buffer);
        }
        
        return bytes_received;
    }
    return 0;
}

// Function to clear the screen
void clear_screen() {
    #ifdef _WIN32
        system("cls");
    #else
        system("clear");
    #endif
}

// Function to send user input to server
void send_input(int socket_fd, const char *input) {
    write(socket_fd, input, strlen(input) + 1);
}

int main() {
    int socket_fd;
    struct sockaddr_in server_addr;
    char buffer[BUFFER_SIZE];
    char server_response[BUFFER_SIZE];
    
    // Create socket
    if ((socket_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }
    
    // Prepare the sockaddr_in structure
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    
    // Convert IP address from text to binary form
    if (inet_pton(AF_INET, SERVER_IP, &server_addr.sin_addr) <= 0) {
        perror("Invalid address/Address not supported");
        exit(EXIT_FAILURE);
    }
    
    // Connect to the server
    if (connect(socket_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Connection failed");
        exit(EXIT_FAILURE);
    }
    
    printf("Connected to Academia Portal Server\n\n");
    
    // Receive welcome message and role selection
    receive_response(socket_fd, NULL);
    
    // Send role choice
    fgets(buffer, BUFFER_SIZE, stdin);
    buffer[strcspn(buffer, "\n")] = 0;
    send_input(socket_fd, buffer);
    
    // Receive username prompt
    receive_response(socket_fd, NULL);
    
    // Send username
    fgets(buffer, BUFFER_SIZE, stdin);
    buffer[strcspn(buffer, "\n")] = 0;
    send_input(socket_fd, buffer);
    
    // Receive password prompt
    receive_response(socket_fd, NULL);
    
    // Send password (hidden input)
    char password[50];
    // get_password(password, sizeof(password));
    fgets(password, sizeof(password), stdin);
    password[strcspn(password, "\n")] = 0;
    send_input(socket_fd, password);
    
    // Receive login result
    receive_response(socket_fd, server_response);
    
    // Check if login was successful
    if (strstr(server_response, "Login failed") != NULL) {
        close(socket_fd);
        return 1;
    }
    
    // If successful, continue with the menu loop
    int count = 0;
    while (1) {
        // Receive menu
        if(count != 0){
            receive_response(socket_fd, server_response);
        }
        count++;
        // Get user choice
        fgets(buffer, BUFFER_SIZE, stdin);
        buffer[strcspn(buffer, "\n")] = 0;
        
        // Send choice to server
        send_input(socket_fd, buffer);
        
        // Check if user wants to exit
        int choice = atoi(buffer);
        if ((choice == 5) || (choice == 4 && strstr(server_response, "FACULTY") != NULL) || 
            (choice == 5 && strstr(server_response, "STUDENT") != NULL)) {
            // Exit chosen in any menu
            receive_response(socket_fd, NULL);
            break;
        }
        int outside =0;        
        // For all other options, continue the interaction
        while (1) {
            // Receive server response
            int bytes = receive_response(socket_fd, server_response);
            if (bytes <= 0) break;
            
            // Check if we need more input by looking for prompt indicators
            if (strstr(server_response, "Enter") != NULL ||
                strrchr(server_response, ':') == (server_response + strlen(server_response) - 2) ||
                strrchr(server_response, '?') == (server_response + strlen(server_response) - 2)) {
                
                // Server is asking for input
                fgets(buffer, BUFFER_SIZE, stdin);
                buffer[strcspn(buffer, "\n")] = 0;
                
                // Special handling for password input
                // if (strstr(server_response, "password") || strstr(server_response, "Password")) {
                //     get_password(buffer, BUFFER_SIZE);
                // }
                
                send_input(socket_fd, buffer);
            } else if (strstr(server_response, "successfully") != NULL ||
                      strstr(server_response, "failed") != NULL ||
                      strstr(server_response, "Invalid") != NULL ||
                      strstr(server_response, "not found") != NULL) {
                // Operation complete, break from sub-interaction loop
                break;
            } else if((choice == 5) || (choice == 4 && strstr(server_response, "FACULTY") != NULL) || (choice == 5 && strstr(server_response, "STUDENT") != NULL)) {
                // Exit chosen in any menu
                receive_response(socket_fd, NULL);
                outside = 1;
                break;
            }
        }
        if(outside == 1){
            break;
        }
    }
    
    // Close the connection
    close(socket_fd);
    printf("\nDisconnected from server.\n");
    
    return 0;
}