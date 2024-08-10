// MentalComputationCards.cpp: определяет точку входа для консольного приложения.
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
// Константы, определяющие сложность (разрядность) выражений
//
#define MC_MIN				1	// МИНИМАЛЬНО ДОПУСТИМОЕ ЗНАЧЕНИЕ ДЛЯ ОПЕРАНДА
#define MC_MAX_SUMMAND		99	// МАКСИМАЛЬНОЕ СЛАГАЕМОЕ
#define MC_MAX_SUBTRAHEND	99	// МАКСИМАЛЬНОЕ ВЫЧИТАЕМОЕ (должно быть всегда не больше уменьшаемого)
#define MC_MAX_FACTOR1		99	// МАКСИМАЛЬНОЕ МНОЖИТЕЛЬ
#define MC_MAX_FACTOR2		9	// МАКСИМАЛЬНОЕ МНОЖИТЕЛЬ
#define MC_MAX_DIVISOR		9	// МАКСИМАЛЬНОЕ ДЕЛИТЕЛЬ
#define MC_MAX_QUOTIENT		99	// МАКСИМАЛЬНОЕ ЧАСТНОЕ (должно быть не больше максимального результата)
#define MC_MAX_RESULT		999	// МАКСИМАЛЬНЫЙ РЕЗУЛЬТАТ

#ifndef MC_DEBUG
#define MC_NUMBER_OF_EXPRESSIONS			50										// Число выражение (задач) в тесте
#endif // !MC_DEBUG
#ifdef MC_DEBUG
#define MC_NUMBER_OF_EXPRESSIONS			3										// Число выражение (задач) в тесте
#endif // !MC_DEBUG
#define MC_SECONDS_FOR_TEST					600										// Общее время тестирования
#define MC_BLINK_INTERVAL_BEFORE_TEST_END	10										// Время мигания перед концом теста (секунды)
#define MC_BLINK_FREQUENCY					500										// Частота мигания в миллисекундах
#define MC_BLINK_COLOR						FOREGROUND_RED | FOREGROUND_INTENSITY	// Цвет для мигания
#define MC_BLINK_TIME						60										// Начинать мигать каждые столько-то секунд
#define MC_BLINK_INTERVAL					3										// И мигать в течении стольких-то секунд

//
// Используемые типы данных
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

enum COMPLEXITY { MC_ONE, MC_TWO, MC_THREE, MC_FOUR };	// Сложность генерируемого выражения. Сложность определяется количеством операций.

struct _TEST_LOG_ITEM
{
	wstring	expression;					// Выражение в символьном виде
	int complexity;						// Сложность выражения (количество операций)
	decltype(_LEXEME::result) result;	// Результат выражения
	int attempt;						// Общее число попыток вводе результата
	decltype(difftime(0, 0)) seconds;	// Секунда (с начала теста), на которой была решена задача
};

using TEST_LOG_ITEM = _TEST_LOG_ITEM;
using PTEST_LOG_ITEM = _TEST_LOG_ITEM *;

using TEST_LOG = vector < TEST_LOG_ITEM > ;
using PTEST_LOG = TEST_LOG *;

#define MC_ADD			L'+'
#define MC_SUB			L'-'
#define MC_MUL			L'x'
#define MC_DIV			L':'
#define MC_SEE_ABOVE				INT_MIN		// Для _LEXEME::left_operand и _LEXEME::right_operand - если они равны этому значению, то значение операнда надо брать из предыдущей лексемы
#define MC_NO_RESULT				INT_MIN		// Значение, которому результат не может быть равен никогда
#define MC_SIZE_OF_BUF_FOR_TIME		36			// Размер буфера для преобразования времени в строку
#define MC_FILENAME					L"MC_"		// Префикс имени с результатом

