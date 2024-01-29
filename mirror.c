// SADIQ ISAMAIL                        | 110122554 | Section 2
// Chinnarannagari, Manjunatha Reddy    | 110095281 | Section 2
#include <stdio.h>      // Standard input and output functions
#include <stdlib.h>     // Standard library for memory allocation, process control, etc.
#include <string.h>     // String handling functions
#include <sys/socket.h> // Socket programming functions
#include <netinet/in.h> // Structures and functions for internet domain addresses
#include <arpa/inet.h>  // Functions for manipulating numeric IP addresses
#include <unistd.h>     // Provides access to POSIX operating system API
#include <sys/types.h>  // Definitions of data types used in system calls
#include <sys/stat.h>   // Functions for getting information about files
#include <fcntl.h>      // File control options
#include <dirent.h>     // Directory entry structure
#include <time.h>       // Time and date functions
#include <pwd.h>        // Functions for password database operations
#include <signal.h>     // Signal handling functions
#include <sys/wait.h>   // Declarations for waiting for process termination
#include <sys/file.h>   // Functions for file descriptor operations

// Definition of constants used in the server program
#define MAX_CMD_SIZE 1024         // Defines the maximum size of a command received from the client
#define MAX_RESPONSE_SIZE 4096    // Defines the maximum size of the response sent back to the client
#define BUFFER_SIZE 8192          // Defines the buffer size used for reading/writing data during file transfer
#define BEFORE_DATE 0             // Constant used to indicate a file search for dates before a given date
#define AFTER_DATE 1              // Constant used to indicate a file search for dates after a given date

// Global variables to store the server port, the home directory path, and the root directory for the project
int SERVER_PORT;  // Stores the port number on which the server will listen
char home_dir[1024];  // Buffer to store the home directory path
char root_directory[1024];  // Buffer to store the root directory path for file operations
static const int serverID = 1; // Static constant to identify the server (0 for main, 1 for mirror)

// Signal handler for SIGCHLD, to clean up zombie child processes
void sigchld_handler(int signum) {
    // Continuously call waitpid in a non-blocking manner to clean up all terminated child processes
    while (waitpid((pid_t)(-1), 0, WNOHANG) > 0) {}
}

// Function to obtain and store the home directory path of the current user
void get_home_directory() {
    struct passwd *pw = getpwuid(getuid());  // Get passwd struct for the current user
    strcpy(home_dir, pw->pw_dir);  // Copy the home directory path to the global variable
}

// Function to initialize the root directory for storing files based on the provided root path
void initialize_root_directory(const char *root) {
    strcpy(root_directory, root);  // Copy the provided root path to the global variable
    char commandDirs[4][20] = {"getfz", "getft", "getfdb", "getfda"};  // Names of subdirectories for different commands
    char path[1024];  // Buffer to store the path of each subdirectory
    for (int i = 0; i < 4; i++) {
        snprintf(path, sizeof(path), "%s/%s", root, commandDirs[i]);  // Construct the path for each subdirectory
        struct stat st = {0};  // Struct to check if the directory exists
        if (stat(path, &st) == -1) {  // Check if the directory exists
            if (mkdir(path, 0700) != 0) {  // Try to create the directory if it doesn't exist
                perror("Failed to create directory");  // Print error message if directory creation fails
                exit(EXIT_FAILURE);  // Terminate the program on failure
            }
        }
    }
}

// Function to initialize or reset the connection count file.
// This function is used to maintain the number of connections handled by the server.
void initializeOrResetConnectionCountFile() {
    char countFilePath[1024];
    // Construct the path to the connection count file using the root directory path.
    snprintf(countFilePath, sizeof(countFilePath), "%s/connection_count.txt", root_directory);
    // Open (or create if it doesn't exist) the file in write mode.
    FILE *file = fopen(countFilePath, "w");
    if (!file) {
        // If opening the file fails, print an error and exit.
        perror("Failed to open the connection count file");
        exit(EXIT_FAILURE);
    }
    // Write '0' to the file, effectively resetting or initializing the connection count.
    fprintf(file, "%d", 0);
    // Close the file.
    fclose(file);
}

