#pragma once

#include<memory>

#include<tbb/concurrent_vector.h>

#include "IOCompletionPort.h"
#include "LockFreeCircleQueue.h"

struct DataPacket
{
	int ID;
	char* buffer;
	UINT32 dataSize;

	~DataPacket()
	{
		delete[] buffer;
	}
};

class ClientDataSet
{
public:
	ClientDataSet(int id, std::shared_ptr<SOCKET> clientSocketPtr) : mID(id), mClientSocketPtr(clientSocketPtr)
	{
		mIsConnet = false;
	}
	~ClientDataSet()
	{
		//클라이언트 소켓은 재활용중. 삭제하지 않고있음.
	}

	bool TryAccept(SOCKET& listenSocket, LPFN_ACCEPTEX& acceptExPointer);
	void FeedBack_Accept(bool feedback);
	void FeedBack_Receive(const UINT32 dataSize, char* messageBuff);
	bool SendRequest(const UINT32 dataSize, char* messageBuff);
	void FeedBack_Send();

private:
	void SendInternalWork();
	bool ReceiveReady();	//이거 WSARecv가 연속해서오고 2번째가 WSARecv 재호출 전이면 씹히려나?
	void ReceiveInternalWork();

	LockFreeCircleQueue<DataPacket*> mReceiveDataQueue;
	LockFreeCircleQueue<CustomOverlapped*> mSendDataQueue;

	tbb::concurrent_vector<char> mReceiveBuffer;//멀티쓰레딩환경에서 안전한 vector

	int mID;
	bool mIsConnet;

	constexpr static UINT32 mBufferSize = 10;
	std::shared_ptr<SOCKET> mClientSocketPtr = nullptr;

	CustomOverlapped mAcceptContext;
	CustomOverlapped mReceiveContext;
	CustomOverlapped mSendContext;

	char mAcceptBuf[MAX_SOCK_ACCEPTBUF];
	char mReceiveBuf[MAX_SOCK_RECVBUF];
};

bool ClientDataSet::TryAccept(SOCKET& listenSocket, LPFN_ACCEPTEX& acceptExPointer)
{
	if (mIsConnet)
		return false;

	//재활용된 소켓의 상태검증
	int errorCode;
	int errorCodeSize = sizeof(errorCode);
	getsockopt(*mClientSocketPtr, SOL_SOCKET, SO_ERROR, (char*)&errorCode, &errorCodeSize);
	if (errorCode != 0) {
		std::cerr << "Socket error detected: " << errorCode << std::endl;
		return false;
	}

	// 멀티플렉싱 방법 중 select, poll의 경우 소켓수 제한이 있어(windows 기본값 64개), 많은 클라이언트 연결을 처리하기 힘들다.
	// AcceptEx를 IOCP와 함께 사용하면, 대규모 동시연결을 효율적으로 처리할수있다.

	ZeroMemory(&mAcceptContext, sizeof(mAcceptContext));
	mAcceptContext.operationType = operationType::OP_ACCEPT;
	mAcceptContext.id = mID;
	mAcceptContext.wsabuf.buf = mAcceptBuf;          // 버퍼 주소 설정
	mAcceptContext.wsabuf.len = sizeof(mAcceptBuf);  // 버퍼 크기 설정 -> 이런식으로만 써야하는건가?

	DWORD bytesReceived = 0;
	bool result = acceptExPointer(listenSocket, *mClientSocketPtr, mAcceptBuf,
		0, sizeof(sockaddr_in) + 16, sizeof(sockaddr_in) + 16, &bytesReceived, (CustomOverlapped*)&(mAcceptContext));
	//참고 : accept함수는 OVERLAPPED를 상속받지않은 구조체를 사용해도 되지만, acceptex함수는 비동기작업을 지원하기 위해 OVERLAPPED를 상속받은 구조체를 사용해야한다.

	std::cout << mID << " 클라이언트소켓 연결준비\n";
	return true;
}

void ClientDataSet::FeedBack_Accept(bool feedback)
{
	mIsConnet = feedback;

	if (feedback)
	{
		std::cout << "[Feedback] 연결완료 : " << mID << "\n";
		ReceiveReady();
	}
	else
		std::cout << "[Feedback] 연결실패 : " << mID << "\n";
}

