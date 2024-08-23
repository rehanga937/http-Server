#include <iostream>
#include <string>
#include <unistd.h>
#include <sys/socket.h>
#include <netdb.h>
#include <array>
#include <thread>
#include <fstream>
#include <sys/stat.h>


std::string parseBetween(std::string s, std::string firstMarker, std::string secondMarker);
std::string parseFromThisToEnd (std::string s, std::string firstMarker);
std::string getPath(std::string requestContents); //gets path from the HTTP request
std::string getMethod(std::string requestContents); //gets method from the HTTP request
std::string formulateEchoResponse(std::string path);
std::string formulateUserAgentResponse(std::string requestContents);
std::string fetchFileContents(std::string path, std::string contentType);
std::string codeCraftersGetFile(std::string path, std::string directory); //exclusively a code crafters requirement if file is required from the "files" folder
bool storeFile(std::string path, std::string directory, std::string requestContents); //for POSTing a file
void handleClient(int client_fd, std::string directory);
void shutdownServer(bool&);
bool isValidFilePath(std::string path);
std::string getFileExtension(std::string validPath);
std::string defaultContentType(std::string fileExtension);

const std::string CRLF = "\r\n";
const std::string HTTP200 = "HTTP/1.1 200 OK" + CRLF;
const std::string HTTP201 = "HTTP/1.1 201 Created" + CRLF;
const std::string HTTP400 = "HTTP/1.1 400 Bad Request" + CRLF;
const std::string HTTP404 = "HTTP/1.1 404 Not Found" + CRLF;
const std::string HTTP414 = "HTTP/1.1 414 URI Too Long" + CRLF;
const std::string HTTP500 = "HTTP/1.1 500 Internal Server Error" + CRLF;
const std::string HTTP501 = "HTTP/1.1 501 Not Implemented" + CRLF;

/**
 * Use this link to bring up the search in the documentation: https://pubs.opengroup.org/onlinepubs/9699919799/
 * curl commands for testing: 
 *  curl -v -X GET http://localhost:4221/files/237_dooby_Coo_vanilla
 *  curl -vvv -d "hello world" localhost:4221/files/postfile.txt
*/