// Function to get the current connection count from the file.
int getConnectionCount() {
    char countFilePath[1024];
    // Construct the path to the connection count file.
    snprintf(countFilePath, sizeof(countFilePath), "%s/connection_count.txt", root_directory);
    // Open the file in read mode.
    FILE *file = fopen(countFilePath, "r");
    if (!file) {
        // If opening the file fails, print an error and return -1.
        perror("Error opening file for reading");
        return -1;
    }
    // Lock the file for shared reading.
    if (flock(fileno(file), LOCK_SH) == -1) {
        perror("Error locking file for reading");
        fclose(file);
        return -1;
    }
    int count;
    // Read the count from the file.
    if (fscanf(file, "%d", &count) != 1) {
        perror("Failed to read count");
        count = -1;
    }
    // Unlock the file.
    if (flock(fileno(file), LOCK_UN) == -1) {
        perror("Error unlocking file after reading");
    }
    // Close the file.
    fclose(file);
    return count;
}

// Function to update the connection count in the file.
void setConnectionCount(int count) {
    char countFilePath[1024];
    // Construct the path to the connection count file.
    snprintf(countFilePath, sizeof(countFilePath), "%s/connection_count.txt", root_directory);
    // Open the file in write mode.
    FILE *file = fopen(countFilePath, "w");
    if (!file) {
        perror("Error opening file for writing");
        return;
    }
    // Lock the file for exclusive writing.
    if (flock(fileno(file), LOCK_EX) == -1) {
        perror("Error locking file for writing");
        fclose(file);
        return;
    }
    // Write the updated count to the file.
    fprintf(file, "%d", count);
    fflush(file); // Flush data to disk
    // Unlock the file.
    if (flock(fileno(file), LOCK_UN) == -1) {
        perror("Error unlocking file after writing");
    }
    // Close the file.
    fclose(file);
}

// Function to recursively searche for a file with a specified name in a directory and its subdirectories.
int search_file_recursive(const char *dir_path, const char *filename, char *response) {
    // Open the directory.
    DIR *dir = opendir(dir_path);
    if (dir == NULL) {
        return -1;  // Cannot open directory
    }
    struct dirent *entry;
    // First pass: Check all files in the current directory.
    while ((entry = readdir(dir)) != NULL) {
        // Check if the entry is a regular file.
        if (entry->d_type == DT_REG) {
            // Compare entry name with the target filename.
            if (strcmp(entry->d_name, filename) == 0) {
                char full_path[1024];
                // Construct the full path of the file.
                snprintf(full_path, sizeof(full_path), "%s/%s", dir_path, entry->d_name);
                struct stat statbuf;
                // Get file statistics.
                if (stat(full_path, &statbuf) == 0) {
                    char formattedTime[80];
                    struct tm *timeinfo;
                    // Convert last modified time to local time.
                    timeinfo = localtime(&statbuf.st_mtime); 
                    // Format the time to a readable string.
                    strftime(formattedTime, sizeof(formattedTime), "%a %d %b %Y %I:%M:%S %p %Z", timeinfo);
                    // Write file details to the response buffer.
                    sprintf(response, "File: %s\nSize: %lld bytes\nDate Created: %s\nFile Permissions: %o\n", 
                            full_path, (long long)statbuf.st_size, formattedTime, statbuf.st_mode & 0777);
                    // Close directory and return success.
                    closedir(dir);
                    return 0;
                }
            }
        }
    }
    // Second pass: Recursively search in subdirectories.
    rewinddir(dir);  // Reset directory stream to the beginning.
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_type == DT_DIR) {
            // Skip '.' and '..' directories.
            if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
                continue;
            char new_path[1024];
            // Construct new path for the subdirectory.
            snprintf(new_path, sizeof(new_path), "%s/%s", dir_path, entry->d_name);
            // Recursively search in the subdirectory.
            if (search_file_recursive(new_path, filename, response) == 0) {
                closedir(dir);
                return 0;  // File found in subdirectory.
            }
        }
    }
    closedir(dir);
    return -1;  // File not found in this directory or its subdirectories.
}