//
// Глобальные переменные
//
default_random_engine generator(int(time(nullptr)) - 1421077640);
HANDLE hStdOutDup;							// Содержит дубликат изначального дескриптора SDTOUT. Вынесено во внешние переменные из-за необходимости использования в обработчике событий Ctrl+C и т.п. событий
HANDLE hNewScreenBuffer;					// Содержит дескриптор нового экранного буфера . Вынесено во внешние переменные из-за необходимости использования в обработчике событий Ctrl+C и т.п. событий
CONSOLE_SCREEN_BUFFER_INFOEX save_infoex;	// Содержит копию настроек экранного буфера, с которыми запустилась программа. Вынесено во внешние переменные из-за необходимости использования в обработчике событий Ctrl+C и т.п. событий
bool is_new_screen_buffer_active = false;	// == true, если активен новый экранный буфер
HANDLE hTimer = nullptr;					// Дескриптор таймера. Вынесено во внешние переменные, чтобы можно было восстановить очередь таймера.
HANDLE hTimerQueue = nullptr;				// Дескриптор очереди таймера. Вынесено во внешние переменные, чтобы можно было восстановить очередь таймера.
time_t start_time = 0;						// Время начала теста. Вынесено во внешние переменные по причине использования в функции таймера.

//
// Прототипы используемых функций
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

	// Формируем имя файла для итогового результата
	if (argc != 2 || !GetTempFileName(argv[1], MC_FILENAME, 0, filename_with_result))
	{
		wcout << L"Usage: " << argv[0] << L" [drive:][path]" << endl;
		wcout << L"Example: " << argv[0] << L" D:\\" << endl;
		return 0;
	}

	// Создаём новый экранный буфер и переключаемся на него
	hStdOut = GetStdHandle(STD_OUTPUT_HANDLE);
	if (hStdOut == INVALID_HANDLE_VALUE)								// Получаем дескриптор текущего экранного буфера
		return GetLastError();
	if (!DuplicateHandle(GetCurrentProcess(),							// ... и дублируем его
		hStdOut,
		GetCurrentProcess(),
		&hStdOutDup,
		0,
		FALSE,
		DUPLICATE_SAME_ACCESS))
		return GetLastError();
	hNewScreenBuffer = CreateConsoleScreenBuffer(						// Создаём новый экранный буфер
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
	if (!GetConsoleScreenBufferInfoEx(hStdOut, &save_infoex))			// Получаем настройки текущего экранного буфера
		return GetLastError();
	if (!SetConsoleScreenBufferInfoEx(hNewScreenBuffer, &save_infoex))	// ... и применяем их к новому экранному буферу
		return GetLastError();

	wcout << L"Press any key to start test..." << endl << endl;
	_gettch();

	if (!SetConsoleActiveScreenBuffer(hNewScreenBuffer))				// Делаем активным новый экранный буфер
		return GetLastError();
	if (!ReOpenStdout())												// Переоткрываем stdout (дескриптор hStdOut после этого "испорчен", нужно будет использовать его копию hStdOutDup)
		return -1;
	is_new_screen_buffer_active = true;

	// Устанавливаем свой обработчик Ctrl+C и т.п. событий
	if (!SetConsoleCtrlHandler((PHANDLER_ROUTINE)CtrlHandler, TRUE))
		return GetLastError();

	// Запоминаем время начала теста и выводим его
	current_time = start_time = chrono::system_clock::to_time_t(chrono::system_clock::now());
	localtime_s(&temp_time, &start_time);
	cout << "Start at: " << put_time(&temp_time, "%H:%M:%S") << endl << endl;

	// Устанавливаем таймер
	hTimerQueue = CreateTimerQueue();
	if (!hTimerQueue)
		return -1;
	if (!CreateTimerQueueTimer(&hTimer, hTimerQueue, (WAITORTIMERCALLBACK)TimerRoutine, nullptr, 0, MC_BLINK_FREQUENCY, WT_EXECUTEINTIMERTHREAD))
		return -1;

	// --------------------------------------------------------------------------------------------
	// Сам тест
	// --------------------------------------------------------------------------------------------
	_flushall();
	for (auto i = 1; i <= MC_NUMBER_OF_EXPRESSIONS; i++)
	{
		// Генерируем выражение со случайной сложностью, частично заполняя лог
		log_item.complexity = RollDice();
		GenerateExpression(static_cast<COMPLEXITY>(log_item.complexity), expression);
		log_item.expression = GetExpressionString1(expression);
		log_item.result = GetExpressionResult(expression);

		// Выводим шапку
		current_time = chrono::system_clock::to_time_t(chrono::system_clock::now());
		localtime_s(&temp_time, &current_time);
		wcout << setfill(L'.') << setw(25) << setiosflags(ios::right) << L" " << resetiosflags(ios::right) << i << L" (";
		cout << put_time(&temp_time, "%H:%M:%S");
		wcout << L", +";
		current_time = static_cast<time_t>(difftime(current_time, start_time));
		gmtime_s(&temp_time, &current_time);
		cout << put_time(&temp_time, "%H:%M:%S");
		wcout << L" since start)" << endl << endl;

		// Выводим выражение
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

		// Цикл, пока не будет введён правильный ответ
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

		// Выводим футер
		wcout << endl << L"Elapsed time: " << log_item.seconds << L" seconds" << endl;
		if (log_item.attempt > 1)
			wcout << log_item.attempt << L" attempts" << endl;
		wcout << endl << endl;

		// Сохраняем результат
		log.push_back(log_item);
	}

	// Восстанавливаем очередь таймера
	if (!DeleteTimerQueue(hTimerQueue))
		return -1;
	hTimer = nullptr;
	hTimerQueue = nullptr;

	// Восстанавливаем старый ScreenBuffer
	if (!SetConsoleActiveScreenBuffer(hStdOutDup))						// Делаем активным старый экранный буфер, используя для этого ранее полученный дубликат
		return GetLastError();
	if (!ReOpenStdout())												// Переоткрываем stdout - требуется для работоспособности высокоуровневых функций вывода (cout, wcout и т.п.)
		return -1;
	if (!CloseHandle(hNewScreenBuffer))
		return -1;

	// Выводим итоговый результат
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
	hr = pVoice->Speak(L"ГЕНИЙ", SPF_DEFAULT, NULL);
	// Change pitch hr = pVoice->Speak(L"This sounds normal <pitch middle = '-10'/> but the pitch drops half way through", SPF_IS_XML, NULL );
	pVoice->Release();
	pVoice = NULL;
	}
	::CoUninitialize();
	*/

	// Делаем скриншот экрана и переименовываем получившийся файл в .BMP
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
// Возвращает случайное целое число от 0 (false) до 1 (true)
//
int ThrowCoin()
{
	static uniform_int_distribution<int> distribution(0, 1);
	return distribution(generator);
}

