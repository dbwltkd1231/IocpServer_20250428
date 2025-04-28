#pragma once

#include<string>
#include<memory>

#include "IOCompletionPort.h"

const UINT32 MAX_SOCK_ACCEPTBUF = 64;	// 소켓 버퍼의 크기
const UINT32 MAX_SOCK_RECVBUF = 256;	// 소켓 버퍼의 크기
const UINT32 MAX_SOCK_SENDBUF = 4096;	// 소켓 버퍼의 크기

enum operationType
{
	OP_DEFAULT = 0,
	OP_ACCEPT = 1,
	OP_RECV = 2,
	OP_SEND = 3,
};

#pragma pack(push,1) // 구조체 정렬을 1바이트로 설정
struct CustomOverlapped : public OVERLAPPED // CustomOverlapped 구조체는 OVERLAPPED 구조체를 상속받음
{
    WSABUF wsabuf;               // WSABUF 구조체

    //헤더파일.
    operationType operationType; // 작업 유형
    long id;

    CustomOverlapped() = default;
    ~CustomOverlapped() = default;

    // 복사 생성자
    CustomOverlapped(const CustomOverlapped& other)
        : OVERLAPPED(other),             // 부모 클래스(OVERLAPPED) 복사
        wsabuf(other.wsabuf),          // WSABUF 구조체 복사
        operationType(other.operationType), // 작업 유형 복사
        id(other.id)                   // 클라이언트 ID 복사
    {
        // 추가적으로 복사에 필요한 작업을 여기에 작성 가능
    }
};

#pragma pack(pop) // 구조체 정렬을 원래대로 복원

class Packet
{
public:
    long ClientID = 0;
    char* Buffer = nullptr;
    UINT32 DataSize = 0;

    // 기본 생성자
    Packet() = default;

    // 복사 생성자
    Packet(const Packet& other)
        : ClientID(other.ClientID), DataSize(other.DataSize)
    {
        if (other.Buffer != nullptr) {
            Buffer = new char[DataSize];
            CopyMemory(Buffer, other.Buffer, DataSize);
        }
    }

    // 복사 대입 연산자 (필요하면 추가)
    Packet& operator=(const Packet& other)
    {
        if (this != &other) { // 자기 자신을 복사하지 않도록 방지
            ClientID = other.ClientID;
            DataSize = other.DataSize;

            // 기존 Buffer 메모리 해제
            delete[] Buffer;

            // 새로운 Buffer 할당 및 복사
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

    // 데이터 설정 함수
    void Set(long clientID)
    {
        ClientID = clientID;
    }

    void Set(long clientID, char* targetBuffer, UINT32 dataSize)
    {
        ClientID = clientID;
        delete[] Buffer; // 기존 메모리 해제
        DataSize = dataSize;
        Buffer = new char[dataSize];
        CopyMemory(Buffer, targetBuffer, dataSize);
    }

    // 소멸자
    ~Packet()
    {
        delete[] Buffer; // 배열 메모리 해제
		std::cout << "Packet destructor called!" << std::endl;
    }
};