#include "pch.h"
#include "framework.h"
#include "Efimenko_lab1_Sem6.h"
#include "Efimenko_Thread_Struct.h"
#include "counter.h"
#include <thread>
#include <vector>
#include <string>

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

HANDLE hErr;
HANDLE hIn;

using namespace std;

condition_variable counter::cv_counter_completed;

// Метод для вывода на консоль, но использующий lock_guard(распространяется на промежуток от '{' и до '}'
// Чтобы серверная консолька красиво выводила текст
mutex consoleMtx;
void WriteServerConsole(const char* text)
{
	lock_guard<mutex> guard(consoleMtx);
	cout << text << endl;
}

vector<string> Split(string input, char separator)
{
	vector<string> res;
	string sSubStr = "";
	for (int i = 0; i < input.length(); i++)
	{
		if (input[i] == separator)
		{
			res.push_back(sSubStr);
			sSubStr = "";
			continue;
		}
		sSubStr += input[i];
	}

	if (sSubStr.length() > 0)
		res.push_back(sSubStr);
	return res;
}

CWinApp theApp;


struct Command
{
	enum CmdCode
	{
		StartThreads,
		StopThread,
		SendMsgToThread
	};

public:
	CmdCode code;
	vector<string> args;
};

Command ReadCmd(bool showInfo)
{
	const int len = 1023;
	char buff[len + 1];
	DWORD dwRead, dwWrite, dwe;
	BOOL bSuccess;

	while (true)
	{
		ReadFile(hIn, buff, len, &dwRead, nullptr);

		if (dwRead == 0)
			continue;

		buff[min(len, dwRead)] = 0;

		// Разбиваем пришедший через пайп буффер с командой
		vector<string> splitted = Split(buff, ':');
		// Показываем информацию о пришедшей команде на сервере
		if (showInfo && splitted.size() > 0)
		{
			string cmdName = "cmd name: " + splitted[0];
			WriteServerConsole(cmdName.c_str());
			for (int i = 1; i < splitted.size(); i++)
			{
				string cmdArg = "arg " + to_string(i) + ": " + splitted[i];
				WriteServerConsole(cmdArg.c_str());
			}
		}

		const char* cmd_name = splitted[0].c_str();

		///////////////////////////////////////////////////////
		///////     ПОЛУЧАЕМ ИНФОРМАЦИЮ О КОМАНДЕ     /////////
		Command cmd;
		if (strcmp(cmd_name, "threads_start") == 0) 
		{
			cmd.code = Command::CmdCode::StartThreads;
			WriteServerConsole("cmd type setted to StartThreads");
		}
		else if (strcmp(cmd_name, "thread_stop") == 0) 
		{
			cmd.code = Command::CmdCode::StopThread;
			WriteServerConsole("cmd type setted to StopThread");
		}
		else if (strcmp(cmd_name, "send_message") == 0) {
			cmd.code = Command::CmdCode::SendMsgToThread;
			WriteServerConsole("cmd type setted to SendMsgToThread");
		}

		for (int i = 1; i < splitted.size(); i++) // 0 индекс занимает название команды
			cmd.args.push_back(splitted[i]);
		///////////////////////////////////////////////////////
		///////////////////////////////////////////////////////

		string reqInfo = "client received request: ";
		reqInfo.append(buff);
		WriteServerConsole(reqInfo.c_str());

		return cmd;
	}
}

