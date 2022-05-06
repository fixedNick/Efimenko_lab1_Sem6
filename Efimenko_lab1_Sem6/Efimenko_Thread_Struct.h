#ifndef EFIMENKO_THREAD_STRUCT
#define EFIMENKO_THREAD_STRUCT

#include "pch.h"
#include <string>
#include <vector>
#include <thread>

using namespace std;

struct Efimenko_Thread_Struct
{
private:
	static vector<Efimenko_Thread_Struct> AllThreadsStructs;
	int id;
public:
	static string TextToWrite;

	HANDLE hStopEvent;
	HANDLE hSendEvent;

	static HANDLE hIn;
	static HANDLE hErr;

	static void SetupStdHandles(HANDLE _hIn, HANDLE _hErr);
	static void CreateNewThread();
	static void CreateNewThreads(int threadsCount);
	static void StopLastThread();
	static int GetThreadsCount();
	static Efimenko_Thread_Struct GetStructById(int structId);
	static void SendDataToAllThreads(string text);
	static void SendDataToSingleThread(int idx, string text);

	Efimenko_Thread_Struct();
};

#endif