//
// Возвращает случайное целое число от 0 до 3
//
int RollDice()
{
	static uniform_int_distribution<int> distribution(0, 3);
	return distribution(generator);

}

//
// Возвращает случайное целое число от MC_MIN до max_value
//
int GetRandomValue(int max_value)
{
	uniform_int_distribution<int> distribution(MC_MIN, max_value);
	return distribution(generator);
}

//
// Выравнивает длину строк, добавляя к строке с наименьшей длиной пробелы (с двух сторон)
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
// Возвращает ссылку на символьное представление выражения.
// Деление представлено знаком MC_DIV (L':')
//
wstring& GetExpressionString1(const EXPRESSION &exp)
{
	static wstring str;
	wstring operand;
	bool need_brackets = false;

	for (auto i = exp.begin(); i != exp.end(); i++)
	{
		// Формируем из строки операнд, попутно определяясь со скобками
		operand.clear();
		if (need_brackets)
			operand = L"( ";
		operand += str;
		if (need_brackets)
		{
			operand += L" )";
			need_brackets = false;
		}

		// Формируем строку операции, используя нужные операнды
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

		// Если было умножение или деление, то будут нужны скобки
		if ((*i).operation == MC_ADD || (*i).operation == MC_SUB)
			need_brackets = true;
	}

	return str;
}