// Function to gather files within a specified size range into a tarball.
int gather_files_by_size(const char *root, int size1, int size2, const char *tar_name) {
    char cmd[1024];
    char find_output[1024];
    char find_error[1024];
    // Prepare paths for output and error logs.
    snprintf(find_output, sizeof(find_output), "%s/getfz/find_output.txt", root_directory);
    snprintf(find_error, sizeof(find_error), "%s/getfz/find_error.txt", root_directory);
    // Adjust sizes to account for the 'find' command's behavior.
    int newSize1 = size1 - 1;
    int newSize2 = size2 + 1;
    // Constructing the find command to search for files within the size range.
    snprintf(cmd, sizeof(cmd), "find %s -type f -size +%dc -size -%dc -exec printf '%%s\\n' {} \\; 2>%s 1>%s", 
            root, newSize1, newSize2, find_error, find_output);
    // Execute the find command.
    system(cmd);
    struct stat statbuf;
    int result = 1; // Default to no files found
    // Check the result of the find command.
    if (stat(find_output, &statbuf) == 0 && statbuf.st_size > 0) {
        // Files found, create tarball.
        char tar_cmd[1024];
        snprintf(tar_cmd, sizeof(tar_cmd), "tar -czvf %s -T %s > /dev/null 2>&1", tar_name, find_output);
        system(tar_cmd);
        // Check if find command encountered permission errors.
        if (stat(find_error, &statbuf) == 0 && statbuf.st_size > 0) {
            result = 2; // Partial success (permission issues).
        } else {
            result = 0; // Full success.
        }
    } else {
        result = 1; // No files found.
    }
    // Clean up: Remove temporary output and error files.
    remove(find_output);
    remove(find_error);
    return result;
}

// Function to gather files with specified file extensions into a tarball.
int gather_files_by_types(const char *root, const char **extensions, int num_extensions, const char *tar_name) {
    char find_cmd[MAX_CMD_SIZE] = "";
    char find_output[1024];
    char find_error[1024];
    // Prepare paths for output and error logs.
    snprintf(find_output, sizeof(find_output), "%s/getft/find_output.txt", root_directory);
    snprintf(find_error, sizeof(find_error), "%s/getft/find_error.txt", root_directory);
    // Constructing the find command to search for files with specified extensions.
    strcat(find_cmd, "find ");
    strcat(find_cmd, root);  // Set search root directory.
    strcat(find_cmd, " -type f \\( ");  // Start of the conditional expression for file types.
    for (int i = 0; i < num_extensions; i++) {
        strcat(find_cmd, "-name '*.");  // Pattern for file extension.
        strcat(find_cmd, extensions[i]);  // Append the extension.
        strcat(find_cmd, "' ");
        if (i < num_extensions - 1) {
            strcat(find_cmd, "-o ");  // Logical OR for multiple extensions.
        }
    }
    strcat(find_cmd, " \\) -exec printf '%s\\n' {} \\; 2>");  // End of the conditional and start of the exec command.
    strcat(find_cmd, find_error);  // Redirect stderr to find_error file.
    strcat(find_cmd, " 1>");  // Redirect stdout to find_output file.
    strcat(find_cmd, find_output);
    // Execute the find command.
    system(find_cmd);
    struct stat statbuf;
    int result = 1; // Default to no files found
    // Check the result of the find command.
    if (stat(find_output, &statbuf) == 0 && statbuf.st_size > 0) {
        // Files found, create tarball.
        char tar_cmd[1024];
        snprintf(tar_cmd, sizeof(tar_cmd), "tar -czvf %s -T %s > /dev/null 2>&1", tar_name, find_output);
        system(tar_cmd);
        // Check for permission errors.
        if (stat(find_error, &statbuf) == 0 && statbuf.st_size > 0) {
            result = 2;  // Partial success (permission issues).
        } else {
            result = 0;  // Full success.
        }
    } else {
        result = 1; // No files found.
    }
    // Clean up: Remove temporary output and error files.
    remove(find_output);
    remove(find_error);
    return result;
}

