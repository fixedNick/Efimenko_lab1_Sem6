// dllmain.cpp : Defines the entry point for the DLL application.
#include "pch.h"
#include <fstream>
#include <string>
#include <vector>
#include <mutex>
#include <atlconv.h> // bstr

PROCESS_INFORMATION hServerProcessInformation;

// Потоки направленные в сторону сервера(консольки)
HANDLE hStdToServer_In, hStdToServer_Out;
// Потоки направленные от сервера к клиенту(ответы/подтверждения действий)
HANDLE hStdFromServer_In, hStdFromServer_Out;

#pragma region dllMain


BOOL APIENTRY DllMain(HMODULE hModule,
	DWORD  ul_reason_for_call,
	LPVOID lpReserved
)
{
	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
	case DLL_THREAD_ATTACH:
	case DLL_THREAD_DETACH:
	case DLL_PROCESS_DETACH:
		break;
	}
	return TRUE;
}
#pragma endregion

struct ServerResponse
{
public:
	char* text;
	int length;
};

using namespace std;

BSTR ConvertStringToBStr(char* str)
{
	int wslen = MultiByteToWideChar(CP_ACP, 0, str, strlen(str), 0, 0);
	BSTR bstr = SysAllocStringLen(0, wslen);
	MultiByteToWideChar(CP_ACP, 0, str, strlen(str), bstr, wslen);
	// Use bstr here
	SysFreeString(bstr);
	return bstr;
}

void StopServer()
{
	CloseHandle(hServerProcessInformation.hProcess);
	CloseHandle(hServerProcessInformation.hThread);

	const HANDLE server = OpenProcess(PROCESS_TERMINATE, FALSE, hServerProcessInformation.dwProcessId);
	TerminateProcess(server, 1);
	CloseHandle(server);
}

bool IsServerProcessAlive()
{
	HANDLE process = OpenProcess(SYNCHRONIZE, FALSE, GetProcessId(hServerProcessInformation.hProcess));
	DWORD ret = WaitForSingleObject(process, 0);
	CloseHandle(process);
	return ret == WAIT_TIMEOUT;
}
// Метод будет писать и сразу же ждать ответа
// Боюсь, что без использования событий/mmf или других IPC 
// нам это общение не организовать
char* WriteToServer(char* text, int textSize)
{
	const int len = 1023;
	DWORD dwRead, dwWrite;
	bool bSuccess = false;

	for (;;) {
		bSuccess = WriteFile(hStdToServer_Out, text, textSize, &dwWrite, nullptr);
		if (!bSuccess || dwWrite == textSize)
			break;
	}

	char buff[len + 1];

	for (;;)
	{
		bSuccess = ReadFile(hStdFromServer_In, buff, len, &dwRead, nullptr);

		if (bSuccess)
		{
			buff[min(len, dwRead)] = 0;
			break;
		}
	}

	return buff;
}



extern "C"
{
	// В качестве возвращаемого значения необходим BStr, 
	// для этого используем самописный метод конвертации из char* в bstr
	// метод: ConvertStringToBStr(char* str)
	_declspec(dllexport) void* _stdcall SendTextToThreadViaAnonPipes(char* fileText, int tidx)
	{
		// Проверим, жив ли сервер
		if (!IsServerProcessAlive())
			return ConvertStringToBStr((char*)"Server isn't alive, start it first.");

		// Формируем команду для сервера, по придуманному паттерну
		// Общий Паттерн: <команда>:<аргумент1>:<аргументN>
		// Паттер для данной команды: <команда>:<номер потока>:<текст для записи>
		string request = "send_message:" + to_string(tidx) + ":" + fileText;

		// Отправим команду
		char* response = WriteToServer((char*)request.c_str(), strlen((char*)request.c_str()));

		// Возвращаем клиенту конвертированный в BStr ответ.
		return ConvertStringToBStr(response);
	}
	// todo: переписать на return bool, а количество потоков получать в out аргументе
	_declspec(dllexport) int _stdcall Start(int threadsCount)
	{
		// Если главный поток(сервер/консолька) не запущены - запускаем
		// В таком случае метод вернет в качестве ответа 0
		if (!IsServerProcessAlive()) {
			// Прокладываем пайпы в обе стороны
			SECURITY_ATTRIBUTES sa{ sizeof(SECURITY_ATTRIBUTES), NULL, TRUE };
			if (!CreatePipe(&hStdToServer_In, &hStdToServer_Out, &sa, 0))
				return -1;

			if (!SetHandleInformation(hStdToServer_Out, HANDLE_FLAG_INHERIT, 0))
				return -1;

			if (!CreatePipe(&hStdFromServer_In, &hStdFromServer_Out, &sa, 0))
				return -1;

			if (!SetHandleInformation(hStdFromServer_In, HANDLE_FLAG_INHERIT, 0))
				return -1;

			STARTUPINFO si;
			ZeroMemory(&si, sizeof(STARTUPINFO));
			ZeroMemory(&hServerProcessInformation, sizeof(PROCESS_INFORMATION));

			si.cb = sizeof(si);
			si.hStdError = hStdFromServer_Out;
			si.hStdInput = hStdToServer_In;
			si.dwFlags |= STARTF_USESTDHANDLES;

			CreateProcess(
				NULL,
				(LPSTR)"C:\\Users\\kkos4\\source\\AnonPipes_Events\\x64\\Debug\\Efimenko_lab1_Sem6.exe",
				&sa,
				NULL,
				TRUE,
				CREATE_NEW_CONSOLE,
				NULL,
				NULL,
				&si,
				&hServerProcessInformation);


			// Закрываем хэндлы, которые нужны дочернему процессу
			CloseHandle(hStdFromServer_Out);
			CloseHandle(hStdToServer_In);

			return 0;
		}

		string req = "threads_start:" + to_string(threadsCount);

		// Запускаем N потоков
		char* resp = WriteToServer((char*)req.c_str(), req.length());
		int activeThreadsCount = atoi(resp);

		return activeThreadsCount;
	}

	_declspec(dllexport) int _stdcall Stop(bool stopServer)
	{
		// Если команда на остановку сервера
		if (stopServer)
		{
			StopServer();
			return -1;
		}

		// Отправляет запрос в сервер, чтобы узнать сколько запущено потоков
		// Если потоков 0, то вырубает процесс
		// Если потоков больше 0, то выключает поток
		// Возвращаем количество активных потоков
		// Возвращает -1, если завершается главный поток[он же процесс](сервер/консолька)

		// TODO: Если stopServer:true - остановить все потоки, а после выключить сервер.
		if (IsServerProcessAlive() == false)
			return -1;

		char* resp = WriteToServer((char*)"thread_stop", strlen("thread_stop"));
		int activeThreadsCount = atoi(resp);

		if (activeThreadsCount == -1)
		{
			// Не было запущено ни одного thread'a
			// А значит следует выключить сервер(консоль)
			// -1 означает, что активных thread'ов нет
			// вырубаем сервер
			StopServer();
			return -1;
		} // Иначе возвращаем количество активных потоков [0....9999999(бесконечность)]
		else return activeThreadsCount;

	}
	std::mutex mtx;

	_declspec(dllexport) void __cdecl WriteToFile(int thread_idx, char* str)
	{
		mtx.lock();
		std::ofstream vmdelet_out;                    //создаем поток 
		vmdelet_out.open(std::to_string(thread_idx) + ".txt", std::ios::app);  // открываем файл для записи в конец
		vmdelet_out << str;                        // сама запись
		vmdelet_out.close();                          // закрываем файл
		mtx.unlock();
	}
}