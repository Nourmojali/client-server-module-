//Nour Al-deen mojali 144204
//
//
//
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <ctype.h>
#include <signal.h>
#include <sys/wait.h>
#include <time.h>

#define BUFFER_SIZE 8192
int active_clients = 0;
// Handle terminated child processes
void sigchld_handler(int signo) {
    pid_t pid;
    while ((pid = waitpid(-1, NULL, WNOHANG)) > 0) {
        printf("Child process %d terminated\n", pid);
        active_clients--;
    }
}

// Send the menu to the client
void send_menu(int client_socket) {
    const char *menu[] = {
        "--------------------------------------------------",
        "1. Compute the count of words in the string",
        "2. Compute the count of vowel letters in the string",
        "3. Compute the count of characters that are not letter nor digit in the string",
        "4. Find the longest word in the string",
        "5. Find the repeated words in the string",
        "--------------------------------------------------"
    };

    char buffer[BUFFER_SIZE];
    memset(buffer, 0, sizeof(buffer));
    int offset = 0;

    for (int i = 0; i < 7; i++) {
        offset += snprintf(buffer + offset, BUFFER_SIZE - offset, "%s\n", menu[i]);
        if (offset >= BUFFER_SIZE) {
            fprintf(stderr, "Buffer overflow detected!\n");
            return;
        }
    }

    // Send the menu
    if (write(client_socket, buffer, offset) == -1) {
        perror("Failed to send menu");
    }
}

// Log access details
void log_access(const char *client_ip, int client_port, const char *operation, const char *status) {
    FILE *log_file = fopen("access.log", "a");
    if (!log_file) {
        perror("Failed to open log file");
        return;
    }

    // Get the current local time
    time_t now = time(NULL);
    char time_str[64];
    struct tm *local_time = localtime(&now);
    strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", local_time);

    // Write the log entry
    fprintf(log_file, "[%s] Client %s:%d Operation: %s Status: %s\n",
            time_str, client_ip, client_port, operation, status);

    fclose(log_file);
}

// Perform the requested operation
void process_request(int client_socket, struct sockaddr_in client_addr, char *request) {
    char operation = request[0];
    char *input = request + 2; // Skip operation character and space
    const char *status = "Success";
    char response[BUFFER_SIZE];
    memset(response, 0, sizeof(response));

    if (operation == '1') { // Count words
        int word_count = 0;
        char *token = strtok(input, " ");
        while (token != NULL) {
            word_count++;
            token = strtok(NULL, " ");
        }
        snprintf(response, sizeof(response), "%d", word_count);
    } else if (operation == '2') { // Count vowels
        int vowel_count = 0;
        for (int i = 0; input[i] != '\0'; i++) {
            if (strchr("aeiouAEIOU", input[i])) {
                vowel_count++;
            }
        }
        snprintf(response, sizeof(response), "%d", vowel_count);
    } else if (operation == '3') { // Count non-alphanumeric characters
        int non_alnum_count = 0;
        for (int i = 0; input[i] != '\0'; i++) {
            if (!isalnum(input[i]) && !isspace(input[i])) {
                non_alnum_count++;
            }
        }
        snprintf(response, sizeof(response), "%d", non_alnum_count);
    } else if (operation == '4') { // Find longest word
        char longest_word[BUFFER_SIZE] = "";
        char *token = strtok(input, " ");
        while (token != NULL) {
            if (strlen(token) > strlen(longest_word)) {
                strcpy(longest_word, token);
            }
            token = strtok(NULL, " ");
        }
        snprintf(response, sizeof(response), "%s", longest_word);
    } else if (operation == '5') { // Find repeated words
        snprintf(response, sizeof(response), "unsupported operation");
        status = "Failure";
    } else {
        snprintf(response, sizeof(response), "Invalid operation.");
        status = "Failure";
    }

    // Log the request
    char client_ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, sizeof(client_ip));
    log_access(client_ip, ntohs(client_addr.sin_port), request, status);

    // Send the response back to the client
    write(client_socket, response, strlen(response));
}

// Main server logic
int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <port>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    signal(SIGCHLD, sigchld_handler);

    int server_socket, client_socket;
    struct sockaddr_in server_addr, client_addr;
    socklen_t addr_len = sizeof(client_addr);
    int port = atoi(argv[1]);

    // Create the server socket
    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket == -1) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
        perror("Bind failed");
        close(server_socket);
        exit(EXIT_FAILURE);
    }

    if (listen(server_socket, 4) == -1) {
        perror("Listen failed");
        close(server_socket);
        exit(EXIT_FAILURE);
    }

    printf("Server listening on port %d\n", port);
    while (1) {
        // Wait if the maximum number of clients is reached
        while (active_clients >= 4) {
              sleep(2);
        }
        client_socket = accept(server_socket, (struct sockaddr *)&client_addr, &addr_len);
        active_clients++;
        printf("Client connected: %s:%d\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
        send_menu(client_socket);

        pid_t pid = fork();
        if (pid == 0) {
            close(server_socket);
            char buffer[BUFFER_SIZE];
            memset(buffer, 0, sizeof(buffer));

            while (1) {
                int bytes_read = read(client_socket, buffer, sizeof(buffer) - 1);
                if (bytes_read <= 0) {
                    break; // Client disconnected
                }
                buffer[bytes_read] = '\0';
                printf("Received request: %s\n", buffer);
                process_request(client_socket, client_addr, buffer);
            }

            close(client_socket);
            exit(0);
        } else if (pid > 0) {
            close(client_socket); // Parent closes the client socket
        } else {
            perror("Fork failed");
        }
    }

    close(server_socket);
    return 0;
}