int main(int argc, char **argv) {

  std::string directory = (argc > 2) ? argv[2] : ""; //codecrafters test the get file functionality by passing in a directory path
  //as an argument in the terminal. eg: ./your_server.sh --directory <directory path>. Hence argv[2]. 
  // ./your_server.sh is a bash script used to compile the source code using cmake, and then run the compiled executable.
  

  /** 1. First we create a socket.
   * 
   * A socket is an endpoint of a duplex communication link.
   * We create a socket using the socket method defined in sys/socket.h
   * 
   * Method signature: int socket(int domain, int type, int protocol);
   * 
   * The socket() function shall create an unbound socket in a communications domain, 
   *    and return a file descriptor (an integer) that can be used in later function calls that operate on sockets.
   * https://pubs.opengroup.org/onlinepubs/009695399/functions/socket.html
   * 
   * symbolic constants defined in sys/socket.h
   * AF_INET - Internet domain sockets for use with IPv4 addresses.
   * SOCK_STREAM - Byte-stream socket. (TCP)
   * Specifying a protocol of 0 causes socket() to use an unspecified default protocol appropriate for the requested socket type.
   *    For example here it would select TCP as we specified SOCK_STREAM. Maybe u need to specify the protocol twice for safety reasons.
   * 
   * If we wanted IPV6 and UDP, we use AF_INET6 and SOCK_DGRAM(datagram socket)
   * 
   * Returned file descriptor will be -1 if an error occured, otherwise >= 0
   * 
  */

  int server_fd = socket(AF_INET, SOCK_STREAM, 0);
  if (server_fd < 0) {
   std::cerr << "Failed to create server socket\n";
   return 1;
  }
  

  /* From CodeCrafters: Since the tester restarts your program quite often, setting REUSE_PORT
    ensures that we don't run into 'Address already in use' errors 
  */

  /** 2. We can use the setsockopt method to change socket properties, and comply with the above requirement from CodeCrafters
   * 
   * Method signature: int setsockopt(int socket, int level, int option_name, const void *option_value, socklen_t option_len);
   *
   * The setsockopt() function shall set the option specified by the option_name argument, at the protocol level specified 
   *    by the level argument, to the value pointed to by the option_value argument for the socket associated with the file descriptor 
   *    specified by the socket argument.
   * https://pubs.opengroup.org/onlinepubs/9699919799/functions/setsockopt.html#
   * 
  */
  int reuse = 1;
  if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEPORT, &reuse, sizeof(reuse)) < 0) {
    std::cerr << "setsockopt failed\n";
    return 1;
  }
  
  /** 3. We need to specifiy the address properties we want the socket to use, and then bind it to them.
   * 
   * We use the struct defined in the netdb.h header.
   * https://pubs.opengroup.org/onlinepubs/9699919799/basedefs/netdb.h.html - definitions for network database operations
   * 
   * The sockaddr_in struct is used to store an address.
   * .sin_family = AF_INET  indicates that the address is IPV4
   * .sin_port = htons(4221)  stores the port. htons() converts port from host byte order to network byte order
   * .sin_addr.s_addr = INADDR_ANY  means sockets will bind to all network interfaces on the machine. The constant represents
   *    "Any network interface". Alternatively I've seen (but haven't tested myself) the usage of .sin_addr = { htonl(INADDR_ANY) } 
   *    (source: https://youtu.be/jLplqoB04hE?si=BhTwJ8J1Q_RgLdrz he uses C, not C++)
   * 
   * htons() and htonl() are among a family of functions used to convert 16-bit and 32-bit quantities 
   *    between network byte order and host byte order. For example htons stands for "host to network short" and htonl
   *    is "host to network long"
   * https://pubs.opengroup.org/onlinepubs/9699919799/functions/htons.html 
   * 
   * The address is stored in the sockaddr_in struct (which is casted to sockaddr struct when used in methods - see the bind method below
   *    for an example)
   * 
   * bind() will return 0 if successful
   * 
  */
 
  struct sockaddr_in server_addr;
  server_addr.sin_family = AF_INET;
  server_addr.sin_addr.s_addr = INADDR_ANY;
  server_addr.sin_port = htons(4221);
  
  if (bind(server_fd, (struct sockaddr *) &server_addr, sizeof(server_addr)) != 0) {
    std::cerr << "Failed to bind to port 4221\n";
    return 1;
  }
  
  /** 4. We indicate the socket is now ready to accept incoming connections.
   * 
   * For this we use listen(), and it will return 0 if successful.
   * The second argument is the max size of the queue of pending connections.
   * 
  */
  int connection_backlog = 5;
  if (listen(server_fd, connection_backlog) != 0) {
    std::cerr << "listen failed\n";
    return 1;
  }
  

  /** 5. Now we must prepare and allow potential client connections.
   * 
   * So we define a blank struct to store the client address properties, and also get its length cuz for some reason
   *    you can't use sizeof like we did with the bind method. 
   *    Exact reason: " argument of type "unsigned long" is incompatible with parameter of type "socklen_t *" "
   * The accept function will create a socket for the client.
   * 
  */

  struct sockaddr_in client_addr;
  int client_addr_len = sizeof(client_addr);
  
  std::cout << "Server " << std::to_string(server_fd) << " has started waiting for clients to connect...\n";
  //std::cout << "Enter 'q' to exit program \n";  //in case I enable shutdownServer()

  /** 6. We may have to handle multiple clients concurrently.
   * 
   * The accept function will create a socket for the client.
   * Refer handleClient() for remaining comments.
   * 
   * std::thread is used for concurrency, and it wraps around accept() and handleClient().
   * 
   * Had to add the following line to the CMakeLists.txt, to make my program compile on codecrafters,
   *    even tho it compiled fine on my macbook. Codecrafters gave the error: "Cmake error undefined reference to `pthread_create'"
   *    SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread")
   * 
  */

  bool serverRunning = true;
  
  //in case I enable shutdownServer():
  //if you the function you are putting in the thread takes variables by reference, wrap it in std::ref()
  /*
  std::thread s(shutdownServer, std::ref(serverRunning));
  s.detach();
  */

  while (serverRunning) {
    int client_fd = accept(server_fd, (struct sockaddr *) &client_addr, (socklen_t *) &client_addr_len);
    if (client_fd < 0) {
      std::cerr << "invalid client_fd " << client_fd << std::endl;
      continue;
    }
    std::cout << "Client " << std::to_string(client_fd) << " connected" << std::endl;
    std::thread t(handleClient, client_fd, directory);
    t.detach();
  }

  close(server_fd);
  std::cout << "Server " << std::to_string(server_fd) << " shut down!" << std::endl;
  return 0;
  
}





