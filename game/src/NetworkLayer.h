//
// Created by I3artek on 24/05/2023.
//

#ifndef SPLATTER_NETWORKLAYER_H
#define SPLATTER_NETWORKLAYER_H

#include <vector>
#include "enet/enet.h"
#include <iostream>
#include "Player.h"
#include <sstream>
#include <Serialization.h>

struct Packet {
    int type;
	std::vector<char> data;

    template<typename Stream> bool Serialize(Stream & stream)
    {
        serialize_int32(stream, type);
        serialize_char_vector(stream, data, data.size());
    }
};

struct player_move_packet {
    float x;
    float y;

    template<typename Stream> bool Serialize(Stream & stream)
    {
        serialize_float32(stream, x);
        serialize_float32(stream, x);
    }
};

namespace spt
{
    template<typename T>
    T deserialize(std::vector<char> vec)
    {
        T packet;
        auto rd = new ReadStream((uint8_t*)vec.data(), vec.size());
        packet.Serialize(rd);
        return packet;
    }

    template<typename T>
    std::vector<char> serialize(T packet)
    {
        auto vec = std::vector<char>(sizeof(T));
        auto wr = new WriteStream((uint8_t*)vec.data(), sizeof(T));
        packet.Serialize(wr);
        return vec;
    }
}


namespace net {



//    std::vector<char> serialize_packet(Packet& p)
//    {
//        std::vector<char> data;
//        data.push_back(p.type);
//		data.push_back(p.size);
//		data.push_back(p.size);
//
//		for (int i = 0; i < p.size; i++)
//		{
//			data.push_back(p.data[i]);
//		}
//
//		return data;
//    }
//
//    Packet deserialize_packet(std::vector<char> data)
//    {
//        int curr_byte = 0;
//
//        Packet p;
//    }
}

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
};

class SocketServer {
public:
    virtual bool init(const char* address_string, const char* port_string, int max_clients) = 0;
    

    /// send data to host to which the client is connected
    ///<[in] buffer with binary data 
    ///<[in] size of the buffer
    virtual bool broadcast(char* buf, int size) = 0;

	virtual bool broadcast(Packet p) = 0;

    /// Sends data to specified client
    ///<[in] buffer with binary data
    ///<[in] size of the buffer
	//virtual bool send(ENetPeer* target, char* buf, int size);

    /// receive data
    ///<[in,out] number of bytes to be read \n and later size of the returned buffer
    virtual Packet receive() = 0;
};

class EnetServer : SocketServer {
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

        return server != NULL;
    }
    bool broadcast(char* buf, int size) override
    {
        std::vector<ENetPacket*> packets;
        for(ENetPeer* client : clients)
        {
            packets.push_back(enet_packet_create(buf, size, ENET_PACKET_FLAG_NO_ALLOCATE));
            enet_peer_send(client, 0, packet);
        }
        enet_host_flush (server);
//        for(ENetPacket* packet_ : packets)
//        {
//            enet_packet_destroy(packet_);
//        }
        return true;
    }

	bool broadcast(Packet p) override
	{
		std::vector<ENetPacket*> packets;
		for (ENetPeer* client : clients)
		{

            auto raw_data = spt::serialize(p);

			packets.push_back(enet_packet_create(raw_data.data(), raw_data.size(), ENET_PACKET_FLAG_NO_ALLOCATE));
			enet_peer_send(client, 0, packets.back());
		}
		enet_host_flush(server);
		//        for(ENetPacket* packet_ : packets)
		//        {
		//            enet_packet_destroy(packet_);
		//        }
		return true;
	}
    
   

    Packet receive() override
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
                    return spt::deserialize<Packet>(std::vector<char>(
                            (char*)packet->data,
                            (char*)packet->data + packet->dataLength));
                    break;
                case ENET_EVENT_TYPE_CONNECT:
                    clients.push_back(event.peer);
                    break;
                default:
                    break;
            }
        }
        return Packet();
    }

    char* receive_as_bytes()
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
                    return (char*)packet->data;
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

class EnetClient : SocketClient {
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

    bool send(Packet& p) override
    {

        std::vector<char> data = spt::serialize(p);

        if(packet != NULL)
        {
            //enet_packet_destroy(packet);
            packet = NULL;
        }
        packet = enet_packet_create (data.data(), data.size(), ENET_PACKET_FLAG_NO_ALLOCATE);
        enet_peer_send (peer, 0, packet);
        enet_host_flush (client);
        //enet_packet_destroy(packet);
        packet = NULL;
        return true;
    }

    Packet receive() override
    {
        if(packet != NULL)
        {
            //enet_packet_destroy(packet);
            packet = NULL;
        }
        std::vector<char> vec;
        while (enet_host_service (client, &event, 0) > 0)
        {
            Packet ret;
            switch (event.type)
            {
                case ENET_EVENT_TYPE_RECEIVE:
                    packet = event.packet;
                    vec = std::vector<char>((char*)packet->data, (char*)packet->data + packet->dataLength);
                    ret = spt::deserialize<Packet>(vec);
                    vec.clear();
                    return ret;
                    break;
                default:
                    break;
            }
        }
        return Packet();
    }

    char* receive_as_bytes()
    {
        if(packet != NULL)
        {
            //enet_packet_destroy(packet);
            packet = NULL;
        }
        while (enet_host_service (client, &event, 0) > 0)
        {
            switch (event.type)
            {
                case ENET_EVENT_TYPE_RECEIVE:
                    packet = event.packet;
                    return (char*)packet->data;
                    break;
                default:
                    break;
            }
        }
        return NULL;
    }
};


#endif //SPLATTER_NETWORKLAYER_H