//
// Возвращает символьное представление выражения.
// Если нет деления, то результат возращается только в str[0] (str[1] и str[2] содержат пустые строки с длиной 0), флаг is_three_string при этом равен false
// Если деление есть, то результат возвращается в str[0], str[1] и str[3], флаг is_three_string при этом равен true
//
void GetExpressionString3(const EXPRESSION &exp, wstring str[3], bool &is_three_string)
{
	wstring operand[3];
	bool need_brackets = false;
	int tmp1, tmp2, tmp3;

	// Очищаем строки и выставляем флаг в false
	str[0].clear();
	str[1].clear();
	str[2].clear();
	is_three_string = false;

	//
	for (auto i = exp.begin(); i != exp.end(); i++)
	{
		// Формируем из предыдущей строки операнд, попутно определяясь со скобками
		operand[0].clear();
		if (is_three_string)
		{
			operand[1].clear();
			operand[2].clear();
		}
		if ((*i).operation == MC_DIV)
			need_brackets = false;	// Если будет деление, то скобки не будут нужны - всё "уйдет" в числитель или знаменатель
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

		// Определяем инедксы для дальнейшего использования
		str[0].clear();
		if (is_three_string)
		{
			str[1].clear();
			str[2].clear();
			tmp1 = 1;			// Левый операнд будем добавлять к str[1]
			tmp3 = tmp2 = 1;	// Правый операнд будем добавлять к str[1]
		}
		else
		{
			tmp1 = 0;			// Левый операнд будем добавлять к str[0]
			tmp3 = tmp2 = 0;	// Правый операнд будем добавлять к str[0] (предположительно)
		}

		// Разбираемся с левым операндом
		if ((*i).left_operand == MC_SEE_ABOVE)
			str[tmp1].append(operand[tmp1]);
		else
			str[tmp1].append(to_wstring((*i).left_operand));

		// Разбираемся с самой операцией
		if ((*i).operation == MC_DIV)
		{
			tmp2 = 2;	// Правый операнд надо добавлять к str[2]
			tmp3 = 0;
		}
		else
		{
			str[tmp1].append({ L' ' });
			str[tmp1].append({ (*i).operation });
			str[tmp1].append({ L' ' });
		}

		// Разбираемся с правым операндом
		if ((*i).right_operand == MC_SEE_ABOVE)
			str[tmp2].append(operand[tmp3]);
		else
			str[tmp2].append(to_wstring((*i).right_operand));

		// Подчищаем "хвосты" для разных случаев
		if (is_three_string)
		{
			str[0].append(operand[0]);
			str[2].append(operand[2]);
			// Если в str[1] добаление шло слева, то
			if ((*i).right_operand == MC_SEE_ABOVE)
			{
				// добавляем пробелы слева
				for (auto i = str[1].size() - str[0].size(); i > 0; i--)
				{
					str[0].assign(L' ' + str[0]);
					str[2].assign(L' ' + str[2]);
				}
			}
			// Если в str[1] добаление шло справа, то
			if ((*i).left_operand == MC_SEE_ABOVE)
			{
				// добавляем пробелы справа
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
				// Формируем черту и дополняем числитель или знаменатель пробелами
				AddSpaceToStringW(str[0], str[2]);
				for (auto i = str[0].size(); i > 0; i--)
					str[1].append({ L'-' });

			}
		}

		// Если было умножение или деление, то будут нужны скобки
		if ((*i).operation == MC_ADD || (*i).operation == MC_SUB)
			need_brackets = true;
	}

	return;
}

//
// Возвращает результат выражения
// exp - вектор, содержащий выражение
//
decltype(_LEXEME::result) GetExpressionResult(const EXPRESSION &exp)
{
	auto i = exp.rbegin();
	return (*i).result;
}


