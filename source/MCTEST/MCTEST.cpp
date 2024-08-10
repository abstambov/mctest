// MentalComputationCards.cpp: ���������� ����� ����� ��� ����������� ����������.
//

#include <tchar.h>
#include <iostream>
#include <iomanip>
#include <random>
#include <chrono>
#include <climits>
#include <vector>
#include <string>
#include <windows.h>
//#include <sapi.h>

using namespace std;

// #define MC_DEBUG

//
// ���������, ������������ ��������� (�����������) ���������
//
#define MC_MIN				1	// ���������� ���������� �������� ��� ��������
#define MC_MAX_SUMMAND		99	// ������������ ���������
#define MC_MAX_SUBTRAHEND	99	// ������������ ���������� (������ ���� ������ �� ������ ������������)
#define MC_MAX_FACTOR1		99	// ������������ ���������
#define MC_MAX_FACTOR2		9	// ������������ ���������
#define MC_MAX_DIVISOR		9	// ������������ ��������
#define MC_MAX_QUOTIENT		99	// ������������ ������� (������ ���� �� ������ ������������� ����������)
#define MC_MAX_RESULT		999	// ������������ ���������

#ifndef MC_DEBUG
#define MC_NUMBER_OF_EXPRESSIONS			50										// ����� ��������� (�����) � �����
#endif // !MC_DEBUG
#ifdef MC_DEBUG
#define MC_NUMBER_OF_EXPRESSIONS			3										// ����� ��������� (�����) � �����
#endif // !MC_DEBUG
#define MC_SECONDS_FOR_TEST					600										// ����� ����� ������������
#define MC_BLINK_INTERVAL_BEFORE_TEST_END	10										// ����� ������� ����� ������ ����� (�������)
#define MC_BLINK_FREQUENCY					500										// ������� ������� � �������������
#define MC_BLINK_COLOR						FOREGROUND_RED | FOREGROUND_INTENSITY	// ���� ��� �������
#define MC_BLINK_TIME						60										// �������� ������ ������ �������-�� ������
#define MC_BLINK_INTERVAL					3										// � ������ � ������� ��������-�� ������

//
// ������������ ���� ������
//
struct _LEXEME
{
	int left_operand, right_operand, result;
	__wchar_t operation;
};

using LEXEME = _LEXEME;
using PLEXEME = _LEXEME *;

using EXPRESSION = vector < LEXEME > ;
using PEXPRESSION = EXPRESSION *;

enum COMPLEXITY { MC_ONE, MC_TWO, MC_THREE, MC_FOUR };	// ��������� ������������� ���������. ��������� ������������ ����������� ��������.

struct _TEST_LOG_ITEM
{
	wstring	expression;					// ��������� � ���������� ����
	int complexity;						// ��������� ��������� (���������� ��������)
	decltype(_LEXEME::result) result;	// ��������� ���������
	int attempt;						// ����� ����� ������� ����� ����������
	decltype(difftime(0, 0)) seconds;	// ������� (� ������ �����), �� ������� ���� ������ ������
};

using TEST_LOG_ITEM = _TEST_LOG_ITEM;
using PTEST_LOG_ITEM = _TEST_LOG_ITEM *;

using TEST_LOG = vector < TEST_LOG_ITEM > ;
using PTEST_LOG = TEST_LOG *;

#define MC_ADD			L'+'
#define MC_SUB			L'-'
#define MC_MUL			L'x'
#define MC_DIV			L':'
#define MC_SEE_ABOVE				INT_MIN		// ��� _LEXEME::left_operand � _LEXEME::right_operand - ���� ��� ����� ����� ��������, �� �������� �������� ���� ����� �� ���������� �������
#define MC_NO_RESULT				INT_MIN		// ��������, �������� ��������� �� ����� ���� ����� �������
#define MC_SIZE_OF_BUF_FOR_TIME		36			// ������ ������ ��� �������������� ������� � ������
#define MC_FILENAME					L"MC_"		// ������� ����� � �����������

//
// ���������� ����������
//
default_random_engine generator(int(time(nullptr)) - 1421077640);
HANDLE hStdOutDup;							// �������� �������� ������������ ����������� SDTOUT. �������� �� ������� ���������� ��-�� ������������� ������������� � ����������� ������� Ctrl+C � �.�. �������
HANDLE hNewScreenBuffer;					// �������� ���������� ������ ��������� ������ . �������� �� ������� ���������� ��-�� ������������� ������������� � ����������� ������� Ctrl+C � �.�. �������
CONSOLE_SCREEN_BUFFER_INFOEX save_infoex;	// �������� ����� �������� ��������� ������, � �������� ����������� ���������. �������� �� ������� ���������� ��-�� ������������� ������������� � ����������� ������� Ctrl+C � �.�. �������
bool is_new_screen_buffer_active = false;	// == true, ���� ������� ����� �������� �����
HANDLE hTimer = nullptr;					// ���������� �������. �������� �� ������� ����������, ����� ����� ���� ������������ ������� �������.
HANDLE hTimerQueue = nullptr;				// ���������� ������� �������. �������� �� ������� ����������, ����� ����� ���� ������������ ������� �������.
time_t start_time = 0;						// ����� ������ �����. �������� �� ������� ���������� �� ������� ������������� � ������� �������.