// Function to gathers files created before or after a specified date into a tarball.
int gather_files_by_date(const char *root, const char *date, int mode, const char *tar_name) {
    char find_cmd[MAX_CMD_SIZE] = "";
    char find_output[1024];
    char find_error[1024];
    // Construct the path for output and error files based on the command mode.
    if (mode == BEFORE_DATE) {
        snprintf(find_output, sizeof(find_output), "%s/getfdb/find_output.txt", root_directory);
        snprintf(find_error, sizeof(find_error), "%s/getfdb/find_error.txt", root_directory);
    } else { // AFTER_DATE
        snprintf(find_output, sizeof(find_output), "%s/getfda/find_output.txt", root_directory);
        snprintf(find_error, sizeof(find_error), "%s/getfda/find_error.txt", root_directory);
    }
    // Constructing the find command to search for files based on date.
    snprintf(find_cmd, sizeof(find_cmd), "find %s -type f ", root);
    if (mode == BEFORE_DATE) {
        strcat(find_cmd, "\\( ! -newermt ");  // Search for files older than the specified date.
    } else { // AFTER_DATE
        strcat(find_cmd, "\\( -newermt ");  // Search for files newer than the specified date.
    }
    strcat(find_cmd, date);  // Append the specified date.
    strcat(find_cmd, " \\) -exec printf '%s\\n' {} \\; 2>");  // End of the conditional and start of the exec command.
    strcat(find_cmd, find_error);  // Redirect stderr to find_error file.
    strcat(find_cmd, " 1>");  // Redirect stdout to find_output file.
    strcat(find_cmd, find_output);
    // Execute the find command.
    system(find_cmd);
    int result = 1; // Default to no files found
    struct stat statbuf;
    // Check the result of the find command.
    if (stat(find_output, &statbuf) == 0 && statbuf.st_size > 0) {
        // Files found, create tarball.
        char tar_cmd[1024];
        snprintf(tar_cmd, sizeof(tar_cmd), "tar -czvf %s -T %s > /dev/null 2>&1", tar_name, find_output);
        system(tar_cmd);

        // Check for permission errors.
        if (stat(find_error, &statbuf) == 0 && statbuf.st_size > 0) {
            result = 2; // Partial success (permission issues).
        } else {
            result = 0; // Full success.
        }
    } else {
        result = 1; // No files found.
    }
    // Clean up: Remove temporary output and error files.
    remove(find_output);
    remove(find_error);
    return result;
}

// Function to send a file to a client over a socket.
void send_file(int client_socket, const char *file_name) {
    char buffer[BUFFER_SIZE];
    // Open the file in read-binary mode
    FILE *file = fopen(file_name, "rb");
    if (file == NULL) {
        // If unable to open file, send an error message to the client
        strcpy(buffer, "Error: Unable to open file.\n");
        send(client_socket, buffer, strlen(buffer), 0);
        return;
    }
    // Loop to read and send the file in chunks
    while (1) {
        // Read a chunk of the file into the buffer
        size_t bytes_read = fread(buffer, 1, sizeof(buffer), file);
        if (bytes_read > 0) {
            // Send the chunk read into the buffer to the client
            send(client_socket, buffer, bytes_read, 0);
        }
        // Check if end of file is reached or if there is an error
        if (bytes_read < sizeof(buffer)) {
            if (feof(file)) {
                // End of file is reached
                break;
            }
            if (ferror(file)) {
                // There is an error in reading the file
                perror("Error reading the file");
                break;
            }
        }
    }
    // Close the file
    fclose(file);
    // Sleep for a short duration to ensure the client has time to process received data
    sleep(3);  // Sleep for 3 seconds (not 1 as commented)
    // Send an end-of-transmission message to signify completion of file transfer
    const char* end_of_transmission = "End of file transfer.";
    send(client_socket, end_of_transmission, strlen(end_of_transmission), 0); 
}