//
// Генерация выражения заданной сложности
// choice - задаём сложность выражения (количество операций в нём)
// exp - вектор, в который помещается сгенерированное выражение заданной сложности
//
void GenerateExpression(const enum COMPLEXITY choice, EXPRESSION &exp)
{
	// 1. Инициализируем 4 лексемы, определяем индексы для умножения и вычитания, затем определяем значения операндов в этих лексемах
	LEXEME a[4];
	int mul_index = RollDice(),	// Присваиваем значение от 0 до 3
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

	// 2. Связываем две лексемы (умножение и деление) лексемой сложения или вычитания
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

	// 3. Определяем 4 лексему: у неё всегда индекс 0 или  3
	if (above == 1)
	{
		// свободен индекс 0
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
			// нужно сгенерировать сложение
			a[0].left_operand = GetRandomValue(a[0].result);
			a[0].right_operand = a[0].result - a[0].left_operand;
			a[0].operation = MC_ADD;
			add_index = 0;
		}
		else
		{
			// нужно сгенерировать вычитание
			a[0].right_operand = GetRandomValue(MC_MAX_SUBTRAHEND);
			a[0].left_operand = a[0].result + a[0].right_operand;
			a[0].operation = MC_SUB;
			sub_index = 0;
		}
	}
	else
	{
		// свободен индекс 3
		if (ThrowCoin())
		{
			a[3].left_operand = MC_SEE_ABOVE;
			if (add_index == -1)
			{
				// нужно сгенерировать сложение
				a[3].right_operand = GetRandomValue(MC_MAX_SUMMAND);
				a[3].result = a[2].result + a[3].right_operand;
				a[3].operation = MC_ADD;
				add_index = 3;
			}
			else
			{
				// нужно сгенерировать вычитание
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
				// нужно сгенерировать сложение
				a[3].left_operand = GetRandomValue(MC_MAX_SUMMAND);
				a[3].result = a[3].left_operand + a[2].result;
				a[3].operation = MC_ADD;
				add_index = 3;
			}
			else
			{
				// нужно сгенерировать вычитание
				a[3].result = GetRandomValue(MC_MAX_RESULT);
				a[3].left_operand = a[3].result + a[2].result;
				a[3].operation = MC_SUB;
				sub_index = 3;
			}
		}
	}

	// 4. Заполняем итоговый вектор требуемым количеством лексем
	exp.clear();
	for (auto i = 0; i <= choice; i++)
		exp.push_back(a[i]);

	return;
}

//
// Переопределяет стандарный поток вывода на активный экранный буфер (необходимо для работы высокоуровневых функций вывода)
// ВНИМАНИЕ! Текущий дескриптор стандартного вывода STDOUT "портится" после вызова этой функции, поэтому далее (например, при восстановлении "старого" ScreenBuffer) нужно работать с его дубликатом,
// сделанным при помощи функции DuplicateHandle
// Возвращает 0, если была ошибка
//
inline int ReOpenStdout()
{
	FILE *stream;
	auto stream_error = _wfreopen_s(&stream, L"CONOUT$", L"w", stdout);	// Переоткрываем stdout - требуется для работоспособности высокоуровневых функций вывода (cout, wcout и т.п.)
	return !stream_error;
}

//
// Обработчик Ctrl+C и т.п. событий в консоле
// Возвращает 0, если была ошибка
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

		// Восстанавливаем очередь таймера
		if (!hTimerQueue)
		{
			DeleteTimerQueue(hTimerQueue);
			hTimer = nullptr;
			hTimerQueue = nullptr;
		}

		// Восстанавливаем старый ScreenBuffer
		_flushall();
		if (is_new_screen_buffer_active)
		{
			SetConsoleActiveScreenBuffer(hStdOutDup);						// Делаем активным старый экранный буфер, используя для этого ранее полученный дубликат
			ReOpenStdout();													// Переоткрываем stdout - требуется для работоспособности высокоуровневых функций вывода (cout, wcout и т.п.)
			CloseHandle(hNewScreenBuffer);									// Удаляем ненужный дескриптор предыдущего экранного буфера
		}
		// Выводим сообщение о прерывании теста
		hStd = GetStdHandle(STD_OUTPUT_HANDLE);								// Получаем дескриптор текущего экранного буфера (hStdOutDup не используем, т.к. вдруг он ещё не инициализирован)
		if (hStd != INVALID_HANDLE_VALUE)
			SetConsoleTextAttribute(hStd, FOREGROUND_RED | FOREGROUND_INTENSITY);
		wcout << L"ABNORMAL PROGRAM TERMINATION!" << endl;
		wcout << L"RESULT NOT SAVED." << endl;
		if (hStd != INVALID_HANDLE_VALUE && is_new_screen_buffer_active)	// Если is_new_screen_buffer_active == true, то гарантированно save_infoex заполнена
			SetConsoleTextAttribute(hStd, save_infoex.wAttributes);
	default:					// WTF?!
		return FALSE;
	}
}

