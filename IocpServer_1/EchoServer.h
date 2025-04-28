
#pragma once
#include <iostream>
#include<memory>
#include<thread>
#include<map>
#include<set>
#include<atomic>

#include<tbb/concurrent_map.h>

#include"flatbuffers/flatbuffers.h"
#include"MESSAGE_WRAPPER_generated.h"
#include"IOCompletionPort.h"


class EchoServer
{
public:
	EchoServer()
	{

	};
	~EchoServer()
	{

	};

	void Init();
	void Process();

private:
	void PrepareSocket(int count);
	void Send(const Packet packet);
	void Receive(const Packet packet);
	void Accept(const Packet packet);
	void Disconnect(const Packet packet);
	void MessageProcess(const Packet packet);
	std::shared_ptr<ClientDataSet> ClientFinder(long targetID);

	tbb::concurrent_map<int, std::shared_ptr<ClientDataSet>> mClientDataConMap;

	IOCompletionPort mIOCP;
	constexpr static int CLIENT_INIT_COUNT = 1; 
	constexpr static int CLIENT_INCREASE = 1; 

	std::atomic<int> accumulatedClientId = 0;//id �߱޿�
	std::atomic<int> currentClientCount = 0;
	std::atomic<int> currentClientMax = 0;
};

void EchoServer::PrepareSocket(int count)
{
	int currentId = accumulatedClientId.load(std::memory_order_acquire);

	int limit = currentId + count;
	for (; currentId < limit; currentId++)
	{
		auto socketSharedPtr = mIOCP.PrepareSocket();
		mClientDataConMap[currentId] = std::make_shared<ClientDataSet>(currentId, socketSharedPtr);
	}

	//���� �߰��� Ŭ���̾�Ʈ�� TryAccept�ϵ��� �Ѵ�.
	for (auto iterator = mClientDataConMap.begin(); iterator != mClientDataConMap.end(); ++iterator)
	{
		int id = iterator->first;
		mClientDataConMap[id]->TryAccept(mIOCP.mListenSocket, mIOCP.mAcceptExPointer);
	}

	accumulatedClientId.store(currentId, std::memory_order_release);
	currentClientMax.store(currentClientMax.load(std::memory_order_acquire) + count, std::memory_order_release);
}

void EchoServer::Init()
{
	/*
	mIOCP.Init();

	auto acceptCallback = std::function<void(Packet)>(
		std::bind(&EchoServer::Accept, this, std::placeholders::_1)
	);

	auto receiveCallback = std::function<void(Packet)>(
		std::bind(&EchoServer::Receive, this, std::placeholders::_1)
	);

	auto sendCallback = std::function<void(Packet)>(
		std::bind(&EchoServer::Send, this, std::placeholders::_1)
	);

	auto disconnectCallback = std::function<void(Packet)>(
		std::bind(&EchoServer::Disconnect, this, std::placeholders::_1)
	);

	mIOCP.Setting(acceptCallback, receiveCallback, sendCallback, disconnectCallback);

	PrepareSocket(CLIENT_INIT_COUNT);
	std::cout << "�ʱ�ȭ -> �غ�� ���� : " << CLIENT_INIT_COUNT << "\n";
	*/

	//auto name = builder.CreateString("Alex");
	//auto age = 25;
	//
	//builder.Finish(CreatePerson(builder, name, age));//����ȭ �Ϸ�.
	//
	//auto data = builder.GetBufferPointer();//����ȭ�� ���۸� �����´�. �� ���۰��� ��Ʈ��ũ�� �����ϰų� ���Ϸ� �����Ҽ��ִ�.
	//
	//
	//auto person = GetPerson(data);//������ȭ�Ͽ� person��ü�� �����´�.
	//
	//std::cout << "�̸� : " << person->name()->c_str() << "\n";
	//std::cout << "���� : " << person->age() << "\n";
	//
	//
	//builder.Release();//���۸� �����Ѵ�. �̶� ���۴� �Ҹ�ȴ�.
	//builder.Clear();


}

void EchoServer::Process()
{
	//mIOCP.Process();
}

void EchoServer::Accept(const Packet packet)
{
	auto targetPtr = ClientFinder(packet.ClientID);
	if (targetPtr != nullptr)
	{
		targetPtr->FeedBack_Accept(true); 
		currentClientCount.store(currentClientCount.load(std::memory_order_acquire) + 1, std::memory_order_release);

		bool isEmpty = currentClientMax.load(std::memory_order_acquire) - currentClientCount.load(std::memory_order_acquire) <= 0;
		if (isEmpty)
		{
			PrepareSocket(CLIENT_INCREASE);
			std::cout << "������Ϻ��� -> �߰� ���� : " << CLIENT_INCREASE << "\n";
		}
	}
}