// Function to process client requests on the server.
void pclientrequest(int client_socket) {
    char buffer[BUFFER_SIZE], response[MAX_RESPONSE_SIZE]; // Buffers for receiving and sending data
    // Continuously process incoming requests from the client
    while (1) {
        memset(buffer, 0, sizeof(buffer)); // Clear the buffer to receive a new message
        // Receive data from the client
        int bytes_received = recv(client_socket, buffer, sizeof(buffer), 0); // Receive data into buffer
        // If no bytes are received, client has closed connection or an error occurred
        if (bytes_received <= 0) {
            break; // Break out of the loop to end the function
        }

        // Handle 'getfn' command: Search for a specific file by name
        if (strncmp(buffer, "getfn ", 6) == 0) { // Check if the command is 'getfn'
            char filename[256]; // Buffer to hold the extracted filename
            sscanf(buffer + 6, "%s", filename); // Extract filename from the command (skipping the first 6 characters)
            // Search for the file recursively in the home directory
            if (search_file_recursive(home_dir, filename, response) != 0) { 
                strcpy(response, "File not found\n"); // If file not found, prepare response
            }
            // Send the response back to the client
            send(client_socket, response, strlen(response), 0);
        }

        // Handle 'getfz' command: Gather files by size and create a tarball
        if (strncmp(buffer, "getfz ", 6) == 0) {
        int size1, size2; // Variables to store size range inputs from the command
            // Extract and parse the size range from the command
            if (sscanf(buffer + 6, "%d %d", &size1, &size2) == 2) {
                char tar_name[1024]; // Buffer to store the name of the tarball file
                // Generate the tarball file name with a path in the server's filesystem
                snprintf(tar_name, sizeof(tar_name), "%s/getfz/temp.tar.gz", root_directory);
                // Call the function to gather files by size range and create a tarball
                int result = gather_files_by_size(home_dir, size1, size2, tar_name);
                // Prepare the response based on the result of the file gathering operation
                if (result == 0) {
                    // If successful, notify the client that the tarball is created and will be sent
                    strcpy(response, "Tarball created successfully. Sending file...\n");
                } else if (result == 1) {
                    // If no files are found, notify the client accordingly
                    strcpy(response, "No file found.\n");
                } else {  // result == 2
                    // If there was a partial success (e.g., due to permission issues), notify the client
                    strcpy(response, "Partial success: Some files may not be included due to access restrictions.\n");
                }
                // Send the response back to the client
                send(client_socket, response, strlen(response), 0);
                // If files were found (fully or partially), send the tarball file to the client
                if (result == 0 || result == 2) {
                    send_file(client_socket, tar_name);
                    // The tarball file could be removed here if cleanup is desired
                    //remove(tar_name);
                }
            } else {
                // If the size arguments are not properly formatted, send an error message to the client
                strcpy(response, "Error: Unable to parse size arguments.\n");
                send(client_socket, response, strlen(response), 0);
            }
        }

        // Handle 'getft' command: Gather files by file type extensions and create a tarball
        if (strncmp(buffer, "getft ", 6) == 0) {
            char extensions[3][10]; // Array to store up to three file extensions from the command
            // Extract and parse the file extensions from the command
            int num_extensions = sscanf(buffer + 6, "%s %s %s", extensions[0], extensions[1], extensions[2]);
            char tar_name[1024]; // Buffer to store the name of the tarball file
            // Check if at least one valid extension was provided
            if (num_extensions > 0) {
                // Array of pointers to the extensions, used as input for the file gathering function
                const char *ext_pointers[3] = {extensions[0], extensions[1], extensions[2]};
                // Generate the tarball file name with a path in the server's filesystem
                snprintf(tar_name, sizeof(tar_name), "%s/getft/temp.tar.gz", root_directory);
                // Call the function to gather files by extensions and create a tarball
                int result = gather_files_by_types(home_dir, ext_pointers, num_extensions, tar_name);
                // Prepare the response based on the result of the file gathering operation
                if (result == 0) {
                    // If successful, notify the client that the tarball is created and will be sent
                    strcpy(response, "Tarball created successfully. Sending file...\n");
                } else if (result == 1) {
                    // If no files are found, notify the client accordingly
                    strcpy(response, "No file found.\n");
                } else { // result == 2
                    // If there was a partial success (e.g., due to permission issues), notify the client
                    strcpy(response, "Partial success: Some files may not be included due to access restrictions.\n");
                }
                // Send the response back to the client
                send(client_socket, response, strlen(response), 0);
                // If files were found (fully or partially), send the tarball file to the client
                if (result == 0 || result == 2) {
                    send_file(client_socket, tar_name);
                    // The tarball file could be removed here if cleanup is desired
                    //remove(tar_name);
                }
            } else {
                // If the file extensions are not properly formatted or missing, send an error message to the client
                strcpy(response, "Error: Invalid file type extension list.\n");
                send(client_socket, response, strlen(response), 0);
            }
        }

        // Handle 'getfdb' command: Gather files created before a specified date
        if (strncmp(buffer, "getfdb ", 7) == 0) {
            char date[11]; // Buffer to store the date in YYYY-MM-DD format
            // Extract and parse the date from the command
            if (sscanf(buffer + 7, "%10s", date) == 1) {
                char tar_name[1024]; // Buffer to store the name of the tarball file
                // Generate the tarball file name with a path in the server's filesystem
                snprintf(tar_name, sizeof(tar_name), "%s/getfdb/temp.tar.gz", root_directory);
                // Call the function to gather files created before the specified date and create a tarball
                int result = gather_files_by_date(home_dir, date, BEFORE_DATE, tar_name);
                // Prepare the response based on the result of the file gathering operation
                if (result == 0) {
                    // If successful, notify the client that the tarball is created and will be sent
                    strcpy(response, "Tarball created successfully. Sending file...\n");
                } else if (result == 1) {
                    // If no files are found, notify the client accordingly
                    strcpy(response, "No file found.\n");
                } else { // result == 2
                    // If there was a partial success (e.g., due to permission issues), notify the client
                    strcpy(response, "Partial success: Some files may not be included due to access restrictions.\n");
                }
                // Send the response back to the client
                send(client_socket, response, strlen(response), 0);
                // If files were found (fully or partially), send the tarball file to the client
                if (result == 0 || result == 2) {
                    send_file(client_socket, tar_name);
                }
            } else {
                // If the date is not properly formatted or missing, send an error message to the client
                strcpy(response, "Error: Unable to parse date.\n");
                send(client_socket, response, strlen(response), 0);
            }
        }

        // Handle 'getfdb' command: Gather files created after a specified date
        if (strncmp(buffer, "getfda ", 7) == 0) {
            char date[11]; // Buffer to store the date in YYYY-MM-DD format
            // Extract and parse the date from the command
            if (sscanf(buffer + 7, "%10s", date) == 1) {
                char tar_name[1024]; // Buffer to store the name of the tarball file
                // Generate the tarball file name with a path in the server's filesystem
                snprintf(tar_name, sizeof(tar_name), "%s/getfda/temp.tar.gz", root_directory);
                // Call the function to gather files created after the specified date and create a tarball
                int result = gather_files_by_date(home_dir, date, AFTER_DATE, tar_name);
                // Prepare the response based on the result of the file gathering operation
                if (result == 0) {
                    // If successful, notify the client that the tarball is created and will be sent
                    strcpy(response, "Tarball created successfully. Sending file...\n");
                } else if (result == 1) {
                    // If no files are found, notify the client accordingly
                    strcpy(response, "No file found.\n");
                } else { // result == 2
                    // If there was a partial success (e.g., due to permission issues), notify the client
                    strcpy(response, "Partial success: Some files may not be included due to access restrictions.\n");
                }
                // Send the response back to the client
                send(client_socket, response, strlen(response), 0);
                // If files were found (fully or partially), send the tarball file to the client
                if (result == 0 || result == 2) {
                    send_file(client_socket, tar_name);
                }
            } else {
                // If the date is not properly formatted or missing, send an error message to the client
                strcpy(response, "Error: Unable to parse date.\n");
                send(client_socket, response, strlen(response), 0);
            }
        }

        // Handle 'quitc' command: Close socket and kill child process (send a signal to the main process)
        if (strncmp(buffer, "quitc", 5) == 0) {
            close(client_socket); // Close the client socket and exit the child process
            exit(0); // Terminate the child process
        }
        
        // After sending the response for each command
        memset(response, 0, sizeof(response)); // Clear the response buffer
    }
    // Close the client socket after processing all requests
    close(client_socket);
}