void start()
{
	int i = 1;
	while (true) {
		Command cmd = ReadCmd(true);
		DWORD dwWrite;

		string sActiveThreads;
		string resp;
		switch (cmd.code)
		{
		case Command::CmdCode::StartThreads:
			// cmd.args[0] - Количество потоков для запуска
			if (cmd.args.size() < 1)
			{
				string err = "[Server Error] StartThreads command need at least 1 argument.";
				WriteServerConsole(err.c_str());
				break;
			}

			// Запускаем N потоков
			Efimenko_Thread_Struct::CreateNewThreads(atoi(cmd.args[0].c_str()));
			sActiveThreads = to_string(Efimenko_Thread_Struct::GetThreadsCount());
			//

			resp = "Response to client is: " + sActiveThreads;
			WriteServerConsole(resp.c_str());

			WriteFile(hErr, sActiveThreads.c_str(), (unsigned)strlen(sActiveThreads.c_str()), &dwWrite, nullptr);
			break;

		case Command::CmdCode::StopThread:
			Efimenko_Thread_Struct::StopLastThread();

			sActiveThreads = to_string(Efimenko_Thread_Struct::GetThreadsCount());

			resp = "Response to client is: " + sActiveThreads;
			WriteServerConsole(resp.c_str());

			WriteFile(hErr, sActiveThreads.c_str(), (unsigned)strlen(sActiveThreads.c_str()), &dwWrite, nullptr);
			break;

		case Command::CmdCode::SendMsgToThread:
			// cmd.args[0] - Номер потока, который будет записывать в файл. [-1] - все потоки
			// cmd.args[1] - Текст для записи в поток(-и) 
			if (cmd.args.size() < 2)
			{
				string err = "[Server Error] SendMessage command need at least 2 arguments.";
				WriteServerConsole(err.c_str());
				WriteFile(hErr, err.c_str(), (unsigned)strlen(err.c_str()), &dwWrite, nullptr);
				break;
			}
			WriteServerConsole("cmd started execution");
			int threadId = atoi(cmd.args[0].c_str());
			if (threadId == -1) // Все потоки
			{
				WriteServerConsole("choosen all threads");
				counter::ThreadsNeedToCompleteCount = Efimenko_Thread_Struct::GetThreadsCount();
				Efimenko_Thread_Struct::SendDataToAllThreads((char*) cmd.args[1].c_str());
			}
			else
			{
				WriteServerConsole("choosen single thread");
				counter::ThreadsNeedToCompleteCount = 1;
				Efimenko_Thread_Struct::SendDataToSingleThread(threadId, (char*)cmd.args[1].c_str());
			}


			// Каждый раз, когда кто-то записывает в файл - он оповещает наш cv_counter_completed об этом
			// в свою очередь в cv_counter_completed происходим проверка counter::isAllThreadsCompleteRead
			// как только проверка вернет true, то мы перестаем ожидать оповещения объекта cv_counter_completed
			// далее нужно защититься от ложных срабатываний, повторной проверкой, 
			// а если это действительное оповещение, то ломаем цикл нафиг (break)
			while (true)
			{
				WriteServerConsole("Message from waiter for counter to be complete");
				mutex counter_mutex;
				unique_lock<mutex> lck(counter_mutex);
				counter::cv_counter_completed.wait(lck, [] {
					return counter::isAllThreadsCompleteRead();
					});

				if (counter::isAllThreadsCompleteRead() == false) // ложные срабатывания
					continue;
				else
				{
					counter::Reset();
					break;
				}
			}

			WriteFile(hErr, cmd.args[1].c_str(), (unsigned)strlen(cmd.args[1].c_str()), &dwWrite, nullptr);

			break;
		}
	}
}

int main()
{
	int nRetCode = 0;

	HMODULE hModule = ::GetModuleHandle(nullptr);

	if (hModule != nullptr)
	{
		// initialize MFC and print and error on failure
		if (!AfxWinInit(hModule, nullptr, ::GetCommandLine(), 0))
		{
			// TODO: code your application's behavior here.
			wprintf(L"Fatal Error: MFC initialization failed\n");
			nRetCode = 1;
		}
		else
		{
			hErr = GetStdHandle(STD_ERROR_HANDLE);
			hIn = GetStdHandle(STD_INPUT_HANDLE);
			Efimenko_Thread_Struct::SetupStdHandles(hIn, hErr);
			start();
		}
	}
	else
	{
		// TODO: change error code to suit your needs
		wprintf(L"Fatal Error: GetModuleHandle failed\n");
		nRetCode = 1;
	}

	return nRetCode;
}
