#pragma once

#include <iostream>
#include<set>
#include<list>
#include<memory>
#include<thread>
#include<string>
#include<map>
#include<functional>

#define NOMINMAX
#include <winsock2.h>
#include<MSWSock.h>
#include <windows.h>

#include"CustomOverlapped.h"
#include"LockFreeCircleQueue.h"


#define MY_PORT_NUM 9090
#define BUFFER_SIZE 1024

//1초마다 클라에서 서버에 메세지보내는데, 서버는 이 처리를 체감상 3초마다 묶어서 처리하는 느낌이다.

class IOCompletionPort
{
public:
	IOCompletionPort()
	{
		mServerOn = true;
	}
	~IOCompletionPort()
	{
		mServerOn = false;
	}
	void Init();
	void Setting(std::function<void(Packet)> acceptCallback,
		std::function<void(Packet)> receiveCallback,
		std::function<void(Packet)> sendCallback,
		std::function<void(Packet)> disconnectCallback);
	void Process();
	std::shared_ptr<SOCKET> PrepareSocket();

	SOCKET mListenSocket = INVALID_SOCKET;
	LPFN_ACCEPTEX mAcceptExPointer = nullptr;
private:
	void AcceptWorker();
	void SendWorker();
	void ReceiveWorker();
	void DisconnectWorker();

	HANDLE mIOCPHandle = INVALID_HANDLE_VALUE;
	bool mServerOn;

	LockFreeCircleQueue<Packet> acceptQueue;
	LockFreeCircleQueue<Packet> sendQueue;
	LockFreeCircleQueue<Packet> receiveQueue;
	LockFreeCircleQueue<Packet> disconnectQueue;

	std::function<void(Packet)> acceptCallback;
	std::function<void(Packet)> receiveCallback;
	std::function<void(Packet)> sendCallback;
	std::function<void(Packet)> disconnectCallback;
};

void IOCompletionPort::Init()
{
	WSADATA wsaData;
	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)// 2.2 버전의 Winsock 초기화
	{
		std::cout << "WSAStartup failed" << std::endl;
		return;
	}

	mListenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (mListenSocket == INVALID_SOCKET)
	{
		std::cout << "listenSocket Create failed" << std::endl;
		WSACleanup();
		return;
	}

	sockaddr_in serverAddr{};
	serverAddr.sin_family = AF_INET; // IPv4
	serverAddr.sin_addr.s_addr = INADDR_ANY; // 모든 IP 주소 수신
	serverAddr.sin_port = htons(MY_PORT_NUM); // 포트 9090

	if (bind(mListenSocket, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR)// 소켓에 주소 바인딩
	{
		std::cout << "bind failed" << std::endl;
		closesocket(mListenSocket);
		WSACleanup();
		return;
	}
	std::cout << "bind success" << std::endl;

	if (listen(mListenSocket, SOMAXCONN) == SOCKET_ERROR)// 설정한 만큼의 개수만큼 클라이언트 연결 대기, SOMAXCONN : 서버 소켓이 수용할 수 있는 대기 연결 요청의 최대 개수
	{
		std::cout << "listen failed" << std::endl;
		closesocket(mListenSocket);
		WSACleanup();
		return;
	}
	std::cout << "listen success" << std::endl;

	// CreateIoCompletionPort함수는 IOCP 핸들을 생성과 존재하는IOCP와 소켓을 연결을 모두 처리해준다.
	mIOCPHandle = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 3);// IOCP 핸들 생성.
	if (mIOCPHandle == NULL)
	{
		std::cout << "CreateIoCompletionPort failed" << std::endl;
		closesocket(mListenSocket);
		WSACleanup();
		return;
	}
	std::cout << "CreateIoCompletionPort success" << std::endl;

	// IOCP와 소켓 연결.
	if (!CreateIoCompletionPort((HANDLE)mListenSocket, mIOCPHandle, 0, 0))
	{
		std::cout << "CreateIoCompletionPort failed" << std::endl;
		closesocket(mListenSocket);
		WSACleanup();
		return;
	}

	std::cout << "Iocp - Socket Connect" << std::endl;

	//AccpetEx함수는 winscok2의 확장기능으로 직접선언되어 있지 않기 때문에, 명시적으로 함수포인터를 통해 가져와야한다.
	GUID guidAcceptEx = WSAID_ACCEPTEX;//WSAID_ACCEPTEX는 AcceptEx함수를 식별하는 GUID.
	DWORD bytesReceived;

	//WSAIoctl함수는 GUID를 통해 AcceptEx함수를 찾고, acceptExPointer에 그 주소를 저장한다.
	if (WSAIoctl(mListenSocket, SIO_GET_EXTENSION_FUNCTION_POINTER,
		&guidAcceptEx, sizeof(guidAcceptEx), &mAcceptExPointer, sizeof(mAcceptExPointer),
		&bytesReceived, NULL, NULL) == SOCKET_ERROR)
	{
		std::cout << "WSAIoctl failed" << std::endl;
	}

	mServerOn = true;
	std::cout << "IOCP Port Ready Complete" << std::endl;
}

