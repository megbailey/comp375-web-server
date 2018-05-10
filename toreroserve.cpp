/**
 * ToreroServe: A Lean Web Server
 * COMP 375 (Spring 2018) Project 02
 *
 * This program should take two arguments:
 * 	1. The port number on which to bind and listen for connections
 * 	2. The directory out of which to serve files.
 *
 * Author 1: Matthew Roth mroth@sandiego.edu
 * Author 2: Megan Bailey meganbailey@sandiego.edu
 */

// standard C libraries
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cerrno>

// operating system specific libraries
#include <netinet/in.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>

// C++ standard libraries
#include <vector>
#include <thread>
#include <string>
#include <iostream>
#include <fstream>
#include <system_error>
#include "BoundedBuffer.hpp"
#include <iostream>
#include <string>
#include <regex>

#include <boost/filesystem.hpp>


#define MAX_STRING_LENGTH 100
#define BUFF_SIZE 1024

namespace fs = boost::filesystem;

using std::cout;
using std::string;
using std::vector;
using std::thread;


// This will limit how many clients can be waiting for a connection.
static const int BACKLOG = 10;

// forward declarations
int isValidRequest(char *buffer);
int createSocketAndListen(const int port_num);
int receiveData(int socked_fd, char *dest, size_t buff_size);
void acceptConnections(const int server_sock, BoundedBuffer &sock_buffer);
void handleClient(const int client_sock, string home);
void sendData(int socked_fd, const char *data, size_t data_length);
void threader(BoundedBuffer &sock_buffer, string home);
void send_http400_response(int socked_fd);
void send_http404_response(int socked_fd);
void binaryRead_andSendFile(int socked_fd, string path); 
void send_directory(int socked_fd, string path, string home);

int main(int argc, char** argv) {
	/* Make sure the user called our program correctly. */
	if (argc != 3) {
		cout << "Improper Usage! Please specify the port number and the document root\n";
		cout << "Example: < ./toreroserve 8080 WWW >\n";
		exit(1);
	}

    /* Read the port number from the first command line argument. */
    int port = std::stoi(argv[1]);
	string home_directory = (argv[2]);

	/* Create a socket and start listening for new connections on the
	 * specified port. */
	int server_sock = createSocketAndListen(port);
	int num_threads = 5;
	int id;
	BoundedBuffer sock_buffer(BACKLOG);
	for (id = 0; id < num_threads; id++) {
		std::thread t(threader, std::ref(sock_buffer), std::ref( home_directory));
		t.detach();
	}

	/* Now let's start accepting connections. */
	acceptConnections(server_sock, sock_buffer);

	//close server_sock
    close(server_sock);

	return 0;
}

/**
 * Worker function for all the threads
 *
 * @param sock_buffer passes in a reference to a sock buffer shared by all the
 * threads
 * @param home  name of home directory
 * @return void
 */
void threader(BoundedBuffer &sock_buffer, string home) {
	int client_sock;
	// sticks threads in an infine loop
	while (true) {
		// activates threads when data becomes avaliable
		client_sock = sock_buffer.getItem();
		handleClient(client_sock, home);
	}
}



/**
 * Sends message over given socket, raising an exception if there was a problem
 * sending.
 *
 * @param socket_fd The socket to send data over.
 * @param data The data to send.
 * @param data_length Number of bytes of data to send.
 * @return void
 */
void sendData(int socked_fd, const char *data, size_t data_length) {
	//tracks # of bytes sent
	unsigned int total_sent = 0; 
	//sends bytes until all have been sent
	while (total_sent < data_length) {
		int num_bytes_sent = send(socked_fd, &data[total_sent], data_length, 0);
		if (num_bytes_sent == -1) {
			std::error_code ec(errno, std::generic_category());
			throw std::system_error(ec, "send failed");
		}
		// updates conditions for loops
		else {
			total_sent +=num_bytes_sent;
			data_length-=num_bytes_sent;
		}
		
	}
}


/**
 * Receives message over given socket, raising an exception if there was an
 * error in receiving.
 *
 * @param socket_fd The socket to send data over.
 * @param dest The buffer where we will store the received data.
 * @param buff_size Number of bytes in the buffer.
 * @return The number of bytes received and written to the destination buffer.
 */
