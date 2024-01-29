// SADIQ ISAMAIL                        | 110122554 | Section 2
// Chinnarannagari, Manjunatha Reddy    | 110095281 | Section 2
#include <stdio.h>      // Include standard input and output library for IO operations
#include <stdlib.h>     // Include standard library for functions involving memory allocation, process control, conversions, etc.
#include <string.h>     // Include string library for various string operations
#include <sys/socket.h> // Include socket library for socket operations
#include <arpa/inet.h>  // Include library for Internet operations (e.g., inet_addr)
#include <unistd.h>     // Include POSIX operating system API for access to POSIX OS API
#include <pwd.h>        // Include password structure for getting user information
#include <sys/types.h>  // Include definitions of data types used in system calls
#include <sys/stat.h>   // Include definitions for working with file statistics
#include <time.h>       // Include time handling functions
#include <errno.h>      // Include standard error numbers

// Define constants for maximum sizes of various buffers and commands
#define MAX_CMD_SIZE 1024         // Maximum size of a command input by the user
#define MAX_RESPONSE_SIZE 4096    // Maximum size of response buffer from the server
#define BUFFER_SIZE 8192          // General purpose buffer size for data transfer

int portToConnect;

 // Function to check if the syntax of the command entered by the user is correct.
int verify_command_syntax(const char *cmd) {
    int year, month, day; // Variables to store parsed date components.
    // Check if the command starts with 'getfn' and has additional characters (i.e., filename).
    if (strncmp(cmd, "getfn ", 6) == 0 && strlen(cmd) > 6) {
        return 1; // Return 1 for valid 'getfn' command with filename.
    }
    // Check if the command starts with 'getfz' and parse two integer sizes.
    if (strncmp(cmd, "getfz ", 6) == 0) {
        int size1, size2;
        if (sscanf(cmd + 6, "%d %d", &size1, &size2) == 2) {
            // Check if sizes are valid (size1 <= size2, size1 >= 0, size2 >= 0).
            if (size1 >= 0 && size2 >= 0 && size1 <= size2) {
                return 1; // Return 1 for valid 'getfz' command.
            }
        }
        // Print usage instructions if 'getfz' command syntax is invalid.
        printf("Invalid command syntax. Usage:\n");
        printf("getfz <size1> <size2> {size1 <= size2, size1 >= 0, size2 >= 0. Size is in bytes}\n");
        printf("e.g: \"getfz 1 10\"\n");
        return 0;
    }
    // Check if the command starts with 'getft' and parse up to four file extensions.
    if (strncmp(cmd, "getft ", 6) == 0) {
        char extensions[4][10]; // Temporary array to hold up to four extensions.
        int num_extensions = sscanf(cmd + 6, "%s %s %s %s", 
                                    extensions[0], extensions[1], 
                                    extensions[2], extensions[3]);
        // Validate the number of extensions and each extension format.
        if (num_extensions >= 1 && num_extensions <= 3) {
            for (int i = 0; i < num_extensions; i++) {
                // Check if an extension starts with a dot, which is invalid.
                if (extensions[i][0] == '.') {
                    printf("Invalid extension format. Extensions should not start with a dot.\n");
                    return 0;
                }
            }
            return 1; // Return 1 for valid 'getft' command.
        } else {
            // Print usage instructions if 'getft' command syntax is invalid.
            printf("Invalid command syntax. Usage:\n");
            printf("getft <extension1> <extension2> <extension3>\n");
            printf("Extensions should be between 1 and 3 and should not start with a dot.\n");
            printf("e.g.: \"getft c pdf txt\"\n");
            return 0;
        }
    }
    // Check if the command starts with 'getfdb' or 'getfda' and parse the date.
    if (strncmp(cmd, "getfdb ", 7) == 0 || strncmp(cmd, "getfda ", 7) == 0) {
        // Parse the date in YYYY-MM-DD format.
        if (sscanf(cmd + 7, "%4d-%2d-%2d", &year, &month, &day) == 3) {
            // Validate the date components (year, month, day).
            if (year >= 1000 && year <= 9999 && month >= 1 && month <= 12 && day >= 1 && day <= 31) {
                return 1; // Return 1 for valid date format.
            }
        }
        // Print usage instructions if 'getfdb' or 'getfda' command syntax is invalid.
        printf("Invalid command syntax. Usage:\n");
        printf("%s <date> {Format: YYYY-MM-DD}\n", strncmp(cmd, "getfdb", 6) == 0 ? "getfdb" : "getfda");
        printf("e.g: \"%s 2023-01-01\"\n", strncmp(cmd, "getfdb", 6) == 0 ? "getfdb" : "getfda");
        return 0;
    }
    // Check if the command is 'quitc'.
    if (strcmp(cmd, "quitc") == 0) {
        return 1; // Return 1 for valid 'quitc' command.
    }
    // Print general message for any other invalid command.
    printf("Invalid command syntax. Please enter one of the following:\n");
    printf("getfn <filename.ext>\n");
    printf("getfz <size1> <size2>\n");
    printf("getft <extension list>\n");
    printf("getfdb <date>\n");
    printf("getfda <date>\n");
    printf("quitc\n");
    return 0; // Return 0 for invalid command.
}

