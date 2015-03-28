#include <Polycode.h>
#include <pxcstatus.h>

namespace mobamas {

std::vector<Polycode::Vector3> ActualVertexPositions(Polycode::SceneMesh *mesh);
void ReportPxcBadStatus(const pxcStatus& status);

}