void handleClient(int client_fd, std::string directory) {
  /** 7. After that we want to get the HTTP request the client sent.
   * 
   * For that we need to define an array as a character buffer which will be used by the recv() function.
   * 
   * recv() - https://pubs.opengroup.org/onlinepubs/9699919799/functions/recv.html#
   * recv - receive a message from a connected socket
   * Method signature - ssize_t recv(int socket, void *buffer, size_t length, int flags);
   * The recv() function shall receive a message from a connection-mode or connectionless-mode socket. 
   *    It is normally used with connected sockets because it does not permit the application to retrieve the source 
   *    address of received data.
   *    I'm guessing connection-mode is like TCP and connectionless-mode is like UDP.
   * recv() will return the length of the message in bytes, or -1 if error.
   * 
  */

  std::string response = "";


  std::array<char, 1024> buffer = { '\0' };
  int bytes_received = recv(client_fd, buffer.data(), buffer.size(), 0);
  if (bytes_received < 0) {
    //throw std::runtime_error("Failed to get contents of HTTP request");
    close(client_fd);
    std::cerr << "Failed to get contents of HTTP request of client " << std::to_string(client_fd);
    std::cerr << ". Connection closed.\n";
    return;
  }
  if (bytes_received > buffer.size()) {
    response = HTTP414 + CRLF;
    std::cerr << "HTTP request buffer overflow for client " << std::to_string(client_fd) << std::endl;
  }
  std::string requestContents = buffer.data();
  std::cout << "Client " << client_fd << "'s request contents:\n" <<  "START\n" << requestContents << "END" << std::endl;


  /** 8. Prepare the HTTP response. 
   * 
  */

  std::string path = getPath(requestContents);
  std::string method = getMethod(requestContents);

  if (response != "") { /*response was already formulated above - probably HTTP414 - URI too long*/ }
  else if (method == "GET"){
    if (path == "") {
      response = HTTP200 + CRLF;
    } else if (path.substr(0,5) == "echo/") {
      response = formulateEchoResponse(path); //to print what the client has entered after echo/
    } else if (path.substr(0,10) == "user-agent") {
      response = formulateUserAgentResponse(requestContents); //to print the user-agent contents
    } else if (path.substr(0,6) == "files/") {
        response = codeCraftersGetFile(path, directory); /*if the user sends a URI of format file/<path>, the server
        will return the file as content-type: application/octet-stream from the directory specified as a command-line
        argument. This is a codecrafters requirement.*/
    } else if (isValidFilePath(path)) {
      response = fetchFileContents(path, defaultContentType(getFileExtension(path))); /*if path is a valid location in the server, 
      the file will be returned as content-type: application/octet-stream*/
    } else {
      response = HTTP404 + CRLF;
    }
  } else if (method == "POST"){
    if (path.substr(0,6) == "files/") {
      if (!storeFile(path, directory, requestContents)) {
        std::cerr << "Error saving file. Specified path: " << path << std::endl;
        response = HTTP500 + CRLF;
      } else {
        std::cout << "File saved. Path: " << path << std::endl;
        response = HTTP201 + CRLF;
      }
    } else {
      response = HTTP501 + CRLF; /* This is will also be the response for HEAD requests, even though the web docs at
      https://developer.mozilla.org/en-US/docs/Web/HTTP/Status#server_error_responses specify that servers must support HEAD and GET.
      But I haven't implemented the response formulation for HEAD yet. */
    }
  }
  

  /** 9. We send the HTTP response to the client
   * 
   * Message signature: ssize_t send(int socket, const void *buffer, size_t length, int flags);
   * 
   * Cuz of the format of the second argument, we use .c_str() on our string. 
   *    (Returns a pointer to an array that contains a null-terminated sequence of characters (i.e., a C-string) 
   *    representing the current value of the string object.)
   * 
   * It returns number of bytes sent, -1 if error.
   * 
  */
  
  if ( send(client_fd, response.c_str(), response.length(), 0) == -1 ) {
    std::cerr << "Error sending HTTP response to client " << std::to_string(client_fd) << std::endl;
  }

  /** 10. Close the client socket.
   * https://pubs.opengroup.org/onlinepubs/9699919799/basedefs/unistd.h.html - includes POSIX terminal stuff, including close()
  */
  
  close(client_fd);
  std::cout << "Closed client " << std::to_string(client_fd) << std::endl;
}