// Function to receive a file from the server and save it locally.
void receive_file(int client_socket, const char *file_name) {
    printf("Response from client:\n");
    printf("Starting to receive file \"temp.tar.gz\"\n");
    char buffer[BUFFER_SIZE];  // Buffer to hold received data
    // Open or create a file for writing the received data
    FILE *file = fopen(file_name, "wb");
    if (file == NULL) {
        perror("Error: Unable to open file for writing");
        return;
    }
    int total_bytes_received = 0;  // Track the total number of bytes received
    const char* end_of_transmission = "End of file transfer.";
    int end_of_transmission_len = strlen(end_of_transmission);
    // Loop to receive data until the end-of-transmission message or error
    while (1) {
        int bytes_received = recv(client_socket, buffer, BUFFER_SIZE, 0);  // Receive data from server
        if (bytes_received < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                printf("Timeout occurred. No data received from server for 30 seconds.\n");
                break; // Exit the loop on timeout
            } else {
                perror("recv failed");
                break;
            }
        } else if (bytes_received == 0) {
            printf("Server has closed the connection.\n");
            break;  // Server closed the connection
        }
        // Check for the end-of-transmission message within the received buffer
        char *end_msg_ptr = strstr(buffer, end_of_transmission);
        if (end_msg_ptr != NULL) {
            // Write data up to the end-of-transmission message
            fwrite(buffer, 1, end_msg_ptr - buffer, file);
            break;
        } else {
            // Write the received data to the file
            fwrite(buffer, 1, bytes_received, file);
        }
        total_bytes_received += bytes_received;  // Update total bytes received
    }
    fclose(file);  // Close the file
    printf("File reception completed successfully. Total bytes received: %d\n", total_bytes_received);
    printf("File received and saved as '%s'\n", file_name);  // Confirm file reception
}

// Function to check if a directory exists, and create it if it doesn't.
void dir_check(const char *projectPath) {
    struct stat st = {0};  // Structure to store the status of the file
    // Check if the directory exists
    if (stat(projectPath, &st) == -1) {
        // Directory does not exist, create it
        if (mkdir(projectPath, 0700) != 0) {  // Create directory with read, write, execute permissions for the user
            perror("Failed to create f23project directory");
            exit(EXIT_FAILURE);  // Exit if directory creation fails
        }
    }
}

// Function to generate a filename for the tarball based on the command and current timestamp.
void filename(char *tarFilename, int maxLen, const char *command) {
    // Get the current time
    time_t now = time(NULL);
    struct tm *timeinfo = localtime(&now); // Convert time to local time structure
    // Format the filename with the command, date, and time
    snprintf(tarFilename, maxLen, "temp(%s_%d-%02d-%02d.%02d.%02d.%02d).tar.gz",
             command, // Command used (e.g., getfz, getft)
             timeinfo->tm_year + 1900, // Year
             timeinfo->tm_mon + 1,     // Month (tm_mon is 0-11, so add 1)
             timeinfo->tm_mday,        // Day
             timeinfo->tm_hour,        // Hour
             timeinfo->tm_min,         // Minute
             timeinfo->tm_sec);        // Second
}

// Function to try connecting to the main server and, if declined, the mirror server.
int tryConnect(const char *server_ip, int server_port, int mirror_port) {
    int serverSock, mirrorSock;
    struct sockaddr_in server_addr;
    // Create a socket for the IPv4 network and using TCP/IP
    serverSock = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSock < 0) {
        perror("Socket creation failed");
        return -1;
    }
    // Initialize server address structure
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;  // Set family to IPv4
    server_addr.sin_addr.s_addr = inet_addr(server_ip); // Set server IP address
    // Attempt to connect to the main server
    server_addr.sin_port = htons(server_port);  // Convert port number to network byte order
    printf("Trying to connect to the main server (Port: %d)...\n", server_port);
    if (connect(serverSock, (struct sockaddr *)&server_addr, sizeof(server_addr)) == 0) {
        // Check for a decline message from the server
        char ack[9];
        recv(serverSock, ack, 8, 0);
        ack[8] = '\0';  // Ensure the message is null-terminated
        if (strcmp(ack, "DECLINED") == 0) {
            // If the main server declined, try connecting to the mirror server
            printf("Main server declined the connection. Trying the mirror (Port: %d)...\n", mirror_port);
            close(serverSock); // Close the old socket before creating a new one for the mirror
            mirrorSock = socket(AF_INET, SOCK_STREAM, 0); // Create a new socket
            server_addr.sin_port = htons(mirror_port); // Update port for mirror server
            if (connect(mirrorSock, (struct sockaddr *)&server_addr, sizeof(server_addr)) == 0) {
                printf("Connected to the mirror.\n");
                portToConnect = mirror_port;
                return mirrorSock; // Return the socket descriptor for the mirror
            } else {
                printf("Failed to connect to the mirror.\n");
                close(mirrorSock);
                return -1;
            }
        } else {
            printf("Main server accepted the connection.\n");
            portToConnect = server_port;
            return serverSock; // Return the socket descriptor for the main server
        }
    } else {
        printf("Failed to connect to the main server.\n");
        close(serverSock);  // Close the socket if both attempts fail
        return -1; // Return -1 if neither server nor mirror is available
    }
}

