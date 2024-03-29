//
// Created by I3artek on 24/05/2023.
//

#ifndef SPLATTER_NETWORKLAYER_H
#define SPLATTER_NETWORKLAYER_H

#include <vector>
#include "enet/enet.h"
#include <iostream>
#include <game/Player.h>
#include <sstream>
#include <utils/Serialization.h>
#include "Packets.h"


class SocketClient {
public:
    /// connect - connects the client to host with provided address
    virtual bool connect(const char* address_string, ///<[in] any valid address in form x.x.x.x
                         const char* port_string ///<[in] port number
                         ) = 0;
    /// send data to host to which the client is connected
    virtual bool send(Packet& p) = 0;
    /// receive data
    virtual Packet receive() = 0;

	bool is_connected;
};

class SocketServer {
public:
    virtual bool init(const char* address_string, const char* port_string, int max_clients) = 0;
    

    /// send data to host to which the client is connected
    ///<[in] buffer with binary data 
    ///<[in] size of the buffer
    //virtual bool broadcast(char* buf, int size) = 0;

	virtual bool broadcast(Packet p) = 0;

	virtual bool broadcast_except_sender(Packet p) = 0;

    /// Sends data to specified client
    ///<[in] buffer with binary data
    ///<[in] size of the buffer
	//virtual bool send(ENetPeer* target, char* buf, int size);

    /// receive data
    ///<[in,out] number of bytes to be read \n and later size of the returned buffer
    virtual Packet receive() = 0;
};

class EnetServer : public SocketServer {
private:
    static bool enet_initialized;
    ENetHost* server = NULL;
    ENetAddress address;
    ENetEvent event;
public:
    ENetPeer* sender;
    std::vector <ENetPeer*> clients;
    bool init(const char* address_string, const char* port_string, int max_clients) override
    {
        enet_address_set_host(&address, address_string);
        address.port = atoi(port_string);
        server = enet_host_create (& address /* the address to bind the server host to */,
                                   max_clients      /* allow up to 32 clients and/or outgoing connections */,
                                   2      /* allow up to 2 channels to be used, 0 and 1 */,
                                   0      /* assume any amount of incoming bandwidth */,
                                   0      /* assume any amount of outgoing bandwidth */);

        return server != NULL;
    }
//    bool broadcast(char* buf, int size) override
//    {
//        std::vector<ENetPacket*> packets;
//        for(ENetPeer* client : clients)
//        {
//            packets.push_back(enet_packet_create(buf, size, ENET_PACKET_FLAG_RELIABLE));
//            enet_peer_send(client, 0, packet);
//        }
//        enet_host_flush (server);
////        for(ENetPacket* packet_ : packets)
////        {
////            enet_packet_destroy(packet_);
////        }
//        return true;
//    }

	bool broadcast(Packet p) override
	{
        std::cout << "[SERVER] " << "Sending packet with type: " << p.type << std::endl;

		std::vector<ENetPacket*> packets;
		for (ENetPeer* client : clients)
		{

            auto raw_data = spt::serialize(p);

			packets.push_back(enet_packet_create(raw_data.data(), raw_data.size(), ENET_PACKET_FLAG_RELIABLE));
			enet_peer_send(client, 0, packets.back());
		}
		enet_host_flush(server);
//		for(ENetPacket* packet_ : packets)
//		{
//		    enet_packet_destroy(packet_);
//		}
		return true;
	}

    bool send(Packet p, ENetPeer* client)
    {
		std::cout << "[SERVER] " << "Sending packet with type: " << p.type << " to client: " << client->data << std::endl;

        auto raw_data = spt::serialize(p);

        auto ep = enet_packet_create(raw_data.data(), raw_data.size(), ENET_PACKET_FLAG_RELIABLE);
        enet_peer_send(client, 0, ep);

        enet_host_flush(server);
		//enet_packet_destroy(ep);
        return true;
    }
    
   

    Packet receive() override
    {
//        if(packet != NULL)
//        {
//            //enet_packet_destroy(packet);
//            packet = NULL;
//        }
        while (enet_host_service (server, &event, 1000) > 0)
        {
            switch (event.type)
            {
                case ENET_EVENT_TYPE_RECEIVE:
                {
                    sender = event.peer;
                    std::vector<char> temp(event.packet->dataLength);
                    std::memcpy(temp.data(), event.packet->data, event.packet->dataLength);
                    auto p = spt::deserialize<Packet>(temp);
                    enet_packet_destroy(event.packet);
                    return p;
                    break;
                }
                case ENET_EVENT_TYPE_CONNECT:
                    clients.push_back(event.peer);
                    break;
                default:
                    break;
            }
        }
        return {};
    }

//    char* receive_as_bytes()
//    {
//        if(packet != NULL)
//        {
//            //enet_packet_destroy(packet);
//            packet = NULL;
//        }
//        while (enet_host_service (server, &event, 1000) > 0)
//        {
//            switch (event.type)
//            {
//                case ENET_EVENT_TYPE_RECEIVE:
//                    packet = event.packet;
//                    return (char*)packet->data;
//                    break;
//                case ENET_EVENT_TYPE_CONNECT:
//
//					event.peer->data = (void*)"Client";
//                    clients.push_back(event.peer);
//                    return NULL;
//                    break;
//                default:
//                    break;
//            }
//        }
//        return NULL;
//    }

    bool broadcast_except_sender(Packet p) override
    {
		std::cout << "[SERVER] " << "Sending packet with type: " << p.type << std::endl;

		std::vector<ENetPacket*> packets;
		for (ENetPeer* client : clients)
		{
            if (client == sender)
                continue;

			auto raw_data = spt::serialize(p);

			packets.push_back(enet_packet_create(raw_data.data(), raw_data.size(), ENET_PACKET_FLAG_RELIABLE));
			enet_peer_send(client, 0, packets.back());
		}
		enet_host_flush(server);
		//		for(ENetPacket* packet_ : packets)
		//		{
		//		    enet_packet_destroy(packet_);
		//		}
		return true;
    }

};

class EnetClient : public SocketClient {
private:
    ENetHost* client = NULL;
    ENetEvent event;
    ENetPeer* peer = NULL;
    //ENetPacket* packet;
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
            is_connected = true;
			return true;
        }
        else
        {
            enet_peer_reset(peer);
            return false;
        }
    }

    bool send(Packet& p) override
    {
		std::cout << "[CLIENT] " << "Sending packet with type: " << p.type << std::endl;

        std::vector<char> data = spt::serialize(p);

        //uint32_t* xd = (uint32_t*)data.data();

//        if(packet != NULL)
//        {
//            //enet_packet_destroy(packet);
//            packet = NULL;
//        }

        auto packet = enet_packet_create (data.data(), data.size(), ENET_PACKET_FLAG_RELIABLE);
        enet_peer_send (peer, 0, packet);
        enet_host_flush (client);
        //enet_packet_destroy(packet);
        //packet = NULL;
        return true;
    }

    Packet receive() override
    {
        ENetPacket* packet;
        std::vector<char> vec;
        while (enet_host_service (client, &event, 0) > 0)
        {
            Packet ret;
            switch (event.type)
            {
                case ENET_EVENT_TYPE_RECEIVE:
                    
                    packet = event.packet;
                    vec = std::vector<char>(packet->data, packet->data + packet->dataLength);
                    ret = spt::deserialize<Packet>(vec);
                    vec.clear();
		            std::cout << "[CLIENT] " << "Recieving packet with type: " << ret.type << std::endl;
                    enet_packet_destroy(event.packet);
                    return ret;
                    break;
                default:
                    break;
            }
        }
        return Packet();
    }

//    char* receive_as_bytes()
//    {
//        while (enet_host_service (client, &event, 0) > 0)
//        {
//            switch (event.type)
//            {
//                case ENET_EVENT_TYPE_RECEIVE:
//                    packet = event.packet;
//                    return (char*)packet->data;
//                    break;
//                default:
//                    break;
//            }
//        }
//        return NULL;
//    }
};


#endif //SPLATTER_NETWORKLAYER_H
