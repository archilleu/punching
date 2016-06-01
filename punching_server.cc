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
const short     PORT    = 9981;
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
std::map<std::string, ClientInfo> g_name_info;
//---------------------------------------------------------------------------
bool Register(const std::string& buffer, std::string* name, std::string* lan_ip, std::string* lan_port)
{
    //buffer format "name|ip|port"

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
    *lan_port = buffer.substr(++pos2);
    if(0 == lan_port->size())
        return false;

    return true;
}
//---------------------------------------------------------------------------
bool AddClient(const std::string& buffer, const std::string& wan_ip, const std::string& wan_port)
{
    std::string name;
    std::string ip;
    std::string port;
    if(false == Register(buffer, &name, &ip, &port))
    {
        printf("msg format error:(name|ip|port)\n");
        return false;
    }
    
    auto& client    = g_name_info[name];
    client.lan_ip   = ip;
    client.lan_port = port;
    client.wan_ip   = wan_ip;
    client.wan_port = wan_port;

    printf("total client:%zu\n", g_name_info.size());
    for(auto iter : g_name_info)
    {
        printf("name:%s lan_ip:%s lan_port:%s wan_ip:%s wan_port:%s\n", 
                iter.first.c_str(), iter.second.lan_ip.c_str(), iter.second.lan_port.c_str(), iter.second.wan_ip.c_str(), iter.second.wan_port.c_str());
    }

    return true;
}
//---------------------------------------------------------------------------
void Punching(int fd, const std::string& buffer, struct sockaddr_in& peer_addr)
{
    //format selfname|punchingname
    
    size_t pos1 = buffer.find("|", 0);
    if(std::string::npos == pos1)
    {
        printf("punching format error\n");
        return;
    }
    std::string self_name       = buffer.substr(0, pos1++);
    std::string punching_name   = buffer.substr(pos1);

    auto iter_self = g_name_info.find(self_name);
    if(g_name_info.end() == iter_self)
    {
        printf("client not Register\n");
        return;
    }

    auto iter = g_name_info.find(punching_name);
    if(g_name_info.end() == iter)
    {
        printf("punching name not Register\n");
        return;
    }

    //punching format lan_ip|lan_port|wan_ip|wan_port
    std::string msg = iter->second.lan_ip + "|" + iter->second.lan_port + "|" + iter->second.wan_ip + "|" + iter->second.wan_port;
    std::string msg1 = iter_self->second.lan_ip + "|" + iter_self->second.lan_port + "|" + iter_self->second.wan_ip + "|" + iter_self->second.wan_port;
    printf("punching msg:%s\n", msg.c_str());
    printf("punching msg self:%s\n", msg1.c_str());

    struct sockaddr_in addr;
    bzero(&addr, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(std::atoi(iter->second.wan_port.c_str()));
    inet_pton(AF_INET, iter->second.wan_ip.c_str(), &addr.sin_addr);

    socklen_t len = sizeof(sockaddr_in);
    if(0 > sendto(fd, msg.data(), msg.size()+1, 0, (sockaddr*)&peer_addr, len))
    {
        perror("send to error");
        return;
    }

    if(0 > sendto(fd, msg1.data(), msg1.size()+1, 0, (sockaddr*)&addr, len))
    {
        perror("send to error1");
        return;
    }

    return;
}
//---------------------------------------------------------------------------
void CMD_Dsipatch(int fd, const std::string& buffer, struct sockaddr_in& peer_addr)
{
    size_t pos = buffer.find("|", 0);
    if(std::string::npos == pos)
    {
        printf("error cmd\n");
        return;
    }

    char buf_ip[8];
    char buf_port[INET_ADDRSTRLEN];
    sprintf(buf_port, "%u", ntohs(peer_addr.sin_port));
    inet_ntop(AF_INET, &peer_addr.sin_addr, buf_ip, INET_ADDRSTRLEN);
    printf("peer client ip:%s, port:%s\n", buf_ip, buf_port);
    printf("msg:%s\n", buffer.c_str());

    std::string cmd = buffer.substr(0, pos);
    std::string buf = buffer.substr(pos+1);
    if(cmd == "register")
    {
        AddClient(buf, buf_ip, buf_port);
    }
    else if(cmd == "punching")
    {
        Punching(fd, buf, peer_addr);
    }

    return;
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

    struct sockaddr_in local_addr;
    bzero(&local_addr, sizeof(struct sockaddr_in));
    local_addr.sin_family       = AF_INET;
    local_addr.sin_port         = htons(PORT);
    inet_pton(AF_INET, IP, &local_addr.sin_addr);

    int opt = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (const void*)&opt, sizeof(opt));

    if(-1 == bind(fd, (struct sockaddr*)&local_addr, sizeof(struct sockaddr)))
    {
        perror("bind:");
        return 1;
    }


    struct sockaddr_in  peer_addr;
    socklen_t           addr_len = sizeof(struct sockaddr_in);
    char                buffer[BUF_LEN];
    while(true)
    {
        int rcv_len = recvfrom(fd, static_cast<void*>(buffer), BUF_LEN, 0, (struct sockaddr*)&peer_addr, &addr_len);
        if(0 >= rcv_len)
        {
            perror("recv_from:");
            return 1;
        }

        CMD_Dsipatch(fd, buffer, peer_addr);
    }

    return 0;
}