std::string parseBetween (std::string s, std::string firstMarker, std::string secondMarker) {
  int i = firstMarker.length();
  std::size_t pos = s.find(firstMarker);
  std::string suffix = s.substr(pos+i);
  std::size_t pos2 = suffix.find(secondMarker);
  return s.substr(pos+i,pos2);
}

std::string parseFromThisToEnd (std::string s, std::string firstMarker) {
  int i = firstMarker.length();
  std::size_t pos = s.find(firstMarker);
  return s.substr(pos+i,s.size());
}

std::string getPath (std::string requestContents) {
  return parseBetween(requestContents," /"," "); //because the HTTP Request request-line is in the format: GET /<some path> HTTP/1.0
}

std::string getMethod(std::string requestContents){
  return parseBetween(requestContents,""," ");
}

std::string formulateEchoResponse(std::string path) {
  path = "/" + path;
  std::string body = path.substr(6);
  std::string response = HTTP200 + "Content-Type: text/plain" + CRLF + "Content-Length: " + std::to_string(body.length()) + CRLF;
  response += CRLF; //end of header
  //start of body
  response += body;
  return response;
} //for echoing user URL after echo/ back to them

std::string formulateUserAgentResponse(std::string requestContents) {
  std::string body = parseBetween(requestContents,CRLF + "User-Agent: ",CRLF);
  std::string response = HTTP200 + "Content-Type: text/plain" + CRLF + "Content-Length: " + std::to_string(body.length()) + CRLF;
  response += CRLF; //end of header
  //start of body
  response += body;
  return response;
} //for echoing the user-agent content back to the user


void shutdownServer(bool& serverRunning) {
  std::string userInput = "";
  while (true) {
    std::cin >> userInput;
    if (userInput == "q") { break; } 
    else { userInput = ""; }
  }
  serverRunning = false;
}

bool isValidFilePath(std::string path) {
  //prevent clients from accessing files at the level of the server executable
  int index = path.find_first_of("/");
  if (index > path.size()) { return false; }

  std::ifstream file(path);

  if (file.good()) { return true; }
  //std::cout << "Path not good" << std::endl;
  return false;
}


