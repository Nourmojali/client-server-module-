#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <ctype.h>
#include <signal.h>
#include <sys/wait.h>

#define BUFFER_SIZE 8192

void sigquit_handler(int signo) {
    printf("\nGoodbye!\n");
    exit(0);
}

void receive_menu(int server_socket) {
    char buffer[BUFFER_SIZE];
    memset(buffer, 0, sizeof(buffer));

    int bytes_read = read(server_socket, buffer, sizeof(buffer) - 1);
    if (bytes_read <= 0) {
        perror("Failed to receive menu");
        return;
    }

    printf("Menu:\n%s", buffer);
}

char *read_specific_line(const char *filename, int line_number) {
    FILE *file = fopen(filename, "r");
    if (!file) {
        perror("Failed to open file");
        return NULL;
    }

    char *line = malloc(BUFFER_SIZE);
    if (!line) {
        perror("Memory allocation failed");
        fclose(file);
        return NULL;
    }

    int current_line = 0;

    while (fgets(line, BUFFER_SIZE, file)) {
        current_line++;
        if (current_line == line_number) {
            fclose(file);
            return line; // Return the desired line
        }
    }

    free(line);
    fclose(file);
    return NULL; // Line not found
}

void send_request_recv_response(int server_socket, const char *operation, const char *line) {
    char buffer[BUFFER_SIZE];

    snprintf(buffer, sizeof(buffer), "%s %s", operation, line);

    write(server_socket, buffer, strlen(buffer));

    memset(buffer, 0, sizeof(buffer));
    int bytes_read = read(server_socket, buffer, sizeof(buffer) - 1);
    if (bytes_read > 0) {
        buffer[bytes_read] = '\0';
        printf("OP%s(%s) = %s\n", operation, line, buffer);
    } else {
        perror("Failed to receive response");
    }
}

int main(int argc, char *argv[]) {
    if (argc != 4) {
        fprintf(stderr, "Usage: %s <server_ip> <port> <filename>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    int client_socket;
    struct sockaddr_in server_addr;
    int port = atoi(argv[2]);

    client_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (client_socket == -1) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    signal(SIGQUIT, sigquit_handler);

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    inet_pton(AF_INET, argv[1], &server_addr.sin_addr);
    if (connect(client_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
        perror("Connection failed");
        close(client_socket);
        exit(EXIT_FAILURE);
    }

    printf("Connected to server.\n");
    receive_menu(client_socket);

    char *line = NULL;
    char oper[3];
    int num_line;

    while (1) {
        printf("Enter the line number you need to operate on: ");
        while (scanf("%d", &num_line) != 1 || num_line <= 0) {
            printf("Invalid line number. Please enter a positive integer: ");
            while (getchar() != '\n'); // Clear invalid input
        }

        getchar(); // Clear newline from input buffer after scanf

        line = read_specific_line(argv[3], num_line);
        if (!line) {
            printf("Line %d not found in the file.\n", num_line);
            continue;
        }

        line[strcspn(line, "\n")] = '\0'; // Remove newline character

        while (1) {
            printf("Enter operation (1-5) for this line: ");
            if (fgets(oper, sizeof(oper), stdin)) {
                oper[strcspn(oper, "\n")] = '\0'; // Remove newline character
                if (strlen(oper) == 1 && strchr("12345", oper[0])) {
                    break; // Valid operation
                }
            }
            printf("Invalid operation. Please enter a number between 1 and 5.\n");
        }

        send_request_recv_response(client_socket, oper, line);
        free(line); // Free allocated memory
    }

    close(client_socket);
    return 0;
}
