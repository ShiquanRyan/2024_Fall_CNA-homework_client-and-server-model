#include <iostream>
#include <string>
#include <thread>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <regex>
#include <vector>
// ****** create client class ******
class client{
    public:
        client(std::string);
        std::string name;
        int deposit;
        int socket_num;
        bool is_connected;
        std::string port;
};
client::client(std::string name): name(name), deposit(1000), socket_num(-1), is_connected(false) {}
std::string parseInput(const std::string& input, std::smatch& match) {
    // Define regex patterns for each format
    std::regex registerPattern(R"(REGISTER#(\w+))");
    std::regex portPattern(R"((\w+)#(\d+))");
    std::regex listPattern(R"(List)");
    std::regex exitPattern(R"(Exit)");
    std::regex p2pPattern(R"((\w+)#(\d+)#(\w+))");

    if (std::regex_match(input, match, registerPattern)) {
        std::cout << "Format: REGISTER\n";
        std::cout << "User Account: " << match[1] << "\n";
        return "REGIS";
    } else if (std::regex_match(input, match, portPattern)) {
        std::cout << "Format: User Account with Port\n";
        std::cout << "User Account: " << match[1] << "\n";
        std::cout << "Port: " << match[2] << "\n";
        return "LOGIN";
    } else if (std::regex_match(input, listPattern)) {
        std::cout << "Format: List Command\n";
        return "LIST";
    } else if (std::regex_match(input, exitPattern)) {
        std::cout << "Format: Exit Command\n";
        return "EXIT";
    } else if (std::regex_match(input, match, p2pPattern)) {
        std::cout << "Format: Peer-to-Peer Transfer\n";
        std::cout << "sender name: " << match[1] << "\n";
        std::cout << "money: " << match[2] << "\n";
        std::cout << "recver name: " << match[3] << "\n";
        return "P2P";
    } else {
        std::cout << "Unknown format\n";
        return nullptr;
    }
}