std::string fetchFileContents(std::string path, std::string contentType) {

  std::ifstream file(path);
  std::string line = "";
  std::string contents = "";
  while (file.good()) {
      std::getline(file,line);
      contents += line;
      contents += "\n";
  }
  //std::cout << "Contents: " << contents << std::endl;
  contents = contents.substr(0,contents.size()-1); //to remove additional new line character at the end
  //std::cout << "Contents after removing additional new line character: " << contents << std::endl;
  std::string response = HTTP200 + "Content-Type: " + contentType + CRLF + "Content-Length: " + std::to_string(contents.length()) + CRLF;
  //Content-Type: text/plain will display the contents on the broswer
  //Content-Type: application/octet-stream will offer the file as a download
  response += CRLF; //end of header
  //start of body
  response += contents;
  file.close();
  return response;
} /*for returning file contents to clients. Content-type is application/octet-stream. This causes the file to be offered
as a download by a typical web broswer. If you want to print in the browser window, use Content-Type: text/plain*/

std::string codeCraftersGetFile(std::string path, std::string directory){
  std::string actualPath = path.substr(6,path.size());
  actualPath = directory + actualPath;
  //std::cout << "Actual Path: " << actualPath << std::endl;
  if (!isValidFilePath(actualPath)) { return HTTP404 + CRLF; }
  return fetchFileContents(actualPath, "application/octet-stream");
} /*if the user sends a URI of format file/<path>, the server
      will return the file as content-type: application/octet-stream from the directory specified as a command-line
      argument. This is a codecrafters requirement.*/


bool storeFile(std::string path, std::string directory, std::string requestContents) {
  path = path.substr(6,path.size()); //remove the leading /files/ from the path
  if (directory == "") {
    path = directory + path; //can customize here if needed with a default folder
  } else {
    path = directory + "/" + path;
  }
  
  //std::cout << "Testing. Path to store file:" << path << std::endl;
  mkdir(directory.c_str(), 0777); //https://pubs.opengroup.org/onlinepubs/009695299/functions/mkdir.html
  //0777 refers to the permissions, in this case wrx for user,group,others

  std::ofstream file(path);
  if (!file.good()) { return false; }
  requestContents = parseFromThisToEnd(requestContents,CRLF+CRLF);
  file << requestContents;
  return true;
}

std::string getFileExtension(std::string validPath) {
  std::size_t found = validPath.find_last_of("/");
  std::string file = validPath.substr(found+1);
  /* file has to be extracted first, instead of just searching for a "." to get the file extension
  as it is possible for a file to not have an extension */
  std::size_t found2 = file.find_last_of(".");
  if (found2 == -1) { return ""; }
  return file.substr(found2+1);
}

std::string defaultContentType(std::string fileExtension) {
  // https://developer.mozilla.org/en-US/docs/Web/HTTP/Headers/Content-Type
  // https://developer.mozilla.org/en-US/docs/Web/HTTP/Basics_of_HTTP/MIME_types
  // https://developer.mozilla.org/en-US/docs/Web/HTTP/Basics_of_HTTP/MIME_types/Common_types
  if (fileExtension == "bmp") {
    return "image/bmp";
  } else if (fileExtension == "css") {
    return "text/css";
  } else if (fileExtension == "csv") {
    return "text/csv";
  } else if (fileExtension == "gif") {
    return "image/gif";
  } else if (fileExtension == "htm" || fileExtension == "html") {
    return "text/html";
  } else if (fileExtension == "ico") {
    return "image/vnd.microsoft.icon";
  } else if (fileExtension == "jpg" || fileExtension == "jpeg") {
    return "image/jpeg";
  } else if (fileExtension == "js") {
    return "text/javascript";
  } else if (fileExtension == "json") {
    return "application/json";
  } else if (fileExtension == "png") {
    return "image/png";
  } else if (fileExtension == "pdf") {
    return "application/pdf";
  } else if (fileExtension == "php") {
    return "application/x-httpd-php";
  } else if (fileExtension == "svg") {
    return "image/svg+xml";
  } else if (fileExtension == "tif" || fileExtension == "tiff") {
    return "image/tiff";
  } else if (fileExtension == "txt") {
    return "text/plain";
  } else {
    return "application/octet-stream";
  }
}