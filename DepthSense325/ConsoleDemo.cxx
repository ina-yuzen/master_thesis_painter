#include "Context.h"
#include "DepthMap.h"
#include "DSClient.h"
#include "OmniTouch.h"
#include "Recorder.h"

/*----------------------------------------------------------------------------*/
int main(int argc, char* argv[])
{
	auto client = mobamas::DSClient::create();
	auto context = std::make_shared<mobamas::Context>();
	context->ds_client = client;
	context->recorder = std::unique_ptr<mobamas::Recorder>(new mobamas::Recorder());

	if (!client->Prepare(context, mobamas::onNewDepthSample)) {
		return 1;
	}

	client->Run();

    return 0;
}