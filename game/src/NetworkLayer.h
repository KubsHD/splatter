//
// Created by I3artek on 24/05/2023.
//

#ifndef SPLATTER_NETWORKLAYER_H
#define SPLATTER_NETWORKLAYER_H

#include <vector>
#include "enet/enet.h"

class Client {
public:
    /// connect - connects the client to host with provided address
    virtual bool connect(const char* address_string, ///<[in] any valid address in form x.x.x.x
                         const char* port_string ///<[in] port number
                         ) = 0;
    /// send data to host to which the client is connected
    virtual bool send(char* buf, ///<[in] buffer with binary data
                      int size ///<[in] size of the buffer
                      ) = 0;
    /// receive data
    virtual char* receive(int* size ///<[in,out] number of bytes to be read \n and later size of the returned buffer
    ) = 0;
};

class Server {
public:
    virtual bool init(const char* address_string, const char* port_string, int max_clients) = 0;
    /// send data to host to which the client is connected
    virtual bool send_to_all(char* buf, ///<[in] buffer with binary data
                      int size ///<[in] size of the buffer
    ) = 0;
    /// receive data
    virtual char* receive(int* size ///<[in,out] number of bytes to be read \n and later size of the returned buffer
    ) = 0;
};

class EnetServer : Server {
private:
    static bool enet_initialized;
    ENetHost* server = NULL;
    ENetAddress address;
    ENetEvent event;
    std::vector <ENetPeer*> clients;
    ENetPacket* packet = NULL;
public:
    bool init(const char* address_string, const char* port_string, int max_clients) override
    {
        enet_address_set_host(&address, address_string);
        address.port = atoi(port_string);
        server = enet_host_create (& address /* the address to bind the server host to */,
                                   max_clients      /* allow up to 32 clients and/or outgoing connections */,
                                   2      /* allow up to 2 channels to be used, 0 and 1 */,
                                   0      /* assume any amount of incoming bandwidth */,
                                   0      /* assume any amount of outgoing bandwidth */);
        if (server == NULL)
        {
            return false;
        }
    }
    bool send_to_all(char* buf, int size) override
    {
        std::vector<ENetPacket*> packets;
        for(ENetPeer* client : clients)
        {
            packets.push_back(enet_packet_create(buf, size, 0));
            enet_peer_send(client, 0, packet);
        }
        enet_host_flush (server);
//        for(ENetPacket* packet_ : packets)
//        {
//            enet_packet_destroy(packet_);
//        }
        return true;
    }
    char* receive(int* size) override
    {
        if(packet != NULL)
        {
            //enet_packet_destroy(packet);
            packet = NULL;
        }
        while (enet_host_service (server, &event, 1000) > 0)
        {
            switch (event.type)
            {
                case ENET_EVENT_TYPE_RECEIVE:
                    packet = event.packet;
                    if(size != NULL)
                    {
                        *size = packet->dataLength;
                    }
                    return (char* )packet->data;
                    break;
                case ENET_EVENT_TYPE_CONNECT:
                    clients.push_back(event.peer);
                    break;
                default:
                    break;
            }
        }
        return NULL;
    }
};

class EnetClient : Client {
private:
    ENetHost* client = NULL;
    ENetEvent event;
    ENetPeer* peer = NULL;
    ENetPacket* packet;
public:
    bool connect(const char* address_string, const char* port_string) override
    {
        if(client == nullptr)
        {
            client = enet_host_create (NULL /* create a client host */,
                                       1 /* only allow 1 outgoing connection */,
                                       2 /* allow up 2 channels to be used, 0 and 1 */,
                                       0 /* assume any amount of incoming bandwidth */,
                                       0 /* assume any amount of outgoing bandwidth */);
            if (client == NULL)
            {
                return false;
            }
        }

        ENetAddress addr;
        enet_address_set_host(&addr, address_string);
        addr.port = atoi(port_string);
        peer = enet_host_connect(client, &addr, 2, 0);
        if (peer == NULL)
        {
            return false;
        }
        if (enet_host_service (client, & event, 5000) > 0 &&
            event.type == ENET_EVENT_TYPE_CONNECT)
        {
            return true;
        }
        else
        {
            enet_peer_reset(peer);
            return false;
        }
    }

    bool send(char* buf, int size) override
    {
        if(packet != NULL)
        {
            enet_packet_destroy(packet);
            packet = NULL;
        }
        packet = enet_packet_create (buf, size, 0);
        enet_peer_send (peer, 0, packet);
        enet_host_flush (client);
        //enet_packet_destroy(packet);
        packet = NULL;
        return true;
    }

    char* receive(int *size) override
    {
        if(packet != NULL)
        {
            //enet_packet_destroy(packet);
            packet = NULL;
        }
        while (enet_host_service (client, &event, 17) > 0)
        {
            switch (event.type)
            {
                case ENET_EVENT_TYPE_RECEIVE:
                    packet = event.packet;
                    if(size != NULL)
                    {
                        *size = packet->dataLength;
                    }
                    return (char* )packet->data;
                    break;
                default:
                    break;
            }
        }
        return NULL;
    }
};


#endif //SPLATTER_NETWORKLAYER_H