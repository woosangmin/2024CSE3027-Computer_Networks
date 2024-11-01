#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/tcp.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <sys/sendfile.h>
#include <signal.h>

#define BUFFER_SIZE 1024

// Define structure 'HttpRequest' to get method and path inside of a http request
typedef struct {
    char method[16];
    char path[256];
    char content_type[256];
} HttpRequest;

// Global variables
char buffer[BUFFER_SIZE];
HttpRequest request;

void print_request(int client_socket) {
    ssize_t bytes_received;
    // Receive HTTP request from the client
    bytes_received = read(client_socket, buffer, BUFFER_SIZE);
    if (bytes_received < 0) {
        perror("Error receiving data from client");
        close(client_socket);
        return;
    } 
    // Null-terminate the received data to use it as a string
    buffer[bytes_received] = '\0';

    // Print the received HTTP request
    printf("Received HTTP request:\n%s\n", buffer);
}

void parse_http_request(const char *buffer, HttpRequest *request, int client_socket) {
    // Find the first space to determine the end of the method
    char *method_end = strchr(buffer, ' ');
    if (method_end == NULL) {
        perror("Invalid HTTP request\n");
        close(client_socket);
        return;
    }

    // Extract the method
    int method_length = method_end - buffer;
    strncpy(request->method, buffer, method_length);
    request->method[method_length] = '\0';

    // Find the next space after the method to determine the end of the path
    char *path_start = method_end + 1;
    char *path_end = strchr(path_start, ' ');
    if (path_end == NULL) {
        perror("Invalid HTTP request\n");
        close(client_socket);
        return;
    }

    // Extract the path
    int path_length = path_end - path_start;
    strncpy(request->path, path_start, path_length);
    request->path[path_length] = '\0';

    // If the http request contains "POST" method, then extract the Content-Type field
    if (strcmp(request->method, "POST") == 0) {
        const char *content_type_start = strstr(buffer, "Content-Type: ");
        if (content_type_start != NULL) {
            content_type_start += strlen("Content-Type: ");
            const char *content_type_end = strstr(content_type_start, "\r\n");
            if (content_type_end != NULL) {
                size_t content_type_length = content_type_end - content_type_start;
                strncpy(request->content_type, content_type_start, content_type_length);
                request->content_type[content_type_length] = '\0';
            }
        }
    }
}

void send_response(int client_socket, char *content_type, char *file_path) {
    FILE *s_file;
    s_file = fopen(file_path, "r");

    // If there is some problems with reading files, close the socket
    if (s_file == NULL) {
        perror("Error opening file");
        close(client_socket);
        return;
    } else {
        // Only proceed when the files are properly read
        // Get the file descriptor of the file
        int fd = fileno(s_file);
        if (fd == -1) {
            perror("Error getting file descriptor");
            fclose(s_file);
            close(client_socket);
            return;
        }

        // Get the size of the file
        struct stat file_stat;
        if (fstat(fd, &file_stat) == -1) {
            perror("Error getting file information");
            fclose(s_file);
            close(client_socket);
            return;
        }
        off_t file_size = file_stat.st_size;
        
        // Prepare the HTTP response headers
        char response_headers[1024];
        if (strcmp(file_path, "404.html") == 0) {
            sprintf(response_headers, "HTTP/1.1 404 Not Found\r\nContent-Type: %s\r\nContent-Length: %ld\r\n\r\n", content_type, file_size);
        } else {
            sprintf(response_headers, "HTTP/1.1 200 OK\r\nContent-Type: %s\r\nContent-Length: %ld\r\n\r\n", content_type, file_size);
        }

        // Send the HTTP response headers
        if (write(client_socket, response_headers, strlen(response_headers)) != strlen(response_headers)) {
            perror("Error sending response headers to client");
            fclose(s_file);
            close(client_socket);
            return;
        }

        // Send the file contents to the client using sendfile
        off_t offset = 0;
        ssize_t bytes_sent = sendfile(client_socket, fd, &offset, file_size);
        if (bytes_sent == -1) {
            perror("Error sending file content to client");
            fclose(s_file);
            close(client_socket);
            return;
        }
        // Close file and socket
        bzero(&response_headers, sizeof(response_headers));
        bzero(&request.path, sizeof(request.path));
        bzero(&request.method, sizeof(request.method));
        bzero(&request.content_type, sizeof(request.content_type)); 
        fclose(s_file);
    }
    return;
}   