//
// ��������� ������������ �������
//
int ThrowCoin();
int RollDice();
int GetRandomValue(int max_value);
inline void AddSpaceToStringW(wstring &str1, wstring &str2);
wstring& GetExpressionString1(const EXPRESSION &exp);
void GetExpressionString3(const EXPRESSION &exp, wstring str[3], bool &is_three_string);
decltype(_LEXEME::result) GetExpressionResult(const EXPRESSION &exp);
void GenerateExpression(const enum COMPLEXITY choice, EXPRESSION &exp);
inline int ReOpenStdout();
BOOL CtrlHandler(DWORD fdwCtrlType);
bool GetScreenshot(LPTSTR pszFile);
bool CreateBMPFile(HWND hwnd, LPTSTR pszFile, PBITMAPINFO pbi, HBITMAP hBMP, HDC hDC);
PBITMAPINFO CreateBitmapInfoStruct(HWND hwnd, HBITMAP hBmp);
VOID CALLBACK TimerRoutine(PVOID lpParam, BOOLEAN TimerOrWaitFired);
int TasksPerAllottedTime(TEST_LOG &log);
int OperationsPerAllottedTime(TEST_LOG &log);
int TotalOperations(TEST_LOG &log);
int NumberOfMaxHits(TEST_LOG &log);
int NumberOfTasksWithOneAttempt(TEST_LOG &log);

