//---------------------------------------------------------------------------
#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <stdio.h>
#include <error.h>
#include <strings.h>
#include <arpa/inet.h>
#include <string>
#include <algorithm>
#include <unistd.h>
#include <string.h>
#include <map>
#include <ifaddrs.h>
#include <netinet/in.h> 
#include <string.h> 
#include <arpa/inet.h>
//---------------------------------------------------------------------------
const short     PORT    = 8888;
const char*     PORT_S  = "8888";
const char*     SVR_IP  = "104.224.134.43";
const char*     IP      = "127.0.0.1";
const short     SVR_PORT= 9981;
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
void GetLocalAddress(std::string* ip)
{
    struct ifaddrs * ifAddrStruct=NULL;
    void * tmpAddrPtr=NULL;

    getifaddrs(&ifAddrStruct);

    while (ifAddrStruct!=NULL) {
        if (ifAddrStruct->ifa_addr->sa_family==AF_INET) { // check it is IP4
            // is a valid IP4 Address
            tmpAddrPtr=&((struct sockaddr_in *)ifAddrStruct->ifa_addr)->sin_addr;
            char addressBuffer[INET_ADDRSTRLEN];
            *ip = inet_ntop(AF_INET, tmpAddrPtr, addressBuffer, INET_ADDRSTRLEN);
            if("127.0.0.1" != *ip)
                break;

        } else if (ifAddrStruct->ifa_addr->sa_family==AF_INET6) { // check it is IP6
            // is a valid IP6 Address
            tmpAddrPtr=&((struct sockaddr_in *)ifAddrStruct->ifa_addr)->sin_addr;
            char addressBuffer[INET6_ADDRSTRLEN];
            inet_ntop(AF_INET6, tmpAddrPtr, addressBuffer, INET6_ADDRSTRLEN);
        } 
        ifAddrStruct=ifAddrStruct->ifa_next;
    }

    return;
}
//---------------------------------------------------------------------------
std::string Register(const std::string& name, const std::string& lan_ip, const std::string& lan_port)
{
    std::string ret_val = "register|" + name + "|" + lan_ip + "|" + lan_port;
    return ret_val;
}
//---------------------------------------------------------------------------
bool Decode(const std::string& buffer, std::string* lan_ip, std::string* lan_port, std::string* wan_ip, std::string* wan_port)
{
    //buffer format "lan_ip|lan_port|wan_ip|wan_port"

    size_t pos1 = buffer.find("|", 0);
    if(std::string::npos == pos1)
        return false;
    *lan_ip = buffer.substr(0, pos1);


    size_t pos2 = buffer.find("|", ++pos1);
    if(std::string::npos == pos2)
        return false;
    *lan_port = buffer.substr(pos1, pos2-pos1);

    size_t pos3 = buffer.find("|", ++pos2);
    if(std::string::npos == pos2)
        return false;
    *wan_ip = buffer.substr(pos2, pos3-pos2);

    *wan_port = buffer.substr(++pos3);
    if(0 == wan_port->size())
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
        close(fd);
        return 1;
    }

    //bind address
    struct sockaddr_in local_addr;
    bzero(&local_addr, sizeof(struct sockaddr_in));
    local_addr.sin_family       = AF_INET;
    local_addr.sin_port         = htons(PORT);
    //inet_pton(AF_INET, IP, &local_addr.sin_addr);
    local_addr.sin_addr.s_addr  = INADDR_ANY;

    //reuse port
    int opt = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (const void*)&opt, sizeof(opt));

    if(-1 == bind(fd, (struct sockaddr*)&local_addr, sizeof(struct sockaddr)))
    {
        perror("bind:");
        close(fd);
        return 1;
    }

    //set ttl
    int cur_ttl = 0;
    socklen_t len_ttl = sizeof(int);
    getsockopt(fd, IPPROTO_IP, IP_TTL, (char*)&cur_ttl, &len_ttl);
    printf("cur ttl:%d\n", cur_ttl);
    cur_ttl = 100;
    setsockopt(fd, IPPROTO_IP, IP_TTL, (char*)&cur_ttl, sizeof(cur_ttl));
    getsockopt(fd, IPPROTO_IP, IP_TTL, (char*)&cur_ttl, &len_ttl);
    printf("cur ttl:%d\n", cur_ttl);

    //send msg
    std::string local_ip;
    std::string local_port = PORT_S;
    GetLocalAddress(&local_ip);
    printf("local ip:%s local port:%s\n", local_ip.c_str(), local_port.c_str());
    std::string msg = Register("pc_client1", local_ip, local_port);

    struct sockaddr_in svr_addr;
    bzero(&svr_addr, sizeof(struct sockaddr_in));
    svr_addr.sin_family       = AF_INET;
    svr_addr.sin_port         = htons(SVR_PORT);
    inet_pton(AF_INET, SVR_IP, &svr_addr.sin_addr); 

    socklen_t           len = sizeof(struct sockaddr_in);
    char                buf_ip[8];
    char                buf_port[INET_ADDRSTRLEN];
    socklen_t           addr_len = sizeof(struct sockaddr_in);
    char                buffer[BUF_LEN];
    struct sockaddr_in  peer_addr;
    //while(true)
    {
        //register
        ssize_t snd_len = sendto(fd, msg.data(), msg.size()+1, 0, (const sockaddr*)&svr_addr, len);
        if(snd_len < 0)
        {
            perror("recv_from:");
            close(fd);
            return 1;
        }

        //punching pc_client2 
        msg = "punching|pc_client1|pc_client2";
        snd_len = sendto(fd, msg.data(), msg.size()+1, 0, (const sockaddr*)&svr_addr, len);
        if(snd_len < 0)
        {
            perror("recv_from:");
            close(fd);
            return 1;
        }

        int rcv_len = recvfrom(fd, static_cast<void*>(buffer), BUF_LEN, 0, (struct sockaddr*)&peer_addr, &addr_len);
        if(0 >= rcv_len)
        {
            perror("recv_from:");
            close(fd);
            return 1;
        }

        sprintf(buf_port, "%d", ntohs(peer_addr.sin_port));
        inet_ntop(AF_INET, &peer_addr.sin_addr, buf_ip, INET_ADDRSTRLEN);
        printf("peer client ip:%s, port:%s\n", buf_ip, buf_port);
        printf("msg:%s\n", buffer);

        std::string lan_ip;
        std::string lan_port;
        std::string wan_ip;
        std::string wan_port;
        Decode(buffer, &lan_ip, &lan_port, &wan_ip, &wan_port);

        struct sockaddr_in addr;
        bzero(&addr, sizeof(struct sockaddr_in));
        addr.sin_family       = AF_INET;
        addr.sin_port         = htons(std::atoi(wan_port.c_str()));
        inet_pton(AF_INET, wan_ip.c_str(), &addr.sin_addr); 

        snd_len = sendto(fd, "", 1, 0, (const sockaddr*)&addr, len);
        if(snd_len < 0)
        {
            perror("recv_from:");
            close(fd);
            return 1;
        }

        for(;;)
        {
            int rcv_len = recvfrom(fd, static_cast<void*>(buffer), BUF_LEN, 0, (struct sockaddr*)&peer_addr, &addr_len);
            if(0 >= rcv_len)
            {
                perror("recv_from:");
                close(fd);
                return 1;
            }
        
            printf("recv msg:%s\n", buffer);
        }
    }

    close(fd);
    return 0;
}
