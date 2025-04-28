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

//1�ʸ��� Ŭ�󿡼� ������ �޼��������µ�, ������ �� ó���� ü���� 3�ʸ��� ��� ó���ϴ� �����̴�.

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
	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)// 2.2 ������ Winsock �ʱ�ȭ
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
	serverAddr.sin_addr.s_addr = INADDR_ANY; // ��� IP �ּ� ����
	serverAddr.sin_port = htons(MY_PORT_NUM); // ��Ʈ 9090

	if (bind(mListenSocket, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR)// ���Ͽ� �ּ� ���ε�
	{
		std::cout << "bind failed" << std::endl;
		closesocket(mListenSocket);
		WSACleanup();
		return;
	}
	std::cout << "bind success" << std::endl;

	if (listen(mListenSocket, SOMAXCONN) == SOCKET_ERROR)// ������ ��ŭ�� ������ŭ Ŭ���̾�Ʈ ���� ���, SOMAXCONN : ���� ������ ������ �� �ִ� ��� ���� ��û�� �ִ� ����
	{
		std::cout << "listen failed" << std::endl;
		closesocket(mListenSocket);
		WSACleanup();
		return;
	}
	std::cout << "listen success" << std::endl;

	// CreateIoCompletionPort�Լ��� IOCP �ڵ��� ������ �����ϴ�IOCP�� ������ ������ ��� ó�����ش�.
	mIOCPHandle = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 3);// IOCP �ڵ� ����.
	if (mIOCPHandle == NULL)
	{
		std::cout << "CreateIoCompletionPort failed" << std::endl;
		closesocket(mListenSocket);
		WSACleanup();
		return;
	}
	std::cout << "CreateIoCompletionPort success" << std::endl;

	// IOCP�� ���� ����.
	if (!CreateIoCompletionPort((HANDLE)mListenSocket, mIOCPHandle, 0, 0))
	{
		std::cout << "CreateIoCompletionPort failed" << std::endl;
		closesocket(mListenSocket);
		WSACleanup();
		return;
	}

	std::cout << "Iocp - Socket Connect" << std::endl;

	//AccpetEx�Լ��� winscok2�� Ȯ�������� ��������Ǿ� ���� �ʱ� ������, ��������� �Լ������͸� ���� �����;��Ѵ�.
	GUID guidAcceptEx = WSAID_ACCEPTEX;//WSAID_ACCEPTEX�� AcceptEx�Լ��� �ĺ��ϴ� GUID.
	DWORD bytesReceived;

	//WSAIoctl�Լ��� GUID�� ���� AcceptEx�Լ��� ã��, acceptExPointer�� �� �ּҸ� �����Ѵ�.
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
	SOCKET newSocket = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, 0, WSA_FLAG_OVERLAPPED);//WSA_FLAG_OVERLAPPED��  overlapped i/o�� ����ϱ� ���� �÷���.->�񵿱��۾� ���డ��������.
	CreateIoCompletionPort((HANDLE)newSocket, mIOCPHandle, (ULONG_PTR)newSocket, 0);// IOCP�� Ŭ���̾�Ʈ ���� ����
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
	std::thread acceptThread(&IOCompletionPort::AcceptWorker, this);//thread�� Ŭ���� ����Լ��� �ٷ������Ҽ�����, ��ü�� ����Լ��� ���� �����͸� �����ؾ��Ѵ�.
	acceptThread.detach();

	std::thread sendThread(&IOCompletionPort::SendWorker, this);
	sendThread.detach();

	std::thread receiveThread(&IOCompletionPort::ReceiveWorker, this);
	receiveThread.detach();

	std::thread disconnectThread(&IOCompletionPort::DisconnectWorker, this);
	disconnectThread.detach();

	CustomOverlapped* customOverlapped = nullptr; // overlapped: �񵿱� �۾��� ���� ������ ��� �ִ� ����ü
	DWORD bytesTransferred = 0; //bytesTransferred: ���۵� ����Ʈ ��,
	ULONG_PTR completionKey = 0; //completionKey: �Ϸ� Ű

	while (mServerOn)
	{
		customOverlapped = nullptr;
		bytesTransferred = 0;
		completionKey = 0;

		// iocpHandle: IOCP �ڵ�, bytesTransferred: ���۵� ����Ʈ ��, completionKey: �Ϸ� Ű, overlapped: OVERLAPPED ����ü ������, INFINITE: ���� ���
		BOOL result = GetQueuedCompletionStatus(mIOCPHandle, &bytesTransferred, &completionKey, reinterpret_cast<LPOVERLAPPED*>(&customOverlapped), INFINITE);// IOCP���� �Ϸ�� �۾� ���

		if (result == true)
		{
			if (customOverlapped == nullptr)
				continue;

			auto targetOverlapped = static_cast<CustomOverlapped*>(customOverlapped);	//������Ÿ�Ժ�ȯ�̶� ��ü�����̽��� �߻����� �ʴ´ٰ� �Ѵ�.
			std::cout << "Message Received! \n";
	
			Packet newPacket;
			operationType type = targetOverlapped->operationType;
			long id = targetOverlapped->id;
		//auto sharedOverlapped = std::shared_ptr<CustomOverlapped>(
		//	targetOverlapped,
		//	[](CustomOverlapped* ptr) {
		//		/*���� ���� - �޸𸮸� ���� �������� ����
		//		std::shared_ptr�� ��ü�� �������� �����ϸ�, ���� ī��Ʈ�� ������� �޸𸮸� �����մϴ�.
		//		������ customOverlapped�� IOCP���� ��ȯ�� �����ͷ�, �̹� �ٸ� ������ �޸𸮸� �����ϰ� ���� ���ɼ��� �ֽ��ϴ�.
		//		�� ���, std::shared_ptr�� ������ �޸𸮸� �ٽ� �����Ϸ��� �õ��ϸ鼭 ���� ����(Double Free) ������ �߻��� �� �ֽ��ϴ�
		//		*/
		//		std::cout << "CustomOverlapped deletion skipped (IOCP-managed).\n";
		//	});

			if (bytesTransferred == 0 && type != operationType::OP_ACCEPT)
			{
				//Ŭ���̾�Ʈ ��������.
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
				//���ؽ��ʿ�-> �̰����� ���� ����?
				newPacket.Set(targetOverlapped->id);
				acceptQueue.push(std::move(newPacket));
				break;
			}
			case operationType::OP_RECV:
			{
				newPacket.Set(targetOverlapped->id, targetOverlapped->wsabuf.buf, bytesTransferred);//targetOverlapped->wsabuf.len�� �ƴ϶� bytesTransferred���µ� ���ݱ��� �߸�����?
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
				//Ŭ���̾�Ʈ ��������.

				newPacket.Set(id);
				std::cout << "Client "<< id <<" Disconnected!\n";
				disconnectQueue.push(std::move(newPacket));
				continue;
			}
		}
	}
}

// ���⼭ connect�� disconnect�� �ѹ��� ó���ϴ� ��� �� ������?
// �ܼ��ϰ� �۾��� �켱������ �����ϸ鼭 �۾��� �������� ���°�� �Ʒ��Ͱ���������� ó���ϴ°��� ���?
// �۾������� �Ѱ��� ���־� ���Ἲ�� �����ϰ� ������������, ������ ���� �ٿ� �ڿ��Ҹ��� ����. 
// �۾����� ������� ����ó�� ����, �۾��� �������ɼ�( ex: accept���� dsicconect�����¹߻�)
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