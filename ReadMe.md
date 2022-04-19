# Simple C++ Web Server
A simple multi-threaded web server, Toreroserve, written in C++. It is capable of sending directories, webpages, images, files, and 400 & 404 responses. The root of the web server is /WWW. 

## Steps to run
1. Install boost - on Macos ```brew install boost```
2. ```make```
3. ```./toreroserve 8080 WWW```
4. Connect at localhost:8080 and browse filesystem