int receiveData(int socked_fd, char *dest, size_t buff_size) {
	int num_bytes_received = recv(socked_fd, dest, buff_size, 0);
	if (num_bytes_received == -1) {
		std::error_code ec(errno, std::generic_category());
		throw std::system_error(ec, "recv failed");
	}

	return num_bytes_received;
}

/**
 * Receives a request from a connected HTTP client and sends back the
 * appropriate response.
 *
 * @note After this function returns, client_sock will have been closed (i.e.
 * may not be used again).
 * @param client_sock The client's socket file descriptor.
 * @return void
 */
void handleClient(const int client_sock, string home) {
	// Receive the request from the client. You can use receiveData here.	
	char buffer[BUFF_SIZE];
	//clearing the buffer
	memset(buffer, 0, BUFF_SIZE);
	//recieving the GET request
	receiveData(client_sock, buffer, BUFF_SIZE);	
	//parsing out path
	char path[50];char trash[50];char http[50];
	memset(path, 0, 50);
	sscanf(buffer,"%s" "%s" "%s", trash, path, http);
	//setting different path and home strings for
	//later file traversal
	string woop = home;
	string cpppath(path);
	string dirpath(path);
	woop += cpppath;
	cpppath = woop;
	//creating a boost path
	boost::filesystem::path p(cpppath);
	
	// Generate appropriate response and send response to client
	// Checking if the request is valid
	if (!isValidRequest(buffer)) {
		send_http400_response(client_sock);
	}
	//Checking if the file exists
	else if (!fs::exists(p)) {
		send_http404_response(client_sock);
	}
	//Checking if the file is a directory
	else if (fs::is_directory(p)) {
		send_directory(client_sock, dirpath, home);	
	}
	//sending the file if none of the above
	else if (!fs::is_directory(p) && fs::exists(p)) {
		binaryRead_andSendFile(client_sock, cpppath);
	}

	// Close connection with client.
	close(client_sock);
}

/*
 *Function to check if the GET request was valid
 */
int isValidRequest(char *buffer) {
	//Regex to tell fi the function is valid
	std::regex http_request_regex("GET (( )*|(	)*)*/(.*)* (( )*|(	)*)*HTTP/[0-9]\\.[0-9]\r\n", std::regex_constants::extended);
		
	//trimming the end of the buffer to match the regex
	for(int i = 0; i < (int)strlen(buffer); i++){
		if(buffer[i] == '\n'){
			buffer[i+1] = '\0';
		}
	}	
	//returning the result of the regex match
	int ret = std::regex_match(buffer, http_request_regex);
	return ret;
}

/**
 * Opens a file and reads it in binary. Then sends the file.
 *
 * @param socked_fd client sock
 * @param path path to send to
 * @return void
 *
 **/
void binaryRead_andSendFile(int socked_fd, string path) {	
	
	//binary read in start
	std::streampos file_size;
	char *memblock = NULL; 	
	
	//opens file
	std::ifstream file(path, std::ios::in|std::ios::binary|std::ios::ate);
	
	if (file.is_open()) {
		file_size = file.tellg();
		memblock = new char [file_size];
		file.seekg (0, std::ios::beg);
		file.read(memblock, file_size);
	}
	//error checking
	else {
		cout << "\nFile could not be opened.\n";
		send_http400_response(socked_fd);
	}
	string size_string = std::to_string(file_size);
	// binary read in stop and close file
	file.close();

	// start appending HTMl

	//get content length from bindary read in
	string content_length = "Content-Length: ";	
	content_length += std::to_string(file_size);	
	content_length += "\r\n";

	//get extention and append
	std::size_t index_of_ext = path.find(".");
	string ext = path.substr(index_of_ext+1);

	//checking for all the different file types in WWW
	string content_type;
	if (ext == "jpg") {	
		content_type = "Content-Type: image/";
	}
	else if (ext == "png") {
		content_type = "Content-Type: image/";
	}
	else if (ext == "gif") {
		content_type = "Content-Type: image/";
	}
	else if (ext == "html") {	
		content_type = "Content-Type: text/";
	}
	else if (ext == "txt") {
		content_type = "Content-Type: text/";
	}
	else if (ext == "pdf") {	
		content_type = "Content-Type: application/";
	}
	content_type += ext;
	content_type += "\r\n\r\n";
	
	//making HTTP header
	string resp = "HTTP/1.1 200 OK\r\n";
	resp += content_length;
	resp += content_type;
	
	//sends data in two step process
	sendData(socked_fd, resp.c_str(), resp.length());	
	sendData(socked_fd, memblock, (size_t) file_size);

	// valgrind don't hate me
	delete[] memblock;
}