// Делает скриншот экрана и сохраняет его в файл
// Взято здесь: http://www.codeproject.com/Articles/5051/Various-methods-for-capturing-the-screen
bool GetScreenshot(LPTSTR pszFile)
{
	// 1. Делаем скриншот
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

	// 2. Сохраняем скриншот на диск
	PBITMAPINFO hCaptureBitmapInfo = CreateBitmapInfoStruct(hDesktopWnd, hCaptureBitmap);
	if (!hCaptureBitmapInfo)
		return false;
	if (!CreateBMPFile(hDesktopWnd, pszFile, hCaptureBitmapInfo, hCaptureBitmap, hDesktopDC))
		return false;

	// 3. Освобождаем за собой дескрипторы и память
	LocalFree(hCaptureBitmapInfo);
	ReleaseDC(hDesktopWnd, hDesktopDC);
	DeleteDC(hCaptureDC);
	DeleteObject(hCaptureBitmap);

	return true;
}

//
// Функция для сохранеия BMP в файл - копия из MSDN документации с минимальными изменениями
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
// Функция для получения указателя на структуру BITMAPINFO - копия из MSDN документации с минимальными изменениями
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
// Обработчик событий таймера
//
VOID CALLBACK TimerRoutine(PVOID lpParam, BOOLEAN TimerOrWaitFired)
{
	time_t current_time, tmp;
	CONSOLE_SCREEN_BUFFER_INFOEX infoex;
	static WORD attributes = 0;
	bool is_blink_time, need_change_attributes;

	if (start_time) // Если отчёт времени начался
	{
		// Получаем условие мигания экрана
		current_time = chrono::system_clock::to_time_t(chrono::system_clock::now());
		tmp = (current_time - start_time) % MC_BLINK_TIME;
		is_blink_time = tmp >= (MC_BLINK_TIME - MC_BLINK_INTERVAL) && tmp <= (MC_BLINK_TIME - 1);
		tmp = current_time - start_time;
		is_blink_time = is_blink_time || (tmp >= (MC_SECONDS_FOR_TEST - MC_BLINK_INTERVAL_BEFORE_TEST_END) && tmp <= (MC_SECONDS_FOR_TEST));

		if (is_blink_time)
		{
			// Время мигать
			attributes = (attributes == save_infoex.wAttributes) ? MC_BLINK_COLOR : save_infoex.wAttributes;
			need_change_attributes = true;
		}
		else
		{
			// ... мигать прекращаем
			// Восстанавливаем атрибуты в буфере, если они не изначального цвета
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
			// TODO: Кустарно исправляем баг в функции SetConsoleScreenBufferInfoEx
			// Ошибка не исправлена Microsoft ещё с времён Windows Vista (на 28.01.2015 актуальна Windows 8).
			// ВНИМАНИЕ! В будущем, когда Microsoft исправит баг в своей функции, это будет серьёзной ошибкой - размер окна будет постоянно увеличиваться.
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
// Возвращает количество выполненых  заданий в отведённое время
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
// Возвращает количество произведённых операций в отведённое время
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
// Возвращает общее количество произведённых операций
//
int TotalOperations(TEST_LOG &log)
{
	auto i = 0;
	for (auto item : log)
		i += item.complexity;
	return i;
}

//
// Возвращает максимальное количество хитов - максимальная последовательность из задач, решённых задач с одной попытки
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
// Возвращает количество задач, решённых задач с одной попытки
//
int NumberOfTasksWithOneAttempt(TEST_LOG &log)
{
	auto i = 0;
	for (auto item : log)
		if (item.attempt == 1)
			i++;
	return i;
}
