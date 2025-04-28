#pragma once

#include<iostream>
#include<atomic>

template<typename T>
class LockFreeCircleQueue
{
private:
	constexpr static int MAX_QUEUE_SIZE = 100;
	std::atomic<int> inputIndex;
	std::atomic<int> outputIndex;
	T* buffer;
public:
	LockFreeCircleQueue<T>()
	{
		inputIndex = 0;
		outputIndex = 0;
		buffer = new T [MAX_QUEUE_SIZE];
	}

	~LockFreeCircleQueue<T>()
	{
		delete[] buffer;
	}

	//���⼭ &&�� rvalue reference�� �ǹ��Ѵ�.
	// rvalue reference�� �ӽ� ��ü�� �����ϴµ� ���Ǹ�, std::move()�� �Բ� ����Ͽ� ��ü�� �������� �̵��� �������� �Ҷ� ���ȴ�.
	//����ȭ �̵��� ���� �߿��� �������Ѵ�.
	bool push(T&& data)
	{
		int currentInputIndex = inputIndex.load(std::memory_order_acquire);
		int nextIndex = (currentInputIndex + 1) % MAX_QUEUE_SIZE;

		if (nextIndex == outputIndex.load(std::memory_order_acquire))
		{
			std::cout << "Queue is full" << std::endl;
			return false;
		}

		buffer[currentInputIndex] = std::move(data);
		inputIndex.store(nextIndex, std::memory_order_release);// ������Ʈ �� release
		return true;
	}

	T pop()
	{
		int currentOutputIndex = outputIndex.load(std::memory_order_acquire); // �ֽ� outputIndex�� �����ͼ� ���Ѵ�.

		if (currentOutputIndex == inputIndex.load(std::memory_order_acquire))//�����͸� �д� load �۾����� ����Ͽ� ���� ����ȭ.
		{
			std::cout << "Queue is empty" << std::endl;
			return T(); 
		}

		T data = std::move(buffer[currentOutputIndex]);
		outputIndex.store((currentOutputIndex + 1) % MAX_QUEUE_SIZE, std::memory_order_release);// �����͸� ������ ����� �� �ε����� ������Ʈ�� �� ���
		return data;
	}

	bool empty()
	{
		return inputIndex.load(std::memory_order_acquire) == outputIndex.load(std::memory_order_acquire);
	}

	//TODO Atomic	//size()�� capacity()�� atomic�� ������� ����. �ܼ��� �ε��� ���̷� ����ϱ� ����. ???
	int size()
	{
		auto input = inputIndex.load(std::memory_order_acquire);
		auto output = outputIndex.load(std::memory_order_acquire);

		if (input >= output)
			return input - output;
		else
			return MAX_QUEUE_SIZE - outputIndex + input;
	}

	int capacity()
	{
		return MAX_QUEUE_SIZE;
	}

	void clear()
	{
		inputIndex.store(0, std::memory_order_release);
		outputIndex.store(0, std::memory_order_release);
	}

	void print()
	{
		std::cout << "Input Index: " << inputIndex << ", Output Index: " << outputIndex << std::endl;
		for (int i = 0; i < size(); i++)
		{
			std::cout << buffer[(outputIndex + i) % MAX_QUEUE_SIZE] << " ";
		}
		std::cout << std::endl;
	}

	T Front()
	{
		return buffer[outputIndex.load(std::memory_order_acquire)];
	}
};