int _tmain(int argc, _TCHAR* argv[])
{
	HANDLE hStdOut;
	EXPRESSION expression;
	bool is_three_string;
	__wchar_t filename_with_result[MAX_PATH];
	TEST_LOG_ITEM log_item;
	TEST_LOG log;
	time_t current_time;
	tm temp_time;
	wstring temp_strings[3];
	int temp_integer;

	setlocale(LC_ALL, "rus");

	SetConsoleTitle(TEXT("Mental computation test (MCTEST)"));
	wcout << L"Mental computation test, version 1.1" << endl << L"Copyright (c) 2015 Sergey Vasilyev Sr. aka ABS" << endl << endl;
	GenerateExpression(static_cast<COMPLEXITY>(RollDice()), expression);
	wcout << L"The test consists of " << MC_NUMBER_OF_EXPRESSIONS << L" simple tasks (such as \"" << GetExpressionString1(expression) << L"\"). Test lasts for " << MC_SECONDS_FOR_TEST << " seconds." << endl << endl;

	// ��������� ��� ����� ��� ��������� ����������
	if (argc != 2 || !GetTempFileName(argv[1], MC_FILENAME, 0, filename_with_result))
	{
		wcout << L"Usage: " << argv[0] << L" [drive:][path]" << endl;
		wcout << L"Example: " << argv[0] << L" D:\\" << endl;
		return 0;
	}

	// ������ ����� �������� ����� � ������������� �� ����
	hStdOut = GetStdHandle(STD_OUTPUT_HANDLE);
	if (hStdOut == INVALID_HANDLE_VALUE)								// �������� ���������� �������� ��������� ������
		return GetLastError();
	if (!DuplicateHandle(GetCurrentProcess(),							// ... � ��������� ���
		hStdOut,
		GetCurrentProcess(),
		&hStdOutDup,
		0,
		FALSE,
		DUPLICATE_SAME_ACCESS))
		return GetLastError();
	hNewScreenBuffer = CreateConsoleScreenBuffer(						// ������ ����� �������� �����
		GENERIC_READ |
		GENERIC_WRITE,		     // read/write access 
		FILE_SHARE_READ |
		FILE_SHARE_WRITE,        // shared 
		NULL,                    // default security attributes 
		CONSOLE_TEXTMODE_BUFFER, // must be TEXTMODE 
		NULL);                   // reserved; must be NULL 
	if (hNewScreenBuffer == INVALID_HANDLE_VALUE)
		return GetLastError();
	save_infoex.cbSize = sizeof(save_infoex);
	if (!GetConsoleScreenBufferInfoEx(hStdOut, &save_infoex))			// �������� ��������� �������� ��������� ������
		return GetLastError();
	if (!SetConsoleScreenBufferInfoEx(hNewScreenBuffer, &save_infoex))	// ... � ��������� �� � ������ ��������� ������
		return GetLastError();

	wcout << L"Press any key to start test..." << endl << endl;
	_gettch();

	if (!SetConsoleActiveScreenBuffer(hNewScreenBuffer))				// ������ �������� ����� �������� �����
		return GetLastError();
	if (!ReOpenStdout())												// ������������� stdout (���������� hStdOut ����� ����� "��������", ����� ����� ������������ ��� ����� hStdOutDup)
		return -1;
	is_new_screen_buffer_active = true;

	// ������������� ���� ���������� Ctrl+C � �.�. �������
	if (!SetConsoleCtrlHandler((PHANDLER_ROUTINE)CtrlHandler, TRUE))
		return GetLastError();

	// ���������� ����� ������ ����� � ������� ���
	current_time = start_time = chrono::system_clock::to_time_t(chrono::system_clock::now());
	localtime_s(&temp_time, &start_time);
	cout << "Start at: " << put_time(&temp_time, "%H:%M:%S") << endl << endl;

	// ������������� ������
	hTimerQueue = CreateTimerQueue();
	if (!hTimerQueue)
		return -1;
	if (!CreateTimerQueueTimer(&hTimer, hTimerQueue, (WAITORTIMERCALLBACK)TimerRoutine, nullptr, 0, MC_BLINK_FREQUENCY, WT_EXECUTEINTIMERTHREAD))
		return -1;

	// --------------------------------------------------------------------------------------------
	// ��� ����
	// --------------------------------------------------------------------------------------------
	_flushall();
	for (auto i = 1; i <= MC_NUMBER_OF_EXPRESSIONS; i++)
	{
		// ���������� ��������� �� ��������� ����������, �������� �������� ���
		log_item.complexity = RollDice();
		GenerateExpression(static_cast<COMPLEXITY>(log_item.complexity), expression);
		log_item.expression = GetExpressionString1(expression);
		log_item.result = GetExpressionResult(expression);

		// ������� �����
		current_time = chrono::system_clock::to_time_t(chrono::system_clock::now());
		localtime_s(&temp_time, &current_time);
		wcout << setfill(L'.') << setw(25) << setiosflags(ios::right) << L" " << resetiosflags(ios::right) << i << L" (";
		cout << put_time(&temp_time, "%H:%M:%S");
		wcout << L", +";
		current_time = static_cast<time_t>(difftime(current_time, start_time));
		gmtime_s(&temp_time, &current_time);
		cout << put_time(&temp_time, "%H:%M:%S");
		wcout << L" since start)" << endl << endl;

		// ������� ���������
		GetExpressionString3(expression, temp_strings, is_three_string);
		wcout << temp_strings[0] << endl;
		if (is_three_string)
		{
			wcout << temp_strings[1] << endl;
			wcout << temp_strings[2] << endl;
		}

#ifdef MC_DEBUG
		wcout << L"= " << log_item.result << endl;
#endif // !MC_DEBUG

		// ����, ���� �� ����� ����� ���������� �����
		for (temp_integer = MC_NO_RESULT, log_item.attempt = 0; temp_integer != log_item.result; log_item.attempt++)
		{
			wcout << endl;
			if (log_item.attempt)
			{
				wcout << L"\aWRONG!" << endl;
				wcout << L"(" << log_item.attempt + 1 << L") ";
			}
			wcout << L">";
			cin >> temp_integer;
		}
		current_time = chrono::system_clock::to_time_t(chrono::system_clock::now());
		log_item.seconds = difftime(current_time, start_time);

		// ������� �����
		wcout << endl << L"Elapsed time: " << log_item.seconds << L" seconds" << endl;
		if (log_item.attempt > 1)
			wcout << log_item.attempt << L" attempts" << endl;
		wcout << endl << endl;

		// ��������� ���������
		log.push_back(log_item);
	}

	// ��������������� ������� �������
	if (!DeleteTimerQueue(hTimerQueue))
		return -1;
	hTimer = nullptr;
	hTimerQueue = nullptr;

	// ��������������� ������ ScreenBuffer
	if (!SetConsoleActiveScreenBuffer(hStdOutDup))						// ������ �������� ������ �������� �����, ��������� ��� ����� ����� ���������� ��������
		return GetLastError();
	if (!ReOpenStdout())												// ������������� stdout - ��������� ��� ����������������� ��������������� ������� ������ (cout, wcout � �.�.)
		return -1;
	if (!CloseHandle(hNewScreenBuffer))
		return -1;

	// ������� �������� ���������
	wcout << setfill(L'-') << setw(25) << setiosflags(ios::right) << L" " << resetiosflags(ios::right) << endl << endl;
	wcout << L"Number of tasks: " << MC_NUMBER_OF_EXPRESSIONS << endl;
	wcout << L"Maximum allotted time: " << MC_SECONDS_FOR_TEST << L" seconds" << endl;
	wcout << endl;
	wcout << L"Test start at ";
	localtime_s(&temp_time, &start_time);
	cout << put_time(&temp_time, "%c");
	wcout << endl << L"Test end at ";
	localtime_s(&temp_time, &current_time);
	cout << put_time(&temp_time, "%c");
	wcout << endl << endl;
	temp_integer = TasksPerAllottedTime(log);
	wcout << setiosflags(ios::fixed) << setprecision(2);
	wcout << L"Tasks comleted per allotted time: " << temp_integer << L" (" << static_cast<double>(temp_integer)* 100 / MC_NUMBER_OF_EXPRESSIONS << L"%)" << endl;
	temp_integer = OperationsPerAllottedTime(log);
	wcout << L"Number of operations in allotted time: " << temp_integer << L" (" << static_cast<double>(temp_integer)* 100 / TotalOperations(log) << L"%)" << endl;
	wcout << endl;
	wcout << L"Total time: ";
	current_time = static_cast<time_t>(difftime(current_time, start_time));
	gmtime_s(&temp_time, &current_time);
	cout << put_time(&temp_time, "%H:%M:%S");
	wcout << " (" << static_cast<double>(current_time) / MC_NUMBER_OF_EXPRESSIONS << L" seconds per task)" << endl;
	wcout << L"Total number of operations: " << TotalOperations(log) << endl;
	wcout << endl;
	wcout << L"Number of hits: " << NumberOfMaxHits(log) << endl;
	temp_integer = NumberOfTasksWithOneAttempt(log);
	wcout << L"Number of tasks with one attempt: " << temp_integer << L" (" << static_cast<double>(temp_integer)* 100 / MC_NUMBER_OF_EXPRESSIONS << L"%)" << endl;
	wcout << endl << setfill(L'-') << setw(25) << setiosflags(ios::right) << L" " << resetiosflags(ios::right) << endl;

	/*
	ISpVoice * pVoice = NULL;
	if (FAILED(::CoInitialize(NULL)))
	return FALSE;
	HRESULT hr = CoCreateInstance(CLSID_SpVoice, NULL, CLSCTX_ALL, IID_ISpVoice, (void **)&pVoice);
	if (SUCCEEDED(hr))
	{
	hr = pVoice->Speak(L"�����", SPF_DEFAULT, NULL);
	// Change pitch hr = pVoice->Speak(L"This sounds normal <pitch middle = '-10'/> but the pitch drops half way through", SPF_IS_XML, NULL );
	pVoice->Release();
	pVoice = NULL;
	}
	::CoUninitialize();
	*/

	// ������ �������� ������ � ��������������� ������������ ���� � .BMP
	if (!GetScreenshot(filename_with_result))
		return -1;
	current_time = chrono::system_clock::to_time_t(chrono::system_clock::now());
	localtime_s(&temp_time, &current_time);
	temp_strings[0] = temp_strings[1] = filename_with_result;
	wcsftime(filename_with_result, MAX_PATH, L"%Y%m%d_%H%M%S test result.bmp", &temp_time);
	temp_strings[0] = temp_strings[0].substr(0, temp_strings[0].find_last_of(L'\\')) + L"\\" + MC_FILENAME + filename_with_result;
	if (!_wrename(temp_strings[1].c_str(), temp_strings[0].c_str()))
		return -1;

	wcout << L"See " << temp_strings[0] << L" for test results." << endl;
	wcout << L"Good luck!" << endl;

	return 0;
}