std::shared_ptr<SOCKET> IOCompletionPort::PrepareSocket()
{
	SOCKET newSocket = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, 0, WSA_FLAG_OVERLAPPED);//WSA_FLAG_OVERLAPPED는  overlapped i/o를 사용하기 위한 플래그.->비동기작업 수행가능해진다.
	CreateIoCompletionPort((HANDLE)newSocket, mIOCPHandle, (ULONG_PTR)newSocket, 0);// IOCP와 클라이언트 소켓 연결
	auto socketSharedPtr = std::make_shared<SOCKET>(newSocket);

	return socketSharedPtr;
}

void IOCompletionPort::Setting(std::function<void(Packet)> acceptCallback,
	std::function<void(Packet)> receiveCallback,
	std::function<void(Packet)> sendCallback,
	std::function<void(Packet)> disconnectCallback)
{
	this->acceptCallback = acceptCallback;
	this->receiveCallback = receiveCallback;
	this->sendCallback = sendCallback;
	this->disconnectCallback = disconnectCallback;
}

void IOCompletionPort::Process()
{
	std::thread acceptThread(&IOCompletionPort::AcceptWorker, this);//thread는 클래스 멤버함수에 바로접근할수없고, 객체의 멤버함수에 대한 포인터를 전달해야한다.
	acceptThread.detach();

	std::thread sendThread(&IOCompletionPort::SendWorker, this);
	sendThread.detach();

	std::thread receiveThread(&IOCompletionPort::ReceiveWorker, this);
	receiveThread.detach();

	std::thread disconnectThread(&IOCompletionPort::DisconnectWorker, this);
	disconnectThread.detach();

	CustomOverlapped* customOverlapped = nullptr; // overlapped: 비동기 작업에 대한 정보를 담고 있는 구조체
	DWORD bytesTransferred = 0; //bytesTransferred: 전송된 바이트 수,
	ULONG_PTR completionKey = 0; //completionKey: 완료 키

	while (mServerOn)
	{
		customOverlapped = nullptr;
		bytesTransferred = 0;
		completionKey = 0;

		// iocpHandle: IOCP 핸들, bytesTransferred: 전송된 바이트 수, completionKey: 완료 키, overlapped: OVERLAPPED 구조체 포인터, INFINITE: 무한 대기
		BOOL result = GetQueuedCompletionStatus(mIOCPHandle, &bytesTransferred, &completionKey, reinterpret_cast<LPOVERLAPPED*>(&customOverlapped), INFINITE);// IOCP에서 완료된 작업 대기

		if (result == true)
		{
			if (customOverlapped == nullptr)
				continue;

			auto targetOverlapped = static_cast<CustomOverlapped*>(customOverlapped);	//포인터타입변환이라 객체슬라이싱이 발생하지 않는다고 한다.
			std::cout << "Message Received! \n";
	
			Packet newPacket;
			operationType type = targetOverlapped->operationType;
			long id = targetOverlapped->id;
		//auto sharedOverlapped = std::shared_ptr<CustomOverlapped>(
		//	targetOverlapped,
		//	[](CustomOverlapped* ptr) {
		//		/*삭제 로직 - 메모리를 직접 해제하지 않음
		//		std::shared_ptr는 객체의 소유권을 관리하며, 참조 카운트를 기반으로 메모리를 해제합니다.
		//		하지만 customOverlapped는 IOCP에서 반환된 포인터로, 이미 다른 곳에서 메모리를 관리하고 있을 가능성이 있습니다.
		//		이 경우, std::shared_ptr가 동일한 메모리를 다시 해제하려고 시도하면서 이중 해제(Double Free) 문제가 발생할 수 있습니다
		//		*/
		//		std::cout << "CustomOverlapped deletion skipped (IOCP-managed).\n";
		//	});

			if (bytesTransferred == 0 && type != operationType::OP_ACCEPT)
			{
				//클라이언트 접속종료.
				newPacket.Set(targetOverlapped->id);
				std::cout << "Client " << id << " Disconnected!\n";
				disconnectQueue.push(std::move(newPacket));
				continue;
			}

			switch (type)
			{
			case operationType::OP_DEFAULT:
			{
				std::cout << "Default ????\n";
				break;
			}
			case operationType::OP_ACCEPT:
			{
				//뮤텍스필요-> 이곳에서 락은 위험?
				newPacket.Set(targetOverlapped->id);
				acceptQueue.push(std::move(newPacket));
				break;
			}
			case operationType::OP_RECV:
			{
				newPacket.Set(targetOverlapped->id, targetOverlapped->wsabuf.buf, bytesTransferred);//targetOverlapped->wsabuf.len이 아니라 bytesTransferred였는데 지금까지 잘못쓴듯?
				receiveQueue.push(std::move(newPacket));
				break;
			}
			case operationType::OP_SEND:
			{
				newPacket.Set(targetOverlapped->id, targetOverlapped->wsabuf.buf, bytesTransferred);
				sendQueue.push(std::move(newPacket));
				break;
			}
			}
		}
		else
		{
			if (customOverlapped == nullptr)
				continue;

			auto targetOverlapped = static_cast<CustomOverlapped*>(customOverlapped);
			operationType type = targetOverlapped->operationType;
			Packet newPacket;
			long id = targetOverlapped->id;

			if (bytesTransferred == 0 && type != operationType::OP_ACCEPT)
			{
				//클라이언트 접속종료.

				newPacket.Set(id);
				std::cout << "Client "<< id <<" Disconnected!\n";
				disconnectQueue.push(std::move(newPacket));
				continue;
			}
		}
	}
}

