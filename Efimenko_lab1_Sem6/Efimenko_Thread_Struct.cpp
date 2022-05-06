#include "pch.h"
#include "counter.h"
#include "Efimenko_Thread_Struct.h"
#include "Efimenko_lab1_Sem6.h"
#include "Efimenko_MMF_Data.h"
#include <mutex>
#include <string>

extern "C" _declspec(dllimport) void __cdecl WriteToFile(int thread_idx, char* str);

using namespace std;

vector<Efimenko_Thread_Struct> Efimenko_Thread_Struct::AllThreadsStructs;
HANDLE Efimenko_Thread_Struct::hIn;
HANDLE Efimenko_Thread_Struct::hErr;
string Efimenko_Thread_Struct::TextToWrite;

void ThreadedMethod(int structIdx)
{
	bool isStartedNotificationShowed = false;
	int threadId = int(structIdx) + 1;
	int structId = int(structIdx);

	string sOut = "Thread #" + to_string(threadId) + " started!\n\r";
	WriteServerConsole(sOut.c_str());
	Efimenko_Thread_Struct eStruct = Efimenko_Thread_Struct::GetStructById(structId);

	HANDLE arrHandles[] = { eStruct.hStopEvent, eStruct.hSendEvent };

	while (true)
	{
		int e = WaitForMultipleObjects(2, arrHandles, FALSE, INFINITE) - WAIT_OBJECT_0;
		string resp;
		switch (e)
		{
		case 0:
			//stop
			resp = "Thread # " + to_string(threadId) + " stopped";
			WriteServerConsole(resp.c_str());
			return;

		case 1:
			// send
			int size = Efimenko_Thread_Struct::TextToWrite.length() + 1;
			char* fileData = new char[size];
			memcpy(fileData, Efimenko_Thread_Struct::TextToWrite.c_str(), size);
			WriteToFile(threadId, fileData);
			resp = "Thread # " + to_string(threadId) + " written text";
			WriteServerConsole(resp.c_str());
			counter::ThreadsAlreadyCompletedCount++;
			counter::cv_counter_completed.notify_all();

			break;
		}
	}
	return;
}

Efimenko_Thread_Struct Efimenko_Thread_Struct::GetStructById(int structId)
{
	return AllThreadsStructs[structId];
}

int Efimenko_Thread_Struct::GetThreadsCount()
{
	return AllThreadsStructs.size();
}

void Efimenko_Thread_Struct::CreateNewThreads(int threadsCount)
{
	for (int i = 0; i < threadsCount; i++)
		Efimenko_Thread_Struct::CreateNewThread();
}

void Efimenko_Thread_Struct::SetupStdHandles(HANDLE _hIn, HANDLE _hErr)
{
	Efimenko_Thread_Struct::hIn = _hIn;
	Efimenko_Thread_Struct::hErr = _hErr;
	
}

void Efimenko_Thread_Struct::CreateNewThread()
{
	int currSize = int(Efimenko_Thread_Struct::GetThreadsCount());

	Efimenko_Thread_Struct newStruct;
	newStruct.id = currSize;
	newStruct.hSendEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
	newStruct.hStopEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
	thread th = thread(ThreadedMethod, newStruct.id);
	th.detach();
	AllThreadsStructs.push_back(newStruct);
}

void Efimenko_Thread_Struct::StopLastThread()
{
	SetEvent(AllThreadsStructs[AllThreadsStructs.size() - 1].hStopEvent);
	AllThreadsStructs.pop_back();
}

Efimenko_Thread_Struct::Efimenko_Thread_Struct() {}

void Efimenko_Thread_Struct::SendDataToAllThreads(string text)
{
	for (auto& eStruct : AllThreadsStructs)
	{
		Efimenko_Thread_Struct::TextToWrite = text;
		SetEvent(eStruct.hSendEvent);
	}
}

void Efimenko_Thread_Struct::SendDataToSingleThread(int idx, string text)
{
	Efimenko_Thread_Struct eStruct = GetStructById(idx);
	Efimenko_Thread_Struct::TextToWrite = text;
	SetEvent(eStruct.hSendEvent);
}