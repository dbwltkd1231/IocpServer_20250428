#pragma once

#include "ClientDataSet.h"
#include"CustomOverlapped.h"
#include"LockFreeCircleQueue.h"
#include "IOCompletionPort.h"
#include "EchoServer.h"

//std::map<ULONG_PTR, ClientData*> clientMap;// Ŭ���̾�Ʈ ���ϰ� Ŭ���̾�Ʈ �����͸� �����ϴ� ��

// IOCP���� �Ϸ�� �۾��� ó���ϴ� ������
//void workerThread(HANDLE iocpHandle);

int main()
{
	EchoServer echoServer;

	echoServer.Init();
	echoServer.Process();

	while (true)
	{

	}

	/*
	WSADATA wsaData;
	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)// Winsock �ʱ�ȭ
	{
		std::cout << "WSAStartup failed" << std::endl;
		return 1;
	}

	SOCKET serverSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (serverSocket == INVALID_SOCKET)
	{
		std::cout << "socket failed" << std::endl;
		WSACleanup();
		return 1;
	}

	sockaddr_in serverAddr{};
	serverAddr.sin_family = AF_INET;// IPv4
	serverAddr.sin_addr.s_addr = INADDR_ANY;// ��� IP �ּҿ��� ����
	serverAddr.sin_port = htons(9090);// ��Ʈ 9090

	if (bind(serverSocket, (SOCKADDR*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR)// ���Ͽ� �ּ� ���ε�
	{
		std::cout << "bind failed" << std::endl;
		closesocket(serverSocket);
		WSACleanup();
		return 1;
	}

	std::cout << "bind success" << std::endl;
	if (listen(serverSocket, SOMAXCONN) == SOCKET_ERROR)// Ŭ���̾�Ʈ ���� ���
	{
		std::cout << "listen failed" << std::endl;
		closesocket(serverSocket);
		WSACleanup();
		return 1;
	}
	std::cout << "listen success" << std::endl;

	HANDLE iocpHandle = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 3);// IOCP �ڵ� ���� [���϶Ǵ¼����ڵ�, ����IOCP�ڵ�, �۾��ĺ�Ű, ���� ���� ������ ��]
	CreateIoCompletionPort((HANDLE)serverSocket, iocpHandle, 0, 0);// IOCP�� ���� ����
	std::cout << "Iocp Socket Connect" << std::endl;

	std::thread worker(workerThread, iocpHandle);// �۾��� ������ ����
	worker.detach();// ������ �и�


	fd_set readSet;
	char buffer[1024];
	WSABUF wsabuf;
	wsabuf.buf = buffer;
	wsabuf.len = sizeof(buffer);

	while (true)
	{
		FD_ZERO(&readSet);// ���� ���� �ʱ�ȭ
		FD_SET(serverSocket, &readSet);// ���� ���տ� ���� ���� �߰�
		std::cout << "[MainThread] -> Client Socket Connect Wait..." << std::endl;
		int activity = select(0, &readSet, NULL, NULL, NULL);// ���� ���տ��� Ȱ�� ����
		if (activity > 0)
		{
			if (FD_ISSET(serverSocket, &readSet))// ���� ���Ͽ� ���� ��û�� �ִ� ���
			{
				sockaddr_in clientAddr{};
				int clientAddrSize = sizeof(clientAddr);
				SOCKET clientSocket = accept(serverSocket, (sockaddr*)&clientAddr, &clientAddrSize);// Ŭ���̾�Ʈ ���� ����

				if (clientSocket == INVALID_SOCKET)
				{
					std::cout << "[MainThread] accept failed" << std::endl;
					continue;
				}

				CreateIoCompletionPort((HANDLE)clientSocket, iocpHandle, (ULONG_PTR)clientSocket, 0);// IOCP�� Ŭ���̾�Ʈ ���� ����

				ClientData* clientData = new ClientData(std::make_shared<SOCKET>(clientSocket));// Ŭ���̾�Ʈ ������ ����
				clientMap.insert(std::make_pair((ULONG_PTR)clientSocket, clientData));// Ŭ���̾�Ʈ ���ϰ� Ŭ���̾�Ʈ ������ ����
				std::cout << "[MainThread] Client Socket Connect !" << std::endl;
			}
		}
	}
	*/
}

/*
// IOCP���� �Ϸ�� �۾��� ó���ϴ� ������
void workerThread(HANDLE iocpHandle)
{
	OVERLAPPED* overlapped = nullptr; // overlapped: �񵿱� �۾��� ���� ������ ��� �ִ� ����ü
	DWORD bytesTransferred = 0; //bytesTransferred: ���۵� ����Ʈ ��,
	ULONG_PTR completionKey = 0; //completionKey: �Ϸ� Ű

	while (true)
	{
		// iocpHandle: IOCP �ڵ�, bytesTransferred: ���۵� ����Ʈ ��, completionKey: �Ϸ� Ű, overlapped: OVERLAPPED ����ü ������, INFINITE: ���� ���
		BOOL result = GetQueuedCompletionStatus(iocpHandle, &bytesTransferred, &completionKey, &overlapped, INFINITE);// IOCP���� �Ϸ�� �۾� ���
		if (result == FALSE || bytesTransferred == 0) // False : ��������������, 0 : ������������
		{
			auto it = clientMap.find(completionKey);
			if (it != clientMap.end())
			{
				ClientData* clientData = it->second;
				delete clientData;

				clientMap.erase(completionKey);// Ŭ���̾�Ʈ ���ϰ� Ŭ���̾�Ʈ ������ ���� ����

				std::cout << "[WorkThread] Client disconnected !" << std::endl;
			}
			else
			{
				std::cout << "[WorkThread] Client not found" << std::endl;
			}

			continue;
		}

		// IO_DATA ����ü�� ��ȯ
		IO_DATA* ioData = reinterpret_cast<IO_DATA*>(overlapped);
		//IO_DATA* ioData = (IO_DATA *)(overlapped);

		auto it = clientMap.find(completionKey);
		if (it != clientMap.end())
		{
			switch (ioData->operationType)
			{
			case OP_RECV:
			{
				// ���� �޽��� ���
				std::cout << "[WorkThread] Server Received -> "
					<< std::string(ioData->wsabuf.buf, bytesTransferred)
					<< std::endl;

				std::string response = std::string("Sever : ") + std::string(ioData->wsabuf.buf);
				it->second->SendWorker(response);// Ŭ���̾�Ʈ���� ���� �޽��� ����

				it->second->ReceiveWorker();// Ŭ���̾�Ʈ�κ��� ������ ���� ���
			}
				break;
			case OP_SEND:
				std::cout << "[WorkThread] Server Send -> " << std::string(ioData->wsabuf.buf, bytesTransferred) << std::endl;
				break;
			case OP_DEFAULT:
				std::cout << "[WorkThread] -> OP_DEFAULT " << std::endl;
				break;
			}
		}
		else
		{
			std::cout << "[WorkThread] Client not found" << std::endl;
		}
	}
}
*/

/*
1 �ܰ�. ���� ������ IOCP ����. Echo ���� �ڵ� �����
IOCP API ���ۿ� ���ؼ� ������ �� �ִ�.
���ڼ��� : Ŭ���̾�Ʈ�� ���� �޼����� ������ �״�� ��ȯ�ϴ� ����

*/