int main(int argc, char *argv[]) {
    // Check if the correct number of command-line arguments are provided
    if (argc != 4) {
        // Print usage instructions if incorrect arguments are provided
        fprintf(stderr, "Usage: %s <server and mirror IP> <server port> <mirror port>\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    // Extract server and mirror IP address, server port, and mirror port from command-line arguments
    char *mainIP = argv[1];             // IP address for both server and mirror
    int serverPort = atoi(argv[2]);     // Server port number
    int mirrorPort = atoi(argv[3]);     // Mirror server port number
    // Buffers for storing command input and server response
    char cmd[MAX_CMD_SIZE], response[MAX_RESPONSE_SIZE];
    // Attempt to connect to either the main server or the mirror server
    // int portToConnect = tryConnect(mainIP, serverPort, mirrorPort);
    int client_socket = tryConnect(mainIP, serverPort, mirrorPort);
    if (client_socket == -1) {
        // If connection to both server and mirror fails, terminate the client
        printf("Neither main server nor mirror is available.\n");
        exit(EXIT_FAILURE);
    }
    // Get the home directory path to use for saving files
    char projectPath[1024];
    const char *homeDir = getenv("HOME");
    if (homeDir != NULL) {
        // Construct the project directory path inside the home directory
        snprintf(projectPath, sizeof(projectPath), "%s/f23project", homeDir);
    } else {
        // If home directory is not found, terminate with an error
        fprintf(stderr, "Failed to find the HOME directory.\n");
        exit(EXIT_FAILURE);
    }
    // Create the project directory if it doesn't exist
    dir_check(projectPath);
    // Set a 30-second timeout for receiving data from the server
    struct timeval tv;
    tv.tv_sec = 30;  // 30 seconds
    tv.tv_usec = 0;  // 0 microseconds
    if (setsockopt(client_socket, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof(tv)) < 0) {
        perror("Error setting socket timeout");
        close(client_socket);
        exit(EXIT_FAILURE);
    }
    // Loop to handle client interaction with the server
    while (1) {
        printf("\nClient$ ");
        fgets(cmd, MAX_CMD_SIZE, stdin); // Read command from user
        cmd[strcspn(cmd, "\n")] = 0; // Remove newline character

        // Validate the syntax of the command
        if (!verify_command_syntax(cmd)) {
            continue;  // If invalid, prompt for new input
        }
        // Send the command to the server
        send(client_socket, cmd, strlen(cmd), 0);
        // If command is to quit, exit the loop and close the client
        if (strcmp(cmd, "quitc") == 0) {
            break;
        }
        // Clear response buffer before receiving response
        memset(response, 0, sizeof(response)); // Clear the response buffer
        // Receive the response from the server
       int bytes_received = recv(client_socket, response, MAX_RESPONSE_SIZE, 0);
        if (bytes_received <= 0) {
            printf("Empty response from server\n");
            break;
        }
        response[bytes_received] = '\0'; // Null-terminate the response
        if (portToConnect == serverPort) {
            printf("Response from server:\n%s", response); // Display the server's response
        } else {
            printf("Response from mirror:\n%s", response); // Display the server's response
        }
        // If the command was a file request, handle file reception
        if (strncmp(cmd, "getfz", 5) == 0 || strncmp(cmd, "getft", 5) == 0 || strncmp(cmd, "getfdb", 6) == 0 || strncmp(cmd, "getfda", 6) == 0) {
            char command[7];
            sscanf(cmd, "%6s", command); // Extract the command type
            char tarFilename[1024];
            filename(tarFilename, sizeof(tarFilename), command); // Generate a filename based on the command
            char fullPath[1080];
            snprintf(fullPath, sizeof(fullPath), "%s/%s", projectPath, tarFilename); // Full path to save the file
            // If the response indicates a successful file creation, receive the file
            if (strstr(response, "Tarball created successfully") != NULL || strstr(response, "Partial success") != NULL) {
                receive_file(client_socket, fullPath); // Receive the file from the server
            }
        }
    }
    // Close the client socket and exit the program
    close(client_socket);
    if (portToConnect == serverPort) {
        printf("Disconnected from the server.\n\n");
    } else {
        printf("Disconnected from the mirror.\n\n");
    }
    return 0;
}