void tunnel(std::vector<client>& client_list, int client_socket_num){
// ***** set up a tunnel for client *****
    // client info
    sockaddr_in client_addr;
    socklen_t addr_len = sizeof(client_addr);
    int client_port = 0;
    char tmp_client_ip[INET_ADDRSTRLEN];
    std::cout << client_socket_num  << std::endl;
    if (getpeername(client_socket_num, (struct sockaddr*)&client_addr, &addr_len) == 0){
        client_port = ntohs(client_addr.sin_port);
        inet_ntop(AF_INET, &client_addr.sin_addr, tmp_client_ip, INET_ADDRSTRLEN);
    }else{
        perror("getpeername error");
    }
    std::string client_ip = tmp_client_ip;
    int ThisClientIsMe = 0;

    // set up buffer
    char* message = new char [1024];
    std::smatch match_keyword;
    char* send_buffer;

// ***** handle message from client *****
    while (recv(client_socket_num, message, 1024, 0) > 0){
        std::cout << "Received from " << message << std::endl;
        std::string service_type = parseInput(std::string(message), match_keyword);
        std::cout << match_keyword[0] << std::endl;
        if (service_type == "REGIS"){
            bool overlapped_name = false;
            std::string send_buffer_OK = "100 OK\n";
            std::string send_buffer_FAIL = "210 FAIL\n";

            for (int i = 0; i < client_list.size(); i++){
                if (client_list[i].name == match_keyword[1]){
                    overlapped_name = true;
                    int send_index = send(client_socket_num, send_buffer_FAIL.c_str(), sizeof(send_buffer), 0);
                    if (send_index < 0){
                        std::cerr << "transfer REGIS error" << std::endl;
                    }
                    break;
                }
                else continue;
            }
            if (!overlapped_name){
                int send_index = send(client_socket_num, send_buffer_OK.c_str(), sizeof(send_buffer), 0);
                if (send_index < 0){
                    std::cerr << "transfer REGIS error" << std::endl;
                }
                client_list.emplace_back(client(match_keyword[1]));
            }
        }else if(service_type == "LOGIN"){
            // setting variables
            bool not_register = true;
            std::string client_tmp;
            int connect_count = 0;
            std::string send_buffer_OK;

            // match client name
            for (int i = 0; i < client_list.size(); i++){
                if (!client_list[i].is_connected){
                    if (client_list[i].name == match_keyword[1]){

                        // revise client list info
                        not_register = false;
                        client_list[i].is_connected = true;
                        client_list[i].port = match_keyword[2];
                        client_list[i].socket_num = client_socket_num;
                        connect_count++;
                        ThisClientIsMe = i;

                        // prepare send buffer
                        send_buffer_OK = std::to_string(client_list[i].deposit) + "\n" + "public key" + "\n" +
                                                                    std::to_string(connect_count) + "\n" + 
                                                                    client_list[i].name + '#' + client_ip + '#' + client_list[i].port + "\n";                                      
                    }
                }else{
                    // collect connected client info
                    connect_count++;
                    client_tmp += (client_list[i].name + '#' + client_ip + '#' +client_list[i].port + "\n");
                }
            }

            // sending message
            std::string send_buffer;
            if (!not_register){
                send_buffer = send_buffer_OK + client_tmp;
            }else{
                send_buffer = "220 AUTH_FAIL\n";
            }
            int send_index = send(client_socket_num, send_buffer.c_str(), send_buffer.size(), 0);
            if (send_index < 0){
                std::cerr << "transfer LOGIN error" << std::endl;
            }
        }else if(service_type == "LIST"){
            // setting LIST variables
            std::string client_tmp;
            int connect_count = 0;

            // go through every clients
            for (int i = 0; i < client_list.size(); i++){
                std::cout << client_list[i].name << '#' << client_ip << '#' << client_list[i].port << " " << client_list[i].is_connected << "\n";
                if (client_list[i].is_connected){
                    // collect connected client info
                    connect_count++;
                    client_tmp += (client_list[i].name + '#' + client_ip + '#' + client_list[i].port + "\n");
                }
            }

            // set up sending message
            std::string send_buffer_OK = std::to_string(client_list[ThisClientIsMe].deposit) + "\n" + "public key" + "\n" +
                                                        std::to_string(connect_count) + "\n";
            std::string send_buffer_list = send_buffer_OK + client_tmp;

            // send message
            std::cout << "Sending LIST: " << send_buffer_list << std::endl;
            int send_index = send(client_socket_num, send_buffer_list.c_str(), send_buffer_list.size(), 0);
            if (send_index < 0){
                std::cerr << "transfer LIST error" << std::endl;
            }
        }else if(service_type == "P2P"){
            // setting P2P variables
            std::string send_buffer_p2p;
            int sender_socket_num = 0;
            // find receiver
            bool receiver_found = false;
            if (client_list[ThisClientIsMe].name == match_keyword[3]){
                for (int i = 0; i < client_list.size(); i++){
                    if (client_list[i].name == match_keyword[1]){
                        receiver_found = true;
                        client_list[ThisClientIsMe].deposit += std::stoi(match_keyword[2]);
                        client_list[i].deposit -= std::stoi(match_keyword[2]);
                        sender_socket_num = client_list[i].socket_num;
                        break;
                    }
                }
            }
            if (!receiver_found){
                send_buffer_p2p = "transfer failed!\n";
            }else{
                send_buffer_p2p = "transfer OK!\n";
            }
            int send_index = send(sender_socket_num, send_buffer_p2p.c_str(), send_buffer_p2p.size(), 0);
            if (send_index < 0){
                std::cerr << "transfer P2P error" << std::endl;
            }
        }else if(service_type == "EXIT"){
            std::string send_buffer_exit = "Exit\n";
            client_list[ThisClientIsMe].is_connected = false;
            std::cout << "send_buffer_exit: " << send_buffer_exit << std::endl;
            int send_index = send(client_list[ThisClientIsMe].socket_num, send_buffer_exit.c_str(), send_buffer_exit.size(),0);
            break;
        }
        memset(message, 0, 1024);
    }
}
// argv[1] == ip, argv[2] == port num
int main(int argc, char* argv[]){
    if (argc != 3){
        return -1;
    }
    char* server_ip = argv[1];
    int server_port = std::stoi((std::string(argv[2])));
    int server_socket_num = 0;
    sockaddr_in server_socket, client_socket;
    socklen_t addrsize = sizeof(client_socket);
    std::vector<client> client_list;

    // ***** create socket ***** 
    server_socket_num = socket(AF_INET , SOCK_STREAM , 0);
    if (server_socket_num == -1){
        printf("Fail to create a socket.");
    }

    // ***** bind *****
    server_socket.sin_family = AF_INET;
    server_socket.sin_port = htons(server_port);
    if (inet_pton(AF_INET, server_ip, &server_socket.sin_addr) <= 0) {
        std::cerr << "Invalid IP address" << std::endl;
        return -1;
    }    
    if (bind(server_socket_num, (sockaddr*) &server_socket, sizeof(server_socket))){
        std::cerr << "server bind error" << std::endl;
    }

    // ***** listen *****
    if (listen(server_socket_num, 10)){
        std::cerr << "server listen error" << std::endl;
    }

    // ***** accept *****
    while (true){
        int client_socket_num = accept(server_socket_num, (sockaddr*) &client_socket, &addrsize);
        if (client_socket_num < 0){
            continue;
        } 
        std::thread client_thread(tunnel, std::ref(client_list), client_socket_num);
        client_thread.detach();
    }
    close(server_socket_num);
    return 0;    
}