// 여기서 connect와 disconnect를 한번에 처리하는 방법 은 없을까?
// 단순하고 작업의 우선순위가 동일하면서 작업간 의존성이 없는경우 아래와같은방식으로 처리하는것은 어떨까?
// 작업로직이 한곳에 모여있어 간결성이 증가하고 유지보수용이, 스레드 수를 줄여 자원소모비용 절감. 
// 작업량이 많을경우 병렬처리 부족, 작업의 지연가능성( ex: accept동안 dsicconect대기상태발생)
void IOCompletionPort::AcceptWorker()
{
	while (mServerOn)
	{
		if (acceptQueue.empty())
			continue;

		if (!acceptCallback)
			continue;

		acceptCallback(acceptQueue.Front());
		acceptQueue.pop();
	}
}

void IOCompletionPort::SendWorker()
{
	while (mServerOn)
	{
		if (sendQueue.empty())
			continue;

		if (!sendCallback)
			continue;

		sendCallback(sendQueue.Front());
		sendQueue.pop();
	}
}

void IOCompletionPort::ReceiveWorker()
{
	while (mServerOn)
	{
		if (receiveQueue.empty())
			continue;

		if (!receiveCallback)
			continue;

		receiveCallback(receiveQueue.Front());
		receiveQueue.pop();
	}
}

void IOCompletionPort::DisconnectWorker()
{
	while (mServerOn)
	{
		if (disconnectQueue.empty())
			continue;
		if (!disconnectCallback)
			continue;

		disconnectCallback(disconnectQueue.Front());
		disconnectQueue.pop();
	}
}