//
// ���������� ��������� ����� ����� �� 0 (false) �� 1 (true)
//
int ThrowCoin()
{
	static uniform_int_distribution<int> distribution(0, 1);
	return distribution(generator);
}

//
// ���������� ��������� ����� ����� �� 0 �� 3
//
int RollDice()
{
	static uniform_int_distribution<int> distribution(0, 3);
	return distribution(generator);

}

//
// ���������� ��������� ����� ����� �� MC_MIN �� max_value
//
int GetRandomValue(int max_value)
{
	uniform_int_distribution<int> distribution(MC_MIN, max_value);
	return distribution(generator);
}

//
// ����������� ����� �����, �������� � ������ � ���������� ������ ������� (� ���� ������)
//
inline void AddSpaceToStringW(wstring &str1, wstring &str2)
{
	auto max_lenght = str1.size();
	wstring *pstr = &str2;

	if (str2.size() > max_lenght)
	{
		max_lenght = str2.size();
		pstr = &str1;
	}

	for (bool i = true; (*pstr).size() != max_lenght; i = !i)
	{
		if (i)
			*pstr = L' ' + *pstr;
		else
			*pstr = *pstr + L' ';
	}

	return;
}

//
// ���������� ������ �� ���������� ������������� ���������.
// ������� ������������ ������ MC_DIV (L':')
//
wstring& GetExpressionString1(const EXPRESSION &exp)
{
	static wstring str;
	wstring operand;
	bool need_brackets = false;

	for (auto i = exp.begin(); i != exp.end(); i++)
	{
		// ��������� �� ������ �������, ������� ����������� �� ��������
		operand.clear();
		if (need_brackets)
			operand = L"( ";
		operand += str;
		if (need_brackets)
		{
			operand += L" )";
			need_brackets = false;
		}

		// ��������� ������ ��������, ��������� ������ ��������
		str.clear();
		if ((*i).left_operand == MC_SEE_ABOVE)
			str += operand;
		else
			str += to_wstring((*i).left_operand);
		str.append({ L' ' });
		str.append({ (*i).operation });
		str.append({ L' ' });
		if ((*i).right_operand == MC_SEE_ABOVE)
			str += operand;
		else
			str += to_wstring((*i).right_operand);

		// ���� ���� ��������� ��� �������, �� ����� ����� ������
		if ((*i).operation == MC_ADD || (*i).operation == MC_SUB)
			need_brackets = true;
	}

	return str;
}

//
// ���������� ���������� ������������� ���������.
// ���� ��� �������, �� ��������� ����������� ������ � str[0] (str[1] � str[2] �������� ������ ������ � ������ 0), ���� is_three_string ��� ���� ����� false
// ���� ������� ����, �� ��������� ������������ � str[0], str[1] � str[3], ���� is_three_string ��� ���� ����� true
//
void GetExpressionString3(const EXPRESSION &exp, wstring str[3], bool &is_three_string)
{
	wstring operand[3];
	bool need_brackets = false;
	int tmp1, tmp2, tmp3;

	// ������� ������ � ���������� ���� � false
	str[0].clear();
	str[1].clear();
	str[2].clear();
	is_three_string = false;

	//
	for (auto i = exp.begin(); i != exp.end(); i++)
	{
		// ��������� �� ���������� ������ �������, ������� ����������� �� ��������
		operand[0].clear();
		if (is_three_string)
		{
			operand[1].clear();
			operand[2].clear();
		}
		if ((*i).operation == MC_DIV)
			need_brackets = false;	// ���� ����� �������, �� ������ �� ����� ����� - �� "�����" � ��������� ��� �����������
		if (need_brackets)
		{
			if (is_three_string)
			{
				operand[0].append(L" / ");
				operand[1].append(L"(  ");
				operand[2].append(L" \\ ");
			}
			else
				operand[0].assign(L"( ");
		}
		operand[0].append(str[0]);
		if (is_three_string)
		{
			operand[1].append(str[1]);
			operand[2].append(str[2]);
		}
		if (need_brackets)
		{
			if (is_three_string)
			{
				operand[0].append(L" \\ ");
				operand[1].append(L"  )");
				operand[2].append(L" / ");
			}
			else
				operand[0].append(L" )");
			need_brackets = false;
		}

		// ���������� ������� ��� ����������� �������������
		str[0].clear();
		if (is_three_string)
		{
			str[1].clear();
			str[2].clear();
			tmp1 = 1;			// ����� ������� ����� ��������� � str[1]
			tmp3 = tmp2 = 1;	// ������ ������� ����� ��������� � str[1]
		}
		else
		{
			tmp1 = 0;			// ����� ������� ����� ��������� � str[0]
			tmp3 = tmp2 = 0;	// ������ ������� ����� ��������� � str[0] (����������������)
		}

		// ����������� � ����� ���������
		if ((*i).left_operand == MC_SEE_ABOVE)
			str[tmp1].append(operand[tmp1]);
		else
			str[tmp1].append(to_wstring((*i).left_operand));

		// ����������� � ����� ���������
		if ((*i).operation == MC_DIV)
		{
			tmp2 = 2;	// ������ ������� ���� ��������� � str[2]
			tmp3 = 0;
		}
		else
		{
			str[tmp1].append({ L' ' });
			str[tmp1].append({ (*i).operation });
			str[tmp1].append({ L' ' });
		}

		// ����������� � ������ ���������
		if ((*i).right_operand == MC_SEE_ABOVE)
			str[tmp2].append(operand[tmp3]);
		else
			str[tmp2].append(to_wstring((*i).right_operand));

		// ��������� "������" ��� ������ �������
		if (is_three_string)
		{
			str[0].append(operand[0]);
			str[2].append(operand[2]);
			// ���� � str[1] ��������� ��� �����, ��
			if ((*i).right_operand == MC_SEE_ABOVE)
			{
				// ��������� ������� �����
				for (auto i = str[1].size() - str[0].size(); i > 0; i--)
				{
					str[0].assign(L' ' + str[0]);
					str[2].assign(L' ' + str[2]);
				}
			}
			// ���� � str[1] ��������� ��� ������, ��
			if ((*i).left_operand == MC_SEE_ABOVE)
			{
				// ��������� ������� ������
				for (auto i = str[1].size() - str[0].size(); i > 0; i--)
				{
					str[0].append({ L' ' });
					str[2].append({ L' ' });
				}
			}
		}
		else
		{
			if ((*i).operation == MC_DIV)
			{
				is_three_string = true;
				// ��������� ����� � ��������� ��������� ��� ����������� ���������
				AddSpaceToStringW(str[0], str[2]);
				for (auto i = str[0].size(); i > 0; i--)
					str[1].append({ L'-' });

			}
		}

		// ���� ���� ��������� ��� �������, �� ����� ����� ������
		if ((*i).operation == MC_ADD || (*i).operation == MC_SUB)
			need_brackets = true;
	}

	return;
}

