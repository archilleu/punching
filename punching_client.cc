//---------------------------------------------------------------------------
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <stdio.h>
#include <error.h>
#include <strings.h>
#include <arpa/inet.h>
#include <string>
#include <algorithm>
#include <string.h>
#include <map>
//---------------------------------------------------------------------------
const short     PORT    = 8888;
const char*     IP      = "127.0.0.1";
const size_t    BUF_LEN = 1500;
//---------------------------------------------------------------------------
struct ClientInfo
{
    std::string lan_ip;
    std::string lan_port;
    std::string wan_ip;
    std::string wan_port;
};
//---------------------------------------------------------------------------
void GetLocalAddress(int sockfd, std::string* ip, std::string* port)
{
    struct sockaddr_in local_address;
    socklen_t len = sizeof(local_address);
    if(0 > ::getsockname(sockfd, reinterpret_cast<sockaddr*>(&local_address), &len))
        return;

    char buf_port[8];
    char buf_ip[INET_ADDRSTRLEN];
    sprintf(buf_port, "%u", ntohs(local_address.sin_port));
    inet_ntop(AF_INET, &local_address, buf_ip, INET_ADDRSTRLEN);
    *ip     = buf_ip;
    *port   = buf_port;

    return;
}
//---------------------------------------------------------------------------
std::string Encode(const std::string& name, const std::string& lan_ip, const std::string& lan_port)
{
    std::string ret_val = name + "|" + lan_ip + "|" + lan_port;
    return ret_val;
}
//---------------------------------------------------------------------------
bool Decode(const std::string& buffer, std::string* name, std::string* lan_ip, std::string* lan_port, std::string* sn)
{
    //buffer format "name|ip|port|sn"

    //name
    size_t pos1 = buffer.find("|", 0);
    if(std::string::npos == pos1)
        return false;
    *name = buffer.substr(0, pos1);

    //ip
    size_t pos2 = buffer.find("|", ++pos1);
    if(std::string::npos == pos2)
        return false;
    *lan_ip = buffer.substr(pos1, pos2-pos1);

    //port
    size_t pos3 = buffer.find("|", ++pos2);
    if(std::string::npos == pos2)
        return false;
    *lan_port = buffer.substr(pos2, pos3-pos2);

    //sn
    *sn = buffer.substr(++pos3);
    if(0 == sn->size())
        return false;

    return true;
}
//---------------------------------------------------------------------------
int main(int , char** )
{
    int fd = socket(AF_INET, SOCK_DGRAM, 0);
    if(-1 == fd)
    {
        perror("create socket:");
        return 1;
    }

    //send msg
    std::string local_ip;
    std::string local_port;
    GetLocalAddress(fd, &local_ip, &local_port);
    printf("local ip:%s local port:%s\n", local_ip.c_str(), local_port.c_str());
    std::string msg = Encode("pc_client1", local_ip, local_port);

    //bind address
    struct sockaddr_in local_addr;
    bzero(&local_addr, sizeof(struct sockaddr_in));
    local_addr.sin_family       = AF_INET;
    local_addr.sin_port         = htons(9981);
    inet_pton(AF_INET, IP, &local_addr.sin_addr);

    //reuse port
    int opt = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (const void*)&opt, sizeof(opt));

    if(-1 == bind(fd, (struct sockaddr*)&local_addr, sizeof(struct sockaddr*)))
    {
        perror("bind:");
        return 1;
    }

    char                buf_ip[8];
    char                buf_port[INET_ADDRSTRLEN];
    socklen_t           addr_len;
    char                buffer[BUF_LEN];
    struct sockaddr_in  peer_addr;
    //while(true)
    {
        int snd_len = sendto(fd, msg.data(), msg.size()+1, 0);


        int rcv_len = recvfrom(fd, static_cast<void*>(buffer), 0, BUF_LEN, (struct sockaddr*)&peer_addr, &addr_len);
        if(0 >= rcv_len)
        {
            perror("recv_from:");
            return 1;
        }

        sprintf(buf_port, "%u", ntohs(peer_addr.sin_port));
        inet_ntop(AF_INET, &peer_addr, buf_ip, INET_ADDRSTRLEN);
        printf("peer client ip:%s, port:%s\n", buf_ip, buf_port);

        AddClient(buffer, buf_ip, buf_port);
    }


    return 0;
}
