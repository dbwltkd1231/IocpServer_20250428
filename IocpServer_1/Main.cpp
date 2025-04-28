#pragma once

#include "ClientDataSet.h"
#include"CustomOverlapped.h"
#include"LockFreeCircleQueue.h"
#include "IOCompletionPort.h"
#include "EchoServer.h"

//std::map<ULONG_PTR, ClientData*> clientMap;// 클라이언트 소켓과 클라이언트 데이터를 매핑하는 맵

// IOCP에서 완료된 작업을 처리하는 스레드
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
	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)// Winsock 초기화
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
	serverAddr.sin_addr.s_addr = INADDR_ANY;// 모든 IP 주소에서 수신
	serverAddr.sin_port = htons(9090);// 포트 9090

	if (bind(serverSocket, (SOCKADDR*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR)// 소켓에 주소 바인딩
	{
		std::cout << "bind failed" << std::endl;
		closesocket(serverSocket);
		WSACleanup();
		return 1;
	}

	std::cout << "bind success" << std::endl;
	if (listen(serverSocket, SOMAXCONN) == SOCKET_ERROR)// 클라이언트 연결 대기
	{
		std::cout << "listen failed" << std::endl;
		closesocket(serverSocket);
		WSACleanup();
		return 1;
	}
	std::cout << "listen success" << std::endl;

	HANDLE iocpHandle = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 3);// IOCP 핸들 생성 [파일또는소켓핸들, 기존IOCP핸들, 작업식별키, 동시 실행 스레드 수]
	CreateIoCompletionPort((HANDLE)serverSocket, iocpHandle, 0, 0);// IOCP와 소켓 연결
	std::cout << "Iocp Socket Connect" << std::endl;

	std::thread worker(workerThread, iocpHandle);// 작업자 스레드 생성
	worker.detach();// 스레드 분리


	fd_set readSet;
	char buffer[1024];
	WSABUF wsabuf;
	wsabuf.buf = buffer;
	wsabuf.len = sizeof(buffer);

	while (true)
	{
		FD_ZERO(&readSet);// 소켓 집합 초기화
		FD_SET(serverSocket, &readSet);// 소켓 집합에 서버 소켓 추가
		std::cout << "[MainThread] -> Client Socket Connect Wait..." << std::endl;
		int activity = select(0, &readSet, NULL, NULL, NULL);// 소켓 집합에서 활동 감지
		if (activity > 0)
		{
			if (FD_ISSET(serverSocket, &readSet))// 서버 소켓에 연결 요청이 있는 경우
			{
				sockaddr_in clientAddr{};
				int clientAddrSize = sizeof(clientAddr);
				SOCKET clientSocket = accept(serverSocket, (sockaddr*)&clientAddr, &clientAddrSize);// 클라이언트 소켓 수락

				if (clientSocket == INVALID_SOCKET)
				{
					std::cout << "[MainThread] accept failed" << std::endl;
					continue;
				}

				CreateIoCompletionPort((HANDLE)clientSocket, iocpHandle, (ULONG_PTR)clientSocket, 0);// IOCP와 클라이언트 소켓 연결

				ClientData* clientData = new ClientData(std::make_shared<SOCKET>(clientSocket));// 클라이언트 데이터 생성
				clientMap.insert(std::make_pair((ULONG_PTR)clientSocket, clientData));// 클라이언트 소켓과 클라이언트 데이터 매핑
				std::cout << "[MainThread] Client Socket Connect !" << std::endl;
			}
		}
	}
	*/
}

/*
// IOCP에서 완료된 작업을 처리하는 스레드
void workerThread(HANDLE iocpHandle)
{
	OVERLAPPED* overlapped = nullptr; // overlapped: 비동기 작업에 대한 정보를 담고 있는 구조체
	DWORD bytesTransferred = 0; //bytesTransferred: 전송된 바이트 수,
	ULONG_PTR completionKey = 0; //completionKey: 완료 키

	while (true)
	{
		// iocpHandle: IOCP 핸들, bytesTransferred: 전송된 바이트 수, completionKey: 완료 키, overlapped: OVERLAPPED 구조체 포인터, INFINITE: 무한 대기
		BOOL result = GetQueuedCompletionStatus(iocpHandle, &bytesTransferred, &completionKey, &overlapped, INFINITE);// IOCP에서 완료된 작업 대기
		if (result == FALSE || bytesTransferred == 0) // False : 비정상접속종료, 0 : 정상접속종료
		{
			auto it = clientMap.find(completionKey);
			if (it != clientMap.end())
			{
				ClientData* clientData = it->second;
				delete clientData;

				clientMap.erase(completionKey);// 클라이언트 소켓과 클라이언트 데이터 매핑 해제

				std::cout << "[WorkThread] Client disconnected !" << std::endl;
			}
			else
			{
				std::cout << "[WorkThread] Client not found" << std::endl;
			}

			continue;
		}

		// IO_DATA 구조체로 변환
		IO_DATA* ioData = reinterpret_cast<IO_DATA*>(overlapped);
		//IO_DATA* ioData = (IO_DATA *)(overlapped);

		auto it = clientMap.find(completionKey);
		if (it != clientMap.end())
		{
			switch (ioData->operationType)
			{
			case OP_RECV:
			{
				// 받은 메시지 출력
				std::cout << "[WorkThread] Server Received -> "
					<< std::string(ioData->wsabuf.buf, bytesTransferred)
					<< std::endl;

				std::string response = std::string("Sever : ") + std::string(ioData->wsabuf.buf);
				it->second->SendWorker(response);// 클라이언트에게 받은 메시지 전송

				it->second->ReceiveWorker();// 클라이언트로부터 데이터 수신 대기
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
1 단계. 가장 간단한 IOCP 서버. Echo 서버 코드 만들기
IOCP API 동작에 대해서 이해할 수 있다.
에코서버 : 클라이언트가 보낸 메세지를 서버가 그대로 반환하는 서버

*/