//
// ���������� ��������� ���������
// exp - ������, ���������� ���������
//
decltype(_LEXEME::result) GetExpressionResult(const EXPRESSION &exp)
{
	auto i = exp.rbegin();
	return (*i).result;
}


//
// ��������� ��������� �������� ���������
// choice - ����� ��������� ��������� (���������� �������� � ��)
// exp - ������, � ������� ���������� ��������������� ��������� �������� ���������
//
void GenerateExpression(const enum COMPLEXITY choice, EXPRESSION &exp)
{
	// 1. �������������� 4 �������, ���������� ������� ��� ��������� � ���������, ����� ���������� �������� ��������� � ���� ��������
	LEXEME a[4];
	int mul_index = RollDice(),	// ����������� �������� �� 0 �� 3
		div_index = (mul_index > 1) ? mul_index - 2 : mul_index + 2,
		add_index = -1,
		sub_index = -1,
		above, below;

	a[mul_index].left_operand = GetRandomValue(MC_MAX_FACTOR1);
	a[mul_index].right_operand = GetRandomValue(MC_MAX_FACTOR2);
	a[mul_index].result = a[mul_index].left_operand * a[mul_index].right_operand;
	a[mul_index].operation = MC_MUL;

	a[div_index].right_operand = GetRandomValue(MC_MAX_DIVISOR);
	a[div_index].result = GetRandomValue(MC_MAX_QUOTIENT);
	a[div_index].left_operand = a[div_index].right_operand * a[div_index].result;
	a[div_index].operation = MC_DIV;

	// 2. ��������� ��� ������� (��������� � �������) �������� �������� ��� ���������
	if (mul_index < div_index)
	{
		above = mul_index;
		below = div_index;
	}
	else
	{
		above = div_index;
		below = mul_index;
	}
	if (ThrowCoin())
	{
		a[above + 1].result = a[below].left_operand;
		a[below].left_operand = MC_SEE_ABOVE;
	}
	else
	{
		a[above + 1].result = a[below].right_operand;
		a[below].right_operand = MC_SEE_ABOVE;
	}
	if (ThrowCoin())
	{
		a[above + 1].left_operand = MC_SEE_ABOVE;
		if (a[above].result < a[above + 1].result)
		{
			a[above + 1].right_operand = a[above + 1].result - a[above].result;
			a[above + 1].operation = MC_ADD;
			add_index = above + 1;
		}
		else
		{
			a[above + 1].right_operand = a[above].result - a[above + 1].result;
			a[above + 1].operation = MC_SUB;
			sub_index = above + 1;
		}

	}
	else
	{
		a[above + 1].right_operand = MC_SEE_ABOVE;
		if (a[above].result < a[above + 1].result)
		{
			a[above + 1].left_operand = a[above + 1].result - a[above].result;
			a[above + 1].operation = MC_ADD;
			add_index = above + 1;
		}
		else
		{
			a[above + 1].left_operand = a[above].result + a[above + 1].result;
			a[above + 1].operation = MC_SUB;
			sub_index = above + 1;
		}
	}

	// 3. ���������� 4 �������: � �� ������ ������ 0 ���  3
	if (above == 1)
	{
		// �������� ������ 0
		if (ThrowCoin())
		{
			a[0].result = a[1].left_operand;
			a[1].left_operand = MC_SEE_ABOVE;
		}
		else
		{
			a[0].result = a[1].right_operand;
			a[1].right_operand = MC_SEE_ABOVE;
		}
		if (add_index == -1)
		{
			// ����� ������������� ��������
			a[0].left_operand = GetRandomValue(a[0].result);
			a[0].right_operand = a[0].result - a[0].left_operand;
			a[0].operation = MC_ADD;
			add_index = 0;
		}
		else
		{
			// ����� ������������� ���������
			a[0].right_operand = GetRandomValue(MC_MAX_SUBTRAHEND);
			a[0].left_operand = a[0].result + a[0].right_operand;
			a[0].operation = MC_SUB;
			sub_index = 0;
		}
	}
	else
	{
		// �������� ������ 3
		if (ThrowCoin())
		{
			a[3].left_operand = MC_SEE_ABOVE;
			if (add_index == -1)
			{
				// ����� ������������� ��������
				a[3].right_operand = GetRandomValue(MC_MAX_SUMMAND);
				a[3].result = a[2].result + a[3].right_operand;
				a[3].operation = MC_ADD;
				add_index = 3;
			}
			else
			{
				// ����� ������������� ���������
				a[3].right_operand = GetRandomValue(a[2].result);
				a[3].result = a[2].result - a[3].right_operand;
				a[3].operation = MC_SUB;
				sub_index = 3;
			}
		}
		else
		{
			a[3].right_operand = MC_SEE_ABOVE;
			if (add_index == -1)
			{
				// ����� ������������� ��������
				a[3].left_operand = GetRandomValue(MC_MAX_SUMMAND);
				a[3].result = a[3].left_operand + a[2].result;
				a[3].operation = MC_ADD;
				add_index = 3;
			}
			else
			{
				// ����� ������������� ���������
				a[3].result = GetRandomValue(MC_MAX_RESULT);
				a[3].left_operand = a[3].result + a[2].result;
				a[3].operation = MC_SUB;
				sub_index = 3;
			}
		}
	}

	// 4. ��������� �������� ������ ��������� ����������� ������
	exp.clear();
	for (auto i = 0; i <= choice; i++)
		exp.push_back(a[i]);

	return;
}

