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
		//Ŭ���̾�Ʈ ������ ��Ȱ����. �������� �ʰ�����.
	}

	bool TryAccept(SOCKET& listenSocket, LPFN_ACCEPTEX& acceptExPointer);
	void FeedBack_Accept(bool feedback);
	void FeedBack_Receive(const UINT32 dataSize, char* messageBuff);
	bool SendRequest(const UINT32 dataSize, char* messageBuff);
	void FeedBack_Send();

private:
	void SendInternalWork();
	bool ReceiveReady();	//�̰� WSARecv�� �����ؼ����� 2��°�� WSARecv ��ȣ�� ���̸� ��������?
	void ReceiveInternalWork();

	LockFreeCircleQueue<DataPacket*> mReceiveDataQueue;
	LockFreeCircleQueue<CustomOverlapped*> mSendDataQueue;

	tbb::concurrent_vector<char> mReceiveBuffer;//��Ƽ������ȯ�濡�� ������ vector

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

	//��Ȱ��� ������ ���°���
	int errorCode;
	int errorCodeSize = sizeof(errorCode);
	getsockopt(*mClientSocketPtr, SOL_SOCKET, SO_ERROR, (char*)&errorCode, &errorCodeSize);
	if (errorCode != 0) {
		std::cerr << "Socket error detected: " << errorCode << std::endl;
		return false;
	}

	// ��Ƽ�÷��� ��� �� select, poll�� ��� ���ϼ� ������ �־�(windows �⺻�� 64��), ���� Ŭ���̾�Ʈ ������ ó���ϱ� �����.
	// AcceptEx�� IOCP�� �Բ� ����ϸ�, ��Ը� ���ÿ����� ȿ�������� ó���Ҽ��ִ�.

	ZeroMemory(&mAcceptContext, sizeof(mAcceptContext));
	mAcceptContext.operationType = operationType::OP_ACCEPT;
	mAcceptContext.id = mID;
	mAcceptContext.wsabuf.buf = mAcceptBuf;          // ���� �ּ� ����
	mAcceptContext.wsabuf.len = sizeof(mAcceptBuf);  // ���� ũ�� ���� -> �̷������θ� ����ϴ°ǰ�?

	DWORD bytesReceived = 0;
	bool result = acceptExPointer(listenSocket, *mClientSocketPtr, mAcceptBuf,
		0, sizeof(sockaddr_in) + 16, sizeof(sockaddr_in) + 16, &bytesReceived, (CustomOverlapped*)&(mAcceptContext));
	//���� : accept�Լ��� OVERLAPPED�� ��ӹ������� ����ü�� ����ص� ������, acceptex�Լ��� �񵿱��۾��� �����ϱ� ���� OVERLAPPED�� ��ӹ��� ����ü�� ����ؾ��Ѵ�.

	std::cout << mID << " Ŭ���̾�Ʈ���� �����غ�\n";
	return true;
}

void ClientDataSet::FeedBack_Accept(bool feedback)
{
	mIsConnet = feedback;

	if (feedback)
	{
		std::cout << "[Feedback] ����Ϸ� : " << mID << "\n";
		ReceiveReady();
	}
	else
		std::cout << "[Feedback] ������� : " << mID << "\n";
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

	WSARecv(*mClientSocketPtr, &mReceiveContext.wsabuf, 1, nullptr, &flags, &mReceiveContext, nullptr);// �񵿱� �б� ��û

	//std::cout << mID << " : �б��غ� �Ϸ�";
	return true;
}

void ClientDataSet::FeedBack_Receive(const UINT32 dataSize, char* messageBuff)
{
	//�޽������۸�
	{
		mReceiveBuffer.grow_by(messageBuff, messageBuff + dataSize);// ���ŵ� �����͸� ���ۿ� ����

		//���ۿ� Ư��ũ�� �̻��� �׿��������� ����
		while (mReceiveBuffer.size() > mBufferSize)
		{
			DataPacket* newDataPacket = new DataPacket();
			newDataPacket->ID = mID;//���ʿ�??
			newDataPacket->buffer = new char[mBufferSize];
			std::copy_n(mReceiveBuffer.begin(), mBufferSize, newDataPacket->buffer);
			newDataPacket->dataSize = mBufferSize;
			mReceiveDataQueue.push(std::move(newDataPacket));// ���ŵ� �����͸� ť�� ����

			tbb::concurrent_vector<char> swapBuffers;
			swapBuffers.assign(mReceiveBuffer.begin() + mBufferSize, mReceiveBuffer.end());	// queue�� ���� �����͸� ���ܽ�Ŵ
			mReceiveBuffer.swap(swapBuffers);// ���۸� ��ü�Ͽ� ���ο� �����͸� ����ϵ��� ��
		}
	}

	// ���ŵ� �޼����� ���� ó��.
	if (mReceiveDataQueue.empty())
	{
		ReceiveReady();
	}
	else
	{
		ReceiveInternalWork();
	}
}

//����Լ�
void ClientDataSet::ReceiveInternalWork()
{
	auto context = mReceiveDataQueue.Front();
	mReceiveDataQueue.pop();

	//context ó��.
	std::cout << "[��������] " << context->ID << "  : " << std::string(context->buffer, context->dataSize) << "\n";


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
	ZeroMemory(newSendContext, sizeof(CustomOverlapped));//��������?
	newSendContext->operationType = operationType::OP_SEND;
	newSendContext->id = mID;
	newSendContext->wsabuf.buf = messageBuff;
	newSendContext->wsabuf.len = dataSize;
	CopyMemory(newSendContext->wsabuf.buf, messageBuff, dataSize);//�������� 2?

	mSendDataQueue.push(std::move(newSendContext));// ť�� �߰��Ͽ� ���߿� ������ �� �ֵ��� ��

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
	WSASend(*mClientSocketPtr, &mSendContext.wsabuf, 1, nullptr, flags, &(*context), nullptr);// �񵿱� ���� ��û
}


void ClientDataSet::FeedBack_Send()
{
	auto context = mSendDataQueue.Front();
	mSendDataQueue.pop();
	std::cout << "[�����߽�] " << mID << "  : " << std::string(context->wsabuf.buf, context->wsabuf.len) << "\n";

	delete[] context->wsabuf.buf;//���� ������ϳ�?
	delete context;

	if (!mSendDataQueue.empty())
	{
		SendInternalWork();
	}
}