#pragma once

#include <fstream>
#include <Polycode.h>

#include "Context.h"

namespace mobamas {

class Recorder;

class Writer {
public:
	Writer(Models const& model, OperationMode const& mode);
	~Writer();
	void WriteTexture(Polycode::Texture* texture);
	void WritePose(Polycode::Skeleton* skeleton);
	std::wostream& log();
	Recorder& recorder() { return *recorder_; }

private:
	std::wstring dirname_;
	std::wofstream log_;
	std::unique_ptr<Recorder> recorder_;
};

}