/*
 *Function to send a dynamic html page of links if the file is a dictionary
 *
 * @param socked_fd client sock number 
 * @param path path to send data to
 * @param home home directory for reference
 * @return void
 */
void send_directory(int socked_fd, string path, string home){
	//bool to tell if we have broken out of the for loop, only happens if
	//an index.html is present
	int nobroke = 1;	
	//creating the html string
	string resp ="<!DOCTYPE html>\n";
	resp+="<body>\n";
	resp+="<h1> Files found in ";
	//adding our path
	resp+=path;
	resp+="</h1>\n";
	resp+="<ul>\n";
	//creating a complete path
	string boostpath = home+path;
	//using boost to iterate through the directory
	for(fs::directory_iterator entry(boostpath); entry !=fs::directory_iterator() && nobroke; ++entry){
		//getting the filename of all files
		std::string filename = entry->path().filename().string();
		//checking if the file is the index.html
		if(filename == "index.html"){
			//customizing the path to send the index to the
			//TODO
			//binaryRead_andSendFile
			/*
			string ret = path;
			ret.erase(0);
			ret+=home;
			ret+="/";
			ret+=filename;
			*/
			string rot = boostpath;
			rot+=filename;
			binaryRead_andSendFile(socked_fd, rot);	
			//exiting the for loop
			nobroke = 0;
			break;
		}
		else{
			//creating list elements out of the files in the directory
			resp+="<li>";
			resp+="<a href=\"";
			resp += filename;
			resp+="/";
			resp+="\">";
			resp+=filename;
			resp+="</a>";
			resp += "</li>\n";
		}

	}
	//finishing the html page
	resp+="</ul>\n";
	resp+="</body>\n";
	resp+="</html>\n";
	//converting the html page
	char *ret = (char*)resp.c_str();
	if(nobroke){	
		//if we didn't encounter the index.html, we send
		string header = "HTTP/1.1 200 OK\r\n";
		header += "Content-Type: text/html\r\n\r\n";
		sendData(socked_fd, header.c_str(), header.length());
		sendData(socked_fd, ret, strlen(ret));
	}
}


/**
 * Creates a new socket and starts listening on that socket for new
 * connections.
 *
 * @param port_num The port number on which to listen for connections.
 * @returns int The socket file descriptor
 */
int createSocketAndListen(const int port_num) {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("Creating socket failed");
        exit(1);
    }

    /* 
	 * A server socket is bound to a port, which it will listen on for incoming
     * connections.  By default, when a bound socket is closed, the OS waits a
     * couple of minutes before allowing the port to be re-used.  This is
     * inconvenient when you're developing an application, since it means that
     * you have to wait a minute or two after you run to try things again, so
     * we can disable the wait time by setting a socket option called
     * SO_REUSEADDR, which tells the OS that we want to be able to immediately
     * re-bind to that same port. See the socket(7) man page ("man 7 socket")
     * and setsockopt(2) pages for more details about socket options.
	 */
    int reuse_true = 1;

	int retval; // for checking return values

    retval = setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &reuse_true,
                        sizeof(reuse_true));

    if (retval < 0) {
        perror("Setting socket option failed");
        exit(1);
    }

    /*
	 * Create an address structure.  This is very similar to what we saw on the
     * client side, only this time, we're not telling the OS where to connect,
     * we're telling it to bind to a particular address and port to receive
     * incoming connections.  Like the client side, we must use htons() to put
     * the port number in network byte order.  When specifying the IP address,
     * we use a special constant, INADDR_ANY, which tells the OS to bind to all
     * of the system's addresses.  If your machine has multiple network
     * interfaces, and you only wanted to accept connections from one of them,
     * you could supply the address of the interface you wanted to use here.
	 */
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port_num);
    addr.sin_addr.s_addr = INADDR_ANY;

    /* 
	 * As its name implies, this system call asks the OS to bind the socket to
     * address and port specified above.
	 */
    retval = bind(sock, (struct sockaddr*)&addr, sizeof(addr));
    if (retval < 0) {
        perror("Error binding to port");
        exit(1);
    }

    /* 
	 * Now that we've bound to an address and port, we tell the OS that we're
     * ready to start listening for client connections. This effectively
	 * activates the server socket. BACKLOG (a global constant defined above)
	 * tells the OS how much space to reserve for incoming connections that have
	 * not yet been accepted.
	 */
    retval = listen(sock, BACKLOG);
    if (retval < 0) {
        perror("Error listening for connections");
        exit(1);
    }

	return sock;
}