// Function to continuously listen for and handle client connections.
void serverLoop(int server_socket, int serverID) {
    while (1) {
        // Accept a new client connection
        int client_socket = accept(server_socket, NULL, NULL);
        // If the accept call fails, log the error and continue listening
        if (client_socket < 0) {
            perror("Accept failed");
            continue;
        }
        // Get the current connection count from the connection count file
        int connectionNumber = getConnectionCount();
        // If unable to get the connection count, continue after a short pause
        if (connectionNumber == -1) {
            perror("Error getting connection count");
            sleep(1);
            continue;
        }
        // Increment connection count by 1
        setConnectionCount(connectionNumber + 1);
        int connectionNumberUpdated = getConnectionCount();
        printf("Client Connection %d detected\n", connectionNumberUpdated);
        // Fork a new process for handling the client request
        if (fork() == 0) {
            // In child process: close the listening server socket
            close(server_socket);
            // Call function to process the client request
            pclientrequest(client_socket);
            // After handling the request, close the client socket and exit the child process
            close(client_socket);
            exit(0);
        }
        // In the parent process: close the client socket and continue listening for new connections
        close(client_socket);
    }
}

int main(int argc, char *argv[]) {
    // Check if the correct number of command line arguments is provided
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <port>\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    // Convert the port number from string to integer
    int SERVER_PORT = atoi(argv[1]);
    int server_socket;
    struct sockaddr_in server_address;
    // Retrieve and set up the home directory for server operations
    get_home_directory();
    initialize_root_directory(home_dir);
    // Initialize or reset the connection count file
    initializeOrResetConnectionCountFile();
    // Create the server socket
    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket < 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }
    // Bind the server socket to the specified port
    memset(&server_address, 0, sizeof(server_address));
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(SERVER_PORT);
    server_address.sin_addr.s_addr = INADDR_ANY;
    if (bind(server_socket, (struct sockaddr *)&server_address, sizeof(server_address)) < 0) {
        perror("Socket bind failed");
        exit(EXIT_FAILURE);
    }
    // Listen on the server socket for incoming connections
    if (listen(server_socket, 10) < 0) {
        perror("Socket listen failed");
        exit(EXIT_FAILURE);
    }
    // Set up a signal handler for dealing with zombie child processes
    struct sigaction sa;
    sa.sa_handler = &sigchld_handler; // Handler function defined elsewhere
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART | SA_NOCLDSTOP;
    if (sigaction(SIGCHLD, &sa, NULL) == -1) {
        perror("sigaction failed");
        exit(1);
    }
    // Notify that the server is running
    printf("Mirror is running on port %d...\n", SERVER_PORT);
    // Enter the main server loop to accept and handle connections
    serverLoop(server_socket, serverID);
    // Close the server socket before exiting
    close(server_socket);
    return 0;
}