//launches the game and inserts the payload into the game
/*
	Copyright (C) 2021  admiralnelson aka kris b.

	This program is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program.  If not, see <http://www.gnu.org/licenses/>.

	email: admiralofinternetmeme[at]outlook[dot]com
	discord: z bb - tablet (bot easy)#1668
*/

//subject to changes, incomplete.

#define EXPORT extern "C" __declspec(dllexport)
#include <iostream>
#include <Windows.h>
#include "Detours.h"

const wchar_t* LicenseText =  L""
L"Warhammer 2 SNED (Script Native Extension DLL) Loader Copyright(C) 2021 admiralnelson\n"
L"This program comes with ABSOLUTELY NO WARRANTY;"
L"for details please read the supplied LICENSE.txt in the supplied archive\n"
L"This is free software, and you are welcome to redistribute it"
L"under certain conditions.\n";

std::wstring CharToWString(const char* text)
{
	const size_t size = strlen(text) + 1;
	wchar_t* wText = new wchar_t[size];
	mbstowcs(wText, text, size);
	std::wstring ret(wText);
	delete[] wText;
	return ret;
}

int main(int argc, char* argv[])
{
    std::wcout << LicenseText << L"\n";

	std::wstring argument;
	for (int i = 1; i < argc; i++)
	{
		argument += CharToWString(argv[i]) + std::wstring(L" ");
	}
	std::wcout << L"argument passed: " << argument << L"\n\n";

	argument = L"Warhammer2.exe " + argument;

	std::wcout << L"argument that will be passed to the game: " << argument << L"\n\n";

	STARTUPINFO si;
	PROCESS_INFORMATION pi;

	ZeroMemory(&si, sizeof(si));
	ZeroMemory(&pi, sizeof(pi));
	si.cb = sizeof(si);
	si.dwFlags = STARTF_USESHOWWINDOW;
	si.wShowWindow = SW_SHOW;

	if (!DetourCreateProcessWithDllEx(L"Warhammer2.exe",
		(LPWSTR)argument.data(), NULL, NULL, TRUE,
		CREATE_DEFAULT_ERROR_MODE,// | CREATE_SUSPENDED,
		NULL, NULL, &si, &pi,
		"minhook_test.dll", NULL))
	{
		std::wcout << L"failed to inject\n";
		MessageBox(0, L"failed", 0, 0);
	}
	else
	{
		std::wcout << L"inject ok\n";
	}

	ResumeThread(pi.hThread);

	WaitForSingleObject(pi.hProcess, INFINITE);

	CloseHandle(&si);
	CloseHandle(&pi);

	return EXIT_SUCCESS;
}

// Run program: Ctrl + F5 or Debug > Start Without Debugging menu
// Debug program: F5 or Debug > Start Debugging menu

// Tips for Getting Started: 
//   1. Use the Solution Explorer window to add/manage files
//   2. Use the Team Explorer window to connect to source control
//   3. Use the Output window to see build output and other messages
//   4. Use the Error List window to view errors
//   5. Go to Project > Add New Item to create new code files, or Project > Add Existing Item to add existing code files to the project
//   6. In the future, to open this project again, go to File > Open > Project and select the .sln file
