#pragma once

#include<string>
#include<memory>

#include "IOCompletionPort.h"

const UINT32 MAX_SOCK_ACCEPTBUF = 64;	// ���� ������ ũ��
const UINT32 MAX_SOCK_RECVBUF = 256;	// ���� ������ ũ��
const UINT32 MAX_SOCK_SENDBUF = 4096;	// ���� ������ ũ��

enum operationType
{
	OP_DEFAULT = 0,
	OP_ACCEPT = 1,
	OP_RECV = 2,
	OP_SEND = 3,
};

#pragma pack(push,1) // ����ü ������ 1����Ʈ�� ����
struct CustomOverlapped : public OVERLAPPED // CustomOverlapped ����ü�� OVERLAPPED ����ü�� ��ӹ���
{
    WSABUF wsabuf;               // WSABUF ����ü

    //�������.
    operationType operationType; // �۾� ����
    long id;

    CustomOverlapped() = default;
    ~CustomOverlapped() = default;

    // ���� ������
    CustomOverlapped(const CustomOverlapped& other)
        : OVERLAPPED(other),             // �θ� Ŭ����(OVERLAPPED) ����
        wsabuf(other.wsabuf),          // WSABUF ����ü ����
        operationType(other.operationType), // �۾� ���� ����
        id(other.id)                   // Ŭ���̾�Ʈ ID ����
    {
        // �߰������� ���翡 �ʿ��� �۾��� ���⿡ �ۼ� ����
    }
};

#pragma pack(pop) // ����ü ������ ������� ����

class Packet
{
public:
    long ClientID = 0;
    char* Buffer = nullptr;
    UINT32 DataSize = 0;

    // �⺻ ������
    Packet() = default;

    // ���� ������
    Packet(const Packet& other)
        : ClientID(other.ClientID), DataSize(other.DataSize)
    {
        if (other.Buffer != nullptr) {
            Buffer = new char[DataSize];
            CopyMemory(Buffer, other.Buffer, DataSize);
        }
    }

    // ���� ���� ������ (�ʿ��ϸ� �߰�)
    Packet& operator=(const Packet& other)
    {
        if (this != &other) { // �ڱ� �ڽ��� �������� �ʵ��� ����
            ClientID = other.ClientID;
            DataSize = other.DataSize;

            // ���� Buffer �޸� ����
            delete[] Buffer;

            // ���ο� Buffer �Ҵ� �� ����
            if (other.Buffer != nullptr) {
                Buffer = new char[DataSize];
                CopyMemory(Buffer, other.Buffer, DataSize);
            }
            else {
                Buffer = nullptr;
            }
        }
        return *this;
    }

    // ������ ���� �Լ�
    void Set(long clientID)
    {
        ClientID = clientID;
    }

    void Set(long clientID, char* targetBuffer, UINT32 dataSize)
    {
        ClientID = clientID;
        delete[] Buffer; // ���� �޸� ����
        DataSize = dataSize;
        Buffer = new char[dataSize];
        CopyMemory(Buffer, targetBuffer, dataSize);
    }

    // �Ҹ���
    ~Packet()
    {
        delete[] Buffer; // �迭 �޸� ����
		std::cout << "Packet destructor called!" << std::endl;
    }
};