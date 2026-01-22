#ifndef ECHO_SERVER_H
#define ECHO_SERVER_H

class EchoServer{ 

    public:
    EchoServer(int port);
    ~EchoServer();
    void start();
    void stop();

    private:

    int client_fd;
    int server_fd;
    int port;
    
    void setupSocket();
    void acceptClient();
    void handleClient();
    void cleanup();
   
};
#endif