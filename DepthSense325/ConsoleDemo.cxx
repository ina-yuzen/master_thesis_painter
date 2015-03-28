#include "Context.h"
#include "DepthMap.h"
#include "EditorApp.h"
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

	auto client = std::make_shared<mobamas::RSClient>();
	auto context = std::make_shared<mobamas::Context>();
	context->rs_client = client;
	context->recorder = std::unique_ptr<mobamas::Recorder>(new mobamas::Recorder());

	auto view = new Polycode::PolycodeView(hInstance, nCmdShow, L"MOBAM@S");
	mobamas::EditorApp app(view, context);

	DWORD threadId;
	HANDLE hThread = NULL;
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

	MSG Msg;
	do {
		while (PeekMessage(&Msg, NULL, 0, 0, PM_REMOVE)) {
			TranslateMessage(&Msg);
			DispatchMessage(&Msg);
		}
	} while(app.Update());

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
	return Msg.wParam;
}