/**
 * Sit around forever accepting new connections from client.
 *
 * @param server_sock The socket used by the server.
 * @param sock_buffer a sock buffer shared among the threads
 * @return void
 */
void acceptConnections(const int server_sock, BoundedBuffer &sock_buffer) {
    while (true) {
        // Declare a socket for the client connection.
        int sock;

        /* 
		 * Another address structure.  This time, the system will automatically
         * fill it in, when we accept a connection, to tell us where the
         * connection came from.
		 */
        struct sockaddr_in remote_addr;
        unsigned int socklen = sizeof(remote_addr); 

        /* 
		 * Accept the first waiting connection from the server socket and
         * populate the address information.  The result (sock) is a socket
         * descriptor for the conversation with the newly connected client.  If
         * there are no pending connections in the back log, this function will
         * block indefinitely while waiting for a client connection to be made.
         */
        sock = accept(server_sock, (struct sockaddr*) &remote_addr, &socklen);
        if (sock < 0) {
            perror("Error accepting connection");
            exit(1);
        }

        /* 
		 * At this point, you have a connected socket (named sock) that you can
         * use to send() and recv(). The handleClient function should handle all
		 * of the sending and receiving to/from the client.
		 *
		 * You shouldn't call handleClient directly here. Instead it
		 * should be called from a separate thread. You'll just need to put sock
		 * in a shared buffer and notify the threads (via a condition variable)
		 * that there is a new item on this buffer.
		 */
       	sock_buffer.putItem(sock);
	    
		/* 
		 * Tell the OS to clean up the resources associated with that client
         * connection, now that we're done with it.
		 */
    }
}

/*
 *Generic 404 response that gets fed to sendData
 *
 * @param socked_fd sock to send the error to
 * @return void
 */
void send_http404_response(int socked_fd){
	//content header
	string resp = "HTTP/1.1 404 Not Found\r\n";
	resp+="Content-Type: text/html\r\n\r\n";
	resp+="<!DOCTYPE html>\n";
	resp+="<html>\n";
	resp+="<head>\n";
	//error name
	resp+="\t<title>404 Not Found</title>\n";
	resp+="</head>\n";
	resp+="<body>\n";
	resp+="\t<h1>File Not Found</h1>\n";
	resp+="\t\t<p>Could not find the requested file</p>\n";
	resp+="</body>\n";
	resp+="</html>\n";
	char *ret = (char*)resp.c_str();
	//send data
	sendData(socked_fd, ret, strlen(ret)); 
}

/*
 *Generic 400 response that gets fed to sendData
 * 
 * @param socked_fd 
 * @return void
 */
void send_http400_response(int socked_fd){
	//content header
	string resp = "HTTP/1.1 400 Bad Request\r\n";
	resp+="Content-Type: text/html\r\n\r\n";
	resp+="<!DOCTYPE html>\n";
	resp+="<html>\n";
	resp+="<head>\n";
	//error name
	resp+="\t<title>400 Bad Request</title>\n";
	resp+="</head>\n";
	resp+="<body>\n";
	resp+="\t<h1>The browser sent a false request.</h1>\n";
	resp+="\t\t<p>Please try again.</p>\n";
	resp+="</body>\n";
	resp+="</html>\n";
	char *ret = (char*)resp.c_str();
	//send data
	sendData(socked_fd, ret, strlen(ret));
}

