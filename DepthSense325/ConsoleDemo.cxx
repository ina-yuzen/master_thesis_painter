#include <fstream>

#include "Context.h"
#include "DepthMap.h"
#include "EditorApp.h"
#include "Models.h"
#include "PenAsMouse.h"
#include "Recorder.h"
#include "RSClient.h"

#ifndef NDEBUG
#include "OutputDebugStringBuf.h"
#endif

DWORD WINAPI RunRealSense(LPVOID lpParam) {
	auto context = (mobamas::Context*)lpParam;
	context->rs_client->Run();
	return 0;
}

int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
#ifndef NDEBUG
#ifdef _WIN32
	static OutputDebugStringBuf<char> charDebugOutput;
	std::cout.rdbuf(&charDebugOutput);
	std::cerr.rdbuf(&charDebugOutput);
	std::clog.rdbuf(&charDebugOutput);
		
	static OutputDebugStringBuf<wchar_t> wcharDebugOutput;
	std::wcout.rdbuf(&wcharDebugOutput);
	std::wcerr.rdbuf(&wcharDebugOutput);
	std::wclog.rdbuf(&wcharDebugOutput);
#endif
#endif

	mobamas::Models model = mobamas::Models::ROBOT;
	//if (strcmp(lpCmdLine, "miku") == 0)
		model = mobamas::Models::MIKU;

	auto context = std::make_shared<mobamas::Context>();
	auto client = std::make_shared<mobamas::RSClient>(context);
	context->model = model;
	context->operation_mode = mobamas::OperationMode::MouseMode;
	context->rs_client = client;
	context->recorder = std::unique_ptr<mobamas::Recorder>(new mobamas::Recorder());

	time_t tm;
	time(&tm);
	context->logfs.open("logs\\" + std::to_string(tm) + ".log", std::ios::out | std::ios::trunc);
	context->logfs << tm << ": Start with model " << model << std::endl;

	auto view = new Polycode::PolycodeView(hInstance, nCmdShow, L"MOBAM@S");
	mobamas::hWnd = view->hwnd;
	mobamas::EditorApp app(view, context);

	DWORD threadId;
	HANDLE hThread = NULL;
	if (context->operation_mode == mobamas::OperationMode::MidAirMode) {
		if (client->Prepare()) {
			hThread = CreateThread(NULL, 0, RunRealSense, context.get(), 0, &threadId);
			if (hThread == NULL) {
				OutputDebugString(L"Failed to start depth sense thread");
				LPTSTR text = NULL;
				FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_ALLOCATE_BUFFER, 0, GetLastError(), 0, text, MAX_PATH, 0);
				OutputDebugString(text);
				LocalFree(text);
				return 3;
			}
		}
		else {
			MessageBox(NULL, L"Failed to prepare RealSense", L"Insufficient", MB_ICONWARNING | MB_OK);
		}
	}

	MSG Msg;
	do {
		while (PeekMessage(&Msg, NULL, 0, 0, PM_REMOVE)) {
			TranslateMessage(&Msg);
			DispatchMessage(&Msg);
		}
	} while(app.Update());

	app.Shutdown();
	OutputDebugString(L"Shutting down...");
	if (hThread != NULL) {
		context->rs_client->Quit();
		auto result = WaitForMultipleObjects(1, &hThread, TRUE, 1000);
		switch (result) {
		case WAIT_TIMEOUT:
			OutputDebugString(L"BUG: WaitForMultipleObjects to join the depth sense thread timed out");
			break;
		case WAIT_FAILED:
			OutputDebugString(L"WaitForMultipleObjects failed");
			LPTSTR text = NULL;
			FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_ALLOCATE_BUFFER, 0, GetLastError(), 0, text, MAX_PATH, 0);
			OutputDebugString(text);
			LocalFree(text);
			break;
		}
	}
	time(&tm);
	context->logfs << time(nullptr) << ": End" << std::endl;
	context->logfs.close();
	return Msg.wParam;
}
