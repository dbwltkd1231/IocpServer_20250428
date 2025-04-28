
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

	std::atomic<int> accumulatedClientId = 0;//id 발급용
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

	//새로 추가된 클라이언트만 TryAccept하도록 한다.
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
	std::cout << "초기화 -> 준비된 소켓 : " << CLIENT_INIT_COUNT << "\n";
	*/

	//auto name = builder.CreateString("Alex");
	//auto age = 25;
	//
	//builder.Finish(CreatePerson(builder, name, age));//직렬화 완료.
	//
	//auto data = builder.GetBufferPointer();//직렬화된 버퍼를 가져온다. 이 버퍼값을 네트워크로 전송하거나 파일로 저장할수있다.
	//
	//
	//auto person = GetPerson(data);//역직렬화하여 person객체로 가져온다.
	//
	//std::cout << "이름 : " << person->name()->c_str() << "\n";
	//std::cout << "나이 : " << person->age() << "\n";
	//
	//
	//builder.Release();//버퍼를 해제한다. 이때 버퍼는 소멸된다.
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
			std::cout << "가용소켓부족 -> 추가 소켓 : " << CLIENT_INCREASE << "\n";
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
		std::cout << "ID와 일치하는 Client소켓 탐색 실패.\n";
	}
}

void EchoServer::Disconnect(const Packet packet)
{
	auto targetPtr = ClientFinder(packet.ClientID);
	if (targetPtr != nullptr)
	{
		std::cout <<"연결해제 확인.\n";
		currentClientCount.store(currentClientCount.load(std::memory_order_acquire) - 1, std::memory_order_release);
		targetPtr->TryAccept(mIOCP.mListenSocket, mIOCP.mAcceptExPointer);
	}
}

//find이후 finder->second로 접근할때, iterator가 유효한지 확인하는게 필요하다고는 하던데, 나는 socket을 재활용하고있다보니 erase하는 부분이 없어서 그대로 두었다.
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
	auto verifier = flatbuffers::Verifier(reinterpret_cast<const uint8_t*>(packet.Buffer), packet.DataSize); // 버퍼와 크기를 사용하여 Verifier 객체 생성

	auto message_wrapper = flatbuffers::GetRoot<protocol::MESSAGE_WRAPPER>(packet.Buffer); // FlatBuffers 버퍼에서 루트 객체 가져오기

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