void not_found(int client_socket) {
    char *file_path = "404.html";
    char *content_type = "text/html";
    send_response(client_socket, content_type, file_path);
    return;
}

// If the method is "POST" and the content-type is not in the list below, reply with 404 Not Found Error page
void contentType(char *f_path, int client_socket) {
    char *c_type = request.content_type;

    if (strcmp(f_path, "/") == 0 || strcmp(f_path, "/welcome.html") == 0 || strcmp(f_path, "/index.html") == 0 || strcmp(f_path, "/404.html") == 0 || strcmp(f_path, "/santorini.html") == 0) {
        if (strcmp(c_type, "text/html") != 0) {
            not_found(client_socket);
            return;     
        }
    } else if (strcmp(f_path, "/images/Homer.gif") == 0) {
        if (strcmp(c_type, "image/gif") != 0) {
            not_found(client_socket);
            return;     
        }
    } else if (strcmp(f_path, "/images/Santorini.jpeg") == 0) {
        if (strcmp(c_type, "image/jpeg") != 0) {
            not_found(client_socket);
            return;     
        }
    } else if (strcmp(f_path, "/favicon.ico") == 0 || strcmp(f_path, "/images/favicon.ico") == 0) {
        if (strcmp(c_type, "image/x-icon") != 0) {
            not_found(client_socket);
            return;  
        }  
    } else if (strcmp(f_path, "/others/SantoriniTravelGuide.pdf") == 0) {
        if (strcmp(c_type, "application/pdf") != 0) {
            not_found(client_socket);
            return;  
        } 
    } else if (strcmp(f_path, "/others/wave.mp3") == 0) {
        if (strcmp(c_type, "audio/mpeg") != 0) {
            not_found(client_socket);
            return;  
        } 
    }
    return; 
}


void prepare_response(int client_socket) {
    char *f_path = request.path;
    char *method = request.method;
    char *file_path;

    //Send out HTTP response
    //Determine whether the content-type and the method is a valid one
    if (strcmp(method, "POST") == 0) {
        contentType(f_path, client_socket);
    } else if (strcmp(method, "GET") == 0) {
        fprintf(stderr, "The GET method. The browser will automatically request the proper content-types.\n\n");
    } else if (method == NULL) {
        perror("Invalid HTTP request\n");
        close(client_socket);
        return;
    }
    //Open HTML, .ico, image File or etc
    if (strcmp(f_path, "/") == 0 || strcmp(f_path, "/welcome.html") == 0) {
        file_path = "welcome.html";
    } else if (strcmp(f_path, "/index.html") == 0) {
        file_path = "index.html";
    } else if (strcmp(f_path, "/santorini.html") == 0){
        file_path = "santorini.html";
    } else if (strcmp(f_path, "/favicon.ico") == 0 || strcmp(f_path, "/images/favicon.ico") == 0) {
        file_path = "images/favicon.ico";  
    } else if (strcmp(f_path, "/images/Homer.gif") == 0) {
        file_path = "images/Homer.gif"; 
    } else if (strcmp(f_path, "/images/Santorini.jpeg") == 0) {
        file_path = "images/Santorini.jpeg"; 
    } else if (strcmp(f_path, "/others/SantoriniTravelGuide.pdf") == 0) {
        file_path = "others/SantoriniTravelGuide.pdf"; 
    } else if (strcmp(f_path, "/others/wave.mp3") == 0) {
        file_path = "others/wave.mp3"; 
    } else {
        not_found(client_socket);
        return;
    }

        // Respond with "/welcome.html" or "/index.html", Prepare the HTTP response headers
        if (strcmp(f_path, "/") == 0 || strcmp(f_path, "/welcome.html") == 0 || strcmp(f_path, "/index.html") == 0 || (strcmp(f_path, "/santorini.html") == 0 )) {
            char *content_type = "text/html";
            send_response(client_socket, content_type, file_path);

        // Respond with "/favicon.ico", Prepare the HTTP response headers
        } else if (strcmp(f_path, "/favicon.ico") == 0 || strcmp(f_path, "/images/favicon.ico") == 0) {
            char *content_type = "image/x-icon";
            send_response(client_socket, content_type, file_path);

        // Respond with "/images/Homer.gif" , Prepare the HTTP response headers
        } else if (strcmp(f_path, "/images/Homer.gif") == 0) {
            char *content_type = "image/gif";
            send_response(client_socket, content_type, file_path);

        // Respond with "/images/Santorini.jpeg" , Prepare the HTTP response headers
        } else if (strcmp(f_path, "/images/Santorini.jpeg") == 0) {
            char *content_type = "image/jpeg";
            send_response(client_socket, content_type, file_path);

        // Respond with "/others/SantoriniTravelGuide.pdf" , Prepare the HTTP response headers
        } else if (strcmp(f_path, "/others/SantoriniTravelGuide.pdf") == 0) {
            char *content_type = "application/pdf";
            send_response(client_socket, content_type, file_path);
  
        // Respond with "/others/Summer-four-seasons.mp3" , Prepare the HTTP response headers          
        } else if (strcmp(f_path, "/others/wave.mp3") == 0) {
            char *content_type = "audio/mpeg";
            send_response(client_socket, content_type, file_path);
        }
    }

// Free up the port for a rough exit
void cleanExit() {exit(0);}

int main(int argc, char* argv[]) {
    int server_socket, client_socket, PORT;
    struct sockaddr_in server_addr, client_addr;
    socklen_t addr_size = sizeof(struct sockaddr_in);

    // If there is no input for the port number, the program will exit
    if (argc < 2) {
        fprintf(stderr,"ERROR, no port number provided\n");
        exit(1);
    }

    // Get the port number of server and convert it's data type to 'int'
    if ((PORT = atoi(argv[1])) == 0) {
        fprintf(stderr, "Please insert the port number\n");
        exit(1);       
    }
    // Terminate Program if the inserted port number is 6789, 8080 or in well-known port numbers.
    if (PORT < 1024 || PORT == 8080 || PORT == 6789) {
        printf("The port number you chose is 6789, 8080 or in well-known port numbers. Please Choose another one such as 48992.\n");
        exit(1);
    }

    // Flush the buffer before use
    bzero(&buffer, sizeof(buffer));

    // Create socket for the server
    if ((server_socket = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    // Prepare the sockaddr_in structure
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(PORT);

    // Allow reusing the same port number immediately after shutting down the webserver
    int enable = 1;
    if (setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(enable)) < 0) {
        perror("setsockopt(SO_REUSEADDR) failed");
        exit(EXIT_FAILURE);
    }

    // Set TCP_NODELAY option
    if (setsockopt(server_socket, IPPROTO_TCP, TCP_NODELAY, &enable, sizeof(enable)) < 0) {
        perror("setsockopt TCP_NODELAY failed");
        exit(EXIT_FAILURE);
    }

    // Enable keepalive
    if (setsockopt(server_socket, SOL_SOCKET, SO_KEEPALIVE, &enable, sizeof(enable)) < 0) {
        perror("setsockopt SO_KEEPALIVE failed");
        exit(EXIT_FAILURE);
    }

    // Set the linger option
    struct linger ling;
    ling.l_onoff = 1; // Enable linger option
    ling.l_linger = 10; // Set linger time to 30 seconds

    if (setsockopt(server_socket, SOL_SOCKET, SO_LINGER, &ling, sizeof(ling)) < 0) {
        perror("setsockpot SO_LINGER failed");
        exit(EXIT_FAILURE);
    }

    // Bind server's ip and port number to the socket
    if (bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Bind failed");
        exit(EXIT_FAILURE);
    }

    // Listen
    if (listen(server_socket, 50) == -1) {
        perror("Listen failed");
        exit(EXIT_FAILURE);
    }

    // Indicate the running state of server
    printf("Server listening on port %d\n", PORT);

    // Register signal handlers
    signal(SIGTERM, cleanExit);
    signal(SIGINT, cleanExit);

    while (1) {
        // Accept connection from an incoming client
        if ((client_socket = accept(server_socket, (struct sockaddr *)&client_addr, &addr_size)) < 0) {
            perror("Accept failed");
            exit(EXIT_FAILURE);
        }

        // Handle client request
        print_request(client_socket);

        // Parse the HTTP request
        parse_http_request(buffer, &request, client_socket);

        // Send the Response
        prepare_response(client_socket);

        // Flush the buffer before use
        bzero(&buffer, sizeof(buffer));

        // Close the socket
        close(client_socket);
    }
    return 0;
}