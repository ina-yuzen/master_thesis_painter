#include "Context.h"
#include "DepthMap.h"
#include "DSClient.h"
#include "EditorApp.h"
#include "OmniTouch.h"
#include "Recorder.h"

#ifndef NDEBUG
#include "OutputDebugStringBuf.h"
#endif

DWORD WINAPI RunDepthSense(LPVOID lpParam) {
	auto context = (mobamas::Context*)lpParam;
	context->ds_client->Run();
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

	auto client = mobamas::DSClient::create();
	auto context = std::make_shared<mobamas::Context>();
	context->ds_client = client;
	context->recorder = std::unique_ptr<mobamas::Recorder>(new mobamas::Recorder());

	if (!client->Prepare(context, mobamas::onNewDepthSample)) {
		return 1;
	}

	auto view = new Polycode::PolycodeView(hInstance, nCmdShow, L"MOBAM@S");
	mobamas::EditorApp app(view, context);

	DWORD threadId;
	auto hThread = CreateThread(NULL, 0, RunDepthSense, context.get(), 0, &threadId);
	if (hThread == NULL) {
		OutputDebugString(L"Failed to start depth sense thread");
		LPTSTR text = NULL;
		FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_ALLOCATE_BUFFER, 0, GetLastError(), 0, text, MAX_PATH, 0);
		OutputDebugString(text);
		LocalFree(text);
		return 3;
	}

	MSG Msg;
	do {
		while (PeekMessage(&Msg, NULL, 0, 0, PM_REMOVE)) {
			TranslateMessage(&Msg);
			DispatchMessage(&Msg);
		}
	} while(app.Update());

	OutputDebugString(L"Shutting down...");
	context->ds_client->Quit();
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
	return Msg.wParam;
}