bool ClientDataSet::ReceiveReady()
{
	if (!mIsConnet)
		return false;

	ZeroMemory(&mReceiveContext, sizeof(mReceiveContext));
	mReceiveContext.operationType = operationType::OP_RECV;
	mReceiveContext.wsabuf.buf = mReceiveBuf;
	mReceiveContext.wsabuf.len = sizeof(mReceiveBuf);
	mReceiveContext.id = mID;
	DWORD flags = 0;

	WSARecv(*mClientSocketPtr, &mReceiveContext.wsabuf, 1, nullptr, &flags, &mReceiveContext, nullptr);// 비동기 읽기 요청

	//std::cout << mID << " : 읽기준비 완료";
	return true;
}

void ClientDataSet::FeedBack_Receive(const UINT32 dataSize, char* messageBuff)
{
	//메시지버퍼링
	{
		mReceiveBuffer.grow_by(messageBuff, messageBuff + dataSize);// 수신된 데이터를 버퍼에 저장

		//버퍼에 특정크기 이상이 쌓였을때마다 방출
		while (mReceiveBuffer.size() > mBufferSize)
		{
			DataPacket* newDataPacket = new DataPacket();
			newDataPacket->ID = mID;//불필요??
			newDataPacket->buffer = new char[mBufferSize];
			std::copy_n(mReceiveBuffer.begin(), mBufferSize, newDataPacket->buffer);
			newDataPacket->dataSize = mBufferSize;
			mReceiveDataQueue.push(std::move(newDataPacket));// 수신된 데이터를 큐에 저장

			tbb::concurrent_vector<char> swapBuffers;
			swapBuffers.assign(mReceiveBuffer.begin() + mBufferSize, mReceiveBuffer.end());	// queue에 남긴 데이터를 제외시킴
			mReceiveBuffer.swap(swapBuffers);// 버퍼를 교체하여 새로운 데이터를 사용하도록 함
		}
	}

	// 수신된 메세지에 대한 처리.
	if (mReceiveDataQueue.empty())
	{
		ReceiveReady();
	}
	else
	{
		ReceiveInternalWork();
	}
}

//재귀함수
void ClientDataSet::ReceiveInternalWork()
{
	auto context = mReceiveDataQueue.Front();
	mReceiveDataQueue.pop();

	//context 처리.
	std::cout << "[서버수신] " << context->ID << "  : " << std::string(context->buffer, context->dataSize) << "\n";


	if (mReceiveDataQueue.empty())
		ReceiveReady();
	else
		ReceiveInternalWork();
}

bool ClientDataSet::SendRequest(const UINT32 dataSize, char* messageBuff)
{
	if (!mIsConnet)
		return false;

	CustomOverlapped* newSendContext = new CustomOverlapped();
	ZeroMemory(newSendContext, sizeof(CustomOverlapped));//무슨역할?
	newSendContext->operationType = operationType::OP_SEND;
	newSendContext->id = mID;
	newSendContext->wsabuf.buf = messageBuff;
	newSendContext->wsabuf.len = dataSize;
	CopyMemory(newSendContext->wsabuf.buf, messageBuff, dataSize);//무슨역할 2?

	mSendDataQueue.push(std::move(newSendContext));// 큐에 추가하여 나중에 전송할 수 있도록 함

	if (mSendDataQueue.size() == 1)
	{
		SendInternalWork();
	}

	return true;
}

void ClientDataSet::SendInternalWork()
{
	auto context = mSendDataQueue.Front();

	DWORD flags = 0;
	WSASend(*mClientSocketPtr, &mSendContext.wsabuf, 1, nullptr, flags, &(*context), nullptr);// 비동기 쓰기 요청
}


void ClientDataSet::FeedBack_Send()
{
	auto context = mSendDataQueue.Front();
	mSendDataQueue.pop();
	std::cout << "[서버발신] " << mID << "  : " << std::string(context->wsabuf.buf, context->wsabuf.len) << "\n";

	delete[] context->wsabuf.buf;//따로 해줘야하나?
	delete context;

	if (!mSendDataQueue.empty())
	{
		SendInternalWork();
	}
}