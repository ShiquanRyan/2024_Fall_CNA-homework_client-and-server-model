#include <iostream>
#include <string>
#include <thread>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <regex>
#define BUFFER_SIZE 1024

struct sockaddr_in address;
void P2PreceiveAndSendMessage(int sock_cli, int sock, int client_PORT){
    sockaddr_in client_addr, peers_addr;
    socklen_t addrlen = sizeof(peers_addr);
    char p2p_recv_buffer[BUFFER_SIZE] = {0};
    // set the server's port
    client_addr.sin_family = AF_INET;
    client_addr.sin_port = htons(client_PORT);
    client_addr.sin_addr.s_addr = INADDR_ANY;

    // bind the client's socket and port
    int bind_confirm = bind(sock_cli, (sockaddr*)&client_addr, sizeof(client_addr));
    if (bind_confirm < 0){
        std::cout << "binding failed" << std::endl;
    }

    listen(sock_cli, 5);
    while (true){
        int new_sock = accept(sock_cli, (sockaddr*)&peers_addr, &addrlen);
        int recv_confirm =  recv(new_sock, p2p_recv_buffer, BUFFER_SIZE, 0);
        if (recv_confirm < 0){
            std::cout << "p2p recv failed" << std::endl;
        }
        p2p_recv_buffer[recv_confirm] = '\0';
        std::cout << "p2p_recv_buffer is " << p2p_recv_buffer << std::endl;
        int send_confirm = send(sock, p2p_recv_buffer, recv_confirm, 0);
        if (send_confirm < 0){
            std::cout << "p2p send failed" << std::endl;
        }
        std::string temp_mes = "List";
        send(sock, temp_mes.c_str(), temp_mes.size(), 0);
        
    }
}
bool match_client2client(const std::string& mes){
    std::regex pattern(R"(\w+#\d+#\w+)");
    return std::regex_match(mes, pattern); 
}
bool match_login(const std::string& mes){
    std::regex pattern(R"(\w+#\d+)");
    return std::regex_match(mes, pattern);
}
// argv[1] == ip, argv[2] == port num
int main(int argc, char *argv[]){
    int sock = 0, sock_cli = 0, user_port = 0;
    std::string server_ip = argv[1];
    int server_PORT = atoi(argv[2]);
    struct sockaddr_in serv_addr;
    std::string message;
    char buffer[BUFFER_SIZE] = {0};

    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        std::cerr << "Socket creation error" << std::endl;
        return 1;
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(server_PORT);

    if (inet_pton(AF_INET, server_ip.c_str(), &serv_addr.sin_addr) <= 0) {
        std::cerr << "Invalid address or address not supported" << std::endl;
        return 1;
    }

    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        std::cerr << "Connection failed" << std::endl;
        return 1;
    }
    while(std::cin >> message){
        if (match_client2client(message)){
            // send List
            std::string temp_mes = "List";
            send(sock, temp_mes.c_str(), temp_mes.size(), 0);

            // Receive a response from the server
            int bytes_read = recv(sock, buffer, BUFFER_SIZE, 0);
            std::string str_buffer(buffer);

            // find receiver name in message
            int mark_pos = message.rfind('#');
            std::string recv_name;
            for (int i = mark_pos + 1; i < message.size(); i++){
                recv_name += message[i];
            }

            //find ip and port in List
            int name_pos = str_buffer.find(recv_name);
            int ip_port_pos = str_buffer.find('#', name_pos) + 1;
            int peers_port_int = 0;
            std::string peers_port, peers_ip;
            for (ip_port_pos; ip_port_pos < str_buffer.size(); ip_port_pos++){
                if (str_buffer[ip_port_pos] == '#'){
                    break;
                }
                peers_ip += str_buffer[ip_port_pos];
            }
            ip_port_pos += 1;
            for (ip_port_pos; ip_port_pos < str_buffer.size(); ip_port_pos++){
                if (str_buffer[ip_port_pos] == '\n'){
                    break;
                }
                peers_port += str_buffer[ip_port_pos];
                
            }
            try {
                std::cout << "Converted number: " << peers_port << std::endl;
                peers_port_int = std::stoi(peers_port);  // Attempt to convert string to int
            } catch (const std::invalid_argument& e) {
                std::cerr << "Invalid argument: could not convert to int" << std::endl;
            } catch (const std::out_of_range& e) {
                std::cerr << "Out of range: number is too large or too small" << std::endl;
            }

            // deal with connect
            int p2p_send_sock = 0;
            p2p_send_sock = socket(AF_INET, SOCK_STREAM, 0);
            sockaddr_in p2pConnect;
            p2pConnect.sin_family = AF_INET;
            p2pConnect.sin_port = htons(peers_port_int);

            std::cout<< "peers_port_int is " << peers_port_int << std::endl;
            p2pConnect.sin_addr.s_addr = inet_addr(peers_ip.c_str());
            int err = connect(p2p_send_sock, (sockaddr*) &p2pConnect, sizeof(p2pConnect));
            if (err == -1){
                std::cout << "p2p connect error" << std::endl;
            }

            // send data
            int p2p_send_confirm = send(p2p_send_sock, message.c_str(), message.size(), 0);
            if (p2p_send_confirm < 0){
                std::cout << "send error" << std::endl;
            }
            //close socket
            close(p2p_send_sock);
        }else{
            if (match_login(message)){
                int pos = message.find('#');
                std:: string temp_user_port;
                for (int i = pos + 1; i < message.size(); i++){
                    temp_user_port += message[i];
                }
                user_port = std::stoi(temp_user_port);
                
                // create a socket
                sock_cli = socket(AF_INET, SOCK_STREAM, 0);
                std::thread thread(P2PreceiveAndSendMessage, sock_cli, sock, user_port);
                if (thread.joinable()){
                    std::cout << "Thread is running.\n";
                }
                thread.detach();
            }
            
            // Send a message to the server
            send(sock, message.c_str(), message.size(), 0);
            std::cout << "Message sent to server" << std::endl;

            // Receive a response from the server
            int bytes_read = recv(sock, buffer, BUFFER_SIZE, 0);
            if (bytes_read > 0) {
                std::cout << "Received from server: " << buffer << std::endl;
            }
            if (strcmp(buffer, "Exit\n") == 0) {
                std::cout << "buffer for exit is " << buffer << std::endl;
                break;
            }
        }
        memset(buffer, 0, BUFFER_SIZE);
    }

    // Close the socket
    close(sock);

    return 0;
}