void EchoServer::Send(const Packet packet)
{
	long clientID = packet.ClientID;
	auto targetPtr = ClientFinder(packet.ClientID);
	if (targetPtr != nullptr)
	{
		targetPtr->SendRequest(packet.DataSize, packet.Buffer);
	}
}

void EchoServer::Receive(const Packet packet)
{
	auto targetPtr = ClientFinder(packet.ClientID);
	if (targetPtr != nullptr)
	{
		targetPtr->FeedBack_Receive(packet.DataSize, packet.Buffer);
	}
	else
	{
		std::cout << "ID�� ��ġ�ϴ� Client���� Ž�� ����.\n";
	}
}

void EchoServer::Disconnect(const Packet packet)
{
	auto targetPtr = ClientFinder(packet.ClientID);
	if (targetPtr != nullptr)
	{
		std::cout <<"�������� Ȯ��.\n";
		currentClientCount.store(currentClientCount.load(std::memory_order_acquire) - 1, std::memory_order_release);
		targetPtr->TryAccept(mIOCP.mListenSocket, mIOCP.mAcceptExPointer);
	}
}

//find���� finder->second�� �����Ҷ�, iterator�� ��ȿ���� Ȯ���ϴ°� �ʿ��ϴٰ�� �ϴ���, ���� socket�� ��Ȱ���ϰ��ִٺ��� erase�ϴ� �κ��� ��� �״�� �ξ���.
std::shared_ptr<ClientDataSet> EchoServer::ClientFinder(long targetID)
{
	auto finder = mClientDataConMap.find(targetID);
	if (finder != mClientDataConMap.end())
	{
		return finder->second;
	}

	return nullptr;
}

void EchoServer::MessageProcess(const Packet packet)
{
	auto verifier = flatbuffers::Verifier(reinterpret_cast<const uint8_t*>(packet.Buffer), packet.DataSize); // ���ۿ� ũ�⸦ ����Ͽ� Verifier ��ü ����

	auto message_wrapper = flatbuffers::GetRoot<protocol::MESSAGE_WRAPPER>(packet.Buffer); // FlatBuffers ���ۿ��� ��Ʈ ��ü ��������

	switch (message_wrapper->message_type())
	{
	case protocol::MESSAGETYPE::MESSAGETYPE_BEGIN:
		break;

		//Login
	case protocol::MESSAGETYPE::MESSAGETYPE_REQUEST_LOGIN:
		break;
	case protocol::MESSAGETYPE::MESSAGETYPE_RESOPNSE_LOGIN:
		break;

		//Regist
	case protocol::MESSAGETYPE::MESSAGETYPE_REQUEST_REGIST:
		break;
	case protocol::MESSAGETYPE::MESSAGETYPE_RESPONSE_REGIST:
		break;

		//Room Create
	case protocol::MESSAGETYPE::MESSAGETYPE_REQUEST_ROOM_CREATE:
		break;
	case protocol::MESSAGETYPE::MESSAGETYPE_RESPONSE_ROOM_CREATE:
		break;

		//Room Entrance
	case protocol::MESSAGETYPE::MESSAGETYPE_REQUEST_ROOM_ENTRANCE:
		break;
	case protocol::MESSAGETYPE::MESSAGETYPE_RESPONSE_ROOM_ENTRANCE:
		break;

		//Room Leave
	case protocol::MESSAGETYPE::MESSAGETYPE_REQUEST_ROOM_LEAVE:
		break;
	case protocol::MESSAGETYPE::MESSAGETYPE_RESPONSE_ROOM_LEAVE:
		break;

		//send Message
	case protocol::MESSAGETYPE::MESSAGETYPE_REQUEST_SEND_MESSAGE:
		break;
	case protocol::MESSAGETYPE::MESSAGETYPE_RESPONSE_SEND_MESSAGE:
		break;

	case protocol::MESSAGETYPE::MESSAGETYPE_NOTIFY_MESSAGE:
		break;


	case protocol::MESSAGETYPE::MESSAGETYPE_NOTIFY_ROOMLIST:
		break;

		//Logout
	case protocol::MESSAGETYPE::MESSAGETYPE_REQUEST_LOGOUT:
		break;
	case protocol::MESSAGETYPE::MESSAGETYPE_RESPONSE_LOGOUT:
		break;


	case protocol::MESSAGETYPE::MESSAGETYPE_END:
		break;
//	auto root = flatbuffers::GetRoot(packet.Buffer);
}