//
// �������������� ���������� ����� ������ �� �������� �������� ����� (���������� ��� ������ ��������������� ������� ������)
// ��������! ������� ���������� ������������ ������ STDOUT "��������" ����� ������ ���� �������, ������� ����� (��������, ��� �������������� "�������" ScreenBuffer) ����� �������� � ��� ����������,
// ��������� ��� ������ ������� DuplicateHandle
// ���������� 0, ���� ���� ������
//
inline int ReOpenStdout()
{
	FILE *stream;
	auto stream_error = _wfreopen_s(&stream, L"CONOUT$", L"w", stdout);	// ������������� stdout - ��������� ��� ����������������� ��������������� ������� ������ (cout, wcout � �.�.)
	return !stream_error;
}

//
// ���������� Ctrl+C � �.�. ������� � �������
// ���������� 0, ���� ���� ������
//
BOOL CtrlHandler(DWORD fdwCtrlType)
{
	HANDLE hStd;

	switch (fdwCtrlType)
	{
	case CTRL_C_EVENT:			// Ctrl+C
	case CTRL_BREAK_EVENT:		// Ctrl+BREAK
	case CTRL_CLOSE_EVENT:		// Close console event
	case CTRL_LOGOFF_EVENT:		// System logoff event
	case CTRL_SHUTDOWN_EVENT:	// System shutdown event

		// ��������������� ������� �������
		if (!hTimerQueue)
		{
			DeleteTimerQueue(hTimerQueue);
			hTimer = nullptr;
			hTimerQueue = nullptr;
		}

		// ��������������� ������ ScreenBuffer
		_flushall();
		if (is_new_screen_buffer_active)
		{
			SetConsoleActiveScreenBuffer(hStdOutDup);						// ������ �������� ������ �������� �����, ��������� ��� ����� ����� ���������� ��������
			ReOpenStdout();													// ������������� stdout - ��������� ��� ����������������� ��������������� ������� ������ (cout, wcout � �.�.)
			CloseHandle(hNewScreenBuffer);									// ������� �������� ���������� ����������� ��������� ������
		}
		// ������� ��������� � ���������� �����
		hStd = GetStdHandle(STD_OUTPUT_HANDLE);								// �������� ���������� �������� ��������� ������ (hStdOutDup �� ����������, �.�. ����� �� ��� �� ���������������)
		if (hStd != INVALID_HANDLE_VALUE)
			SetConsoleTextAttribute(hStd, FOREGROUND_RED | FOREGROUND_INTENSITY);
		wcout << L"ABNORMAL PROGRAM TERMINATION!" << endl;
		wcout << L"RESULT NOT SAVED." << endl;
		if (hStd != INVALID_HANDLE_VALUE && is_new_screen_buffer_active)	// ���� is_new_screen_buffer_active == true, �� �������������� save_infoex ���������
			SetConsoleTextAttribute(hStd, save_infoex.wAttributes);
	default:					// WTF?!
		return FALSE;
	}
}

// ������ �������� ������ � ��������� ��� � ����
// ����� �����: http://www.codeproject.com/Articles/5051/Various-methods-for-capturing-the-screen
bool GetScreenshot(LPTSTR pszFile)
{
	// 1. ������ ��������
	int screen_width = GetSystemMetrics(SM_CXSCREEN);
	int screen_height = GetSystemMetrics(SM_CYSCREEN);
	if (!screen_width || !screen_height)
		return false;
	HWND hDesktopWnd = GetDesktopWindow();
	HDC hDesktopDC = GetDC(hDesktopWnd);
	if (!hDesktopDC)
		return false;
	HDC hCaptureDC = CreateCompatibleDC(hDesktopDC);
	if (!hCaptureDC)
		return false;
	HBITMAP hCaptureBitmap = CreateCompatibleBitmap(hDesktopDC, screen_width, screen_height);
	if (!hCaptureBitmap)
		return false;
	if (!SelectObject(hCaptureDC, hCaptureBitmap))
		return false;
	BitBlt(hCaptureDC, 0, 0, screen_width, screen_height, hDesktopDC, 0, 0, SRCCOPY /*| CAPTUREBLT*/);

	// 2. ��������� �������� �� ����
	PBITMAPINFO hCaptureBitmapInfo = CreateBitmapInfoStruct(hDesktopWnd, hCaptureBitmap);
	if (!hCaptureBitmapInfo)
		return false;
	if (!CreateBMPFile(hDesktopWnd, pszFile, hCaptureBitmapInfo, hCaptureBitmap, hDesktopDC))
		return false;

	// 3. ����������� �� ����� ����������� � ������
	LocalFree(hCaptureBitmapInfo);
	ReleaseDC(hDesktopWnd, hDesktopDC);
	DeleteDC(hCaptureDC);
	DeleteObject(hCaptureBitmap);

	return true;
}

//
// ������� ��� ��������� BMP � ���� - ����� �� MSDN ������������ � ������������ �����������
// The following example code defines a function that initializes the remaining structures, retrieves the array of palette indices, opens the file, copies the data, and closes the file.
// https://msdn.microsoft.com/en-us/library/dd145119(v=VS.85).aspx
bool CreateBMPFile(HWND hwnd, LPTSTR pszFile, PBITMAPINFO pbi, HBITMAP hBMP, HDC hDC)
{
	HANDLE hf;                 // file handle  
	BITMAPFILEHEADER hdr;       // bitmap file-header  
	PBITMAPINFOHEADER pbih;     // bitmap info-header  
	LPBYTE lpBits;              // memory pointer  
	DWORD dwTotal;              // total count of bytes  
	DWORD cb;                   // incremental count of bytes  
	BYTE *hp;                   // byte pointer  
	DWORD dwTmp;

	pbih = (PBITMAPINFOHEADER)pbi;
	lpBits = (LPBYTE)GlobalAlloc(GMEM_FIXED, pbih->biSizeImage);

	if (!lpBits)
		return false;

	// Retrieve the color table (RGBQUAD array) and the bits (array of palette indices) from the DIB.  
	if (!GetDIBits(hDC, hBMP, 0, (WORD)pbih->biHeight, lpBits, pbi,
		DIB_RGB_COLORS))
		return false;

	// Create the .BMP file. 
	hf = CreateFile(pszFile,
		GENERIC_READ | GENERIC_WRITE,
		(DWORD)0,
		NULL,
		CREATE_ALWAYS,
		FILE_ATTRIBUTE_NORMAL,
		(HANDLE)NULL);
	if (hf == INVALID_HANDLE_VALUE)
		return false;

	hdr.bfType = 0x4d42;        // 0x42 = "B" 0x4d = "M"  
	// Compute the size of the entire file.  
	hdr.bfSize = (DWORD)(sizeof(BITMAPFILEHEADER) +
		pbih->biSize + pbih->biClrUsed
		* sizeof(RGBQUAD) + pbih->biSizeImage);
	hdr.bfReserved1 = 0;
	hdr.bfReserved2 = 0;

	// Compute the offset to the array of color indices.  
	hdr.bfOffBits = (DWORD) sizeof(BITMAPFILEHEADER) +
		pbih->biSize + pbih->biClrUsed
		* sizeof(RGBQUAD);

	// Copy the BITMAPFILEHEADER into the .BMP file.  
	if (!WriteFile(hf, (LPVOID)&hdr, sizeof(BITMAPFILEHEADER),
		(LPDWORD)&dwTmp, NULL))
		return false;

	// Copy the BITMAPINFOHEADER and RGBQUAD array into the file.  
	if (!WriteFile(hf, (LPVOID)pbih, sizeof(BITMAPINFOHEADER)
		+ pbih->biClrUsed * sizeof(RGBQUAD),
		(LPDWORD)&dwTmp, (NULL)))
		return false;

	// Copy the array of color indices into the .BMP file.  
	dwTotal = cb = pbih->biSizeImage;
	hp = lpBits;
	if (!WriteFile(hf, (LPSTR)hp, (int)cb, (LPDWORD)&dwTmp, NULL))
		return false;

	// Close the .BMP file.  
	if (!CloseHandle(hf))
		return false;

	// Free memory.  
	GlobalFree((HGLOBAL)lpBits);

	return true;

}

//
// ������� ��� ��������� ��������� �� ��������� BITMAPINFO - ����� �� MSDN ������������ � ������������ �����������
// The following example code defines a function that initializes the remaining structures, retrieves the array of palette indices, opens the file, copies the data, and closes the file.
// https://msdn.microsoft.com/en-us/library/dd145119(v=VS.85).aspx
PBITMAPINFO CreateBitmapInfoStruct(HWND hwnd, HBITMAP hBmp)
{
	BITMAP bmp;
	PBITMAPINFO pbmi;
	WORD    cClrBits;

	// Retrieve the bitmap color format, width, and height.  
	if (!GetObject(hBmp, sizeof(BITMAP), (LPSTR)&bmp))
		return nullptr;

	// Convert the color format to a count of bits.  
	cClrBits = (WORD)(bmp.bmPlanes * bmp.bmBitsPixel);
	if (cClrBits == 1)
		cClrBits = 1;
	else if (cClrBits <= 4)
		cClrBits = 4;
	else if (cClrBits <= 8)
		cClrBits = 8;
	else if (cClrBits <= 16)
		cClrBits = 16;
	else if (cClrBits <= 24)
		cClrBits = 24;
	else cClrBits = 32;

	// Allocate memory for the BITMAPINFO structure. (This structure  
	// contains a BITMAPINFOHEADER structure and an array of RGBQUAD  
	// data structures.)  

	if (cClrBits < 24)
		pbmi = (PBITMAPINFO)LocalAlloc(LPTR,
		sizeof(BITMAPINFOHEADER) +
		sizeof(RGBQUAD) * (1 << cClrBits));

	// There is no RGBQUAD array for these formats: 24-bit-per-pixel or 32-bit-per-pixel 

	else
		pbmi = (PBITMAPINFO)LocalAlloc(LPTR,
		sizeof(BITMAPINFOHEADER));

	// Initialize the fields in the BITMAPINFO structure.  

	pbmi->bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	pbmi->bmiHeader.biWidth = bmp.bmWidth;
	pbmi->bmiHeader.biHeight = bmp.bmHeight;
	pbmi->bmiHeader.biPlanes = bmp.bmPlanes;
	pbmi->bmiHeader.biBitCount = bmp.bmBitsPixel;
	if (cClrBits < 24)
		pbmi->bmiHeader.biClrUsed = (1 << cClrBits);

	// If the bitmap is not compressed, set the BI_RGB flag.  
	pbmi->bmiHeader.biCompression = BI_RGB;

	// Compute the number of bytes in the array of color  
	// indices and store the result in biSizeImage.  
	// The width must be DWORD aligned unless the bitmap is RLE 
	// compressed. 
	pbmi->bmiHeader.biSizeImage = ((pbmi->bmiHeader.biWidth * cClrBits + 31) & ~31) / 8
		* pbmi->bmiHeader.biHeight;
	// Set biClrImportant to 0, indicating that all of the  
	// device colors are important.  
	pbmi->bmiHeader.biClrImportant = 0;
	return pbmi;
}

//
// ���������� ������� �������
//
VOID CALLBACK TimerRoutine(PVOID lpParam, BOOLEAN TimerOrWaitFired)
{
	time_t current_time, tmp;
	CONSOLE_SCREEN_BUFFER_INFOEX infoex;
	static WORD attributes = 0;
	bool is_blink_time, need_change_attributes;

	if (start_time) // ���� ����� ������� �������
	{
		// �������� ������� ������� ������
		current_time = chrono::system_clock::to_time_t(chrono::system_clock::now());
		tmp = (current_time - start_time) % MC_BLINK_TIME;
		is_blink_time = tmp >= (MC_BLINK_TIME - MC_BLINK_INTERVAL) && tmp <= (MC_BLINK_TIME - 1);
		tmp = current_time - start_time;
		is_blink_time = is_blink_time || (tmp >= (MC_SECONDS_FOR_TEST - MC_BLINK_INTERVAL_BEFORE_TEST_END) && tmp <= (MC_SECONDS_FOR_TEST));

		if (is_blink_time)
		{
			// ����� ������
			attributes = (attributes == save_infoex.wAttributes) ? MC_BLINK_COLOR : save_infoex.wAttributes;
			need_change_attributes = true;
		}
		else
		{
			// ... ������ ����������
			// ��������������� �������� � ������, ���� ��� �� ������������ �����
			if (attributes != save_infoex.wAttributes)
			{
				attributes = save_infoex.wAttributes;
				need_change_attributes = true;
			}
			else
				need_change_attributes = false;
		}

		if (need_change_attributes)
		{
			infoex.cbSize = sizeof(infoex);
			GetConsoleScreenBufferInfoEx(hNewScreenBuffer, &infoex);
			// TODO: �������� ���������� ��� � ������� SetConsoleScreenBufferInfoEx
			// ������ �� ���������� Microsoft ��� � ����� Windows Vista (�� 28.01.2015 ��������� Windows 8).
			// ��������! � �������, ����� Microsoft �������� ��� � ����� �������, ��� ����� ��������� ������� - ������ ���� ����� ��������� �������������.
			infoex.srWindow.Right++;
			infoex.srWindow.Bottom++;
			//
			infoex.wAttributes = attributes;
			SetConsoleScreenBufferInfoEx(hNewScreenBuffer, &infoex);
		}
	}

	return;
}

//
// ���������� ���������� ����������  ������� � ��������� �����
//
int TasksPerAllottedTime(TEST_LOG &log)
{
	auto i = 0;
	for (auto item : log)
		if (item.seconds > MC_SECONDS_FOR_TEST)
			break;
		else
			i++;
	return i;
}

//
// ���������� ���������� ������������ �������� � ��������� �����
//
int OperationsPerAllottedTime(TEST_LOG &log)
{
	auto i = 0;
	for (auto item : log)
		if (item.seconds > MC_SECONDS_FOR_TEST)
			break;
		else
			i += item.complexity;
	return i;
}

//
// ���������� ����� ���������� ������������ ��������
//
int TotalOperations(TEST_LOG &log)
{
	auto i = 0;
	for (auto item : log)
		i += item.complexity;
	return i;
}

//
// ���������� ������������ ���������� ����� - ������������ ������������������ �� �����, �������� ����� � ����� �������
//
int NumberOfMaxHits(TEST_LOG &log)
{
	auto hits = 0, max_hits = 0;
	for (auto item : log)
	{
		if (item.attempt == 1)
			hits++;
		else
			hits = 0;
		max_hits = max(max_hits, hits);
	}
	return max_hits;
}

//
// ���������� ���������� �����, �������� ����� � ����� �������
//
int NumberOfTasksWithOneAttempt(TEST_LOG &log)
{
	auto i = 0;
	for (auto item : log)
		if (item.attempt == 1)
			i++;
	return i;
}
