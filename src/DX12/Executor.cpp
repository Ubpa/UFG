#include <UFG/DX12/Executor.h>

#include <UFG/DX12/RsrcMngr.h>

using namespace Ubpa::UFG;

void DX12::Executor::Execute(const Compiler::Result& crst, RsrcMngr& rsrcMngr)
{
	rsrcMngr.DHReserve();
	rsrcMngr.AllocateHandle();
	for (auto passNodeIdx : crst.sortedPasses) {
		const auto& passinfo = crst.idx2info.find(passNodeIdx)->second;

		for (const auto& rsrc : passinfo.constructRsrcs)
			rsrcMngr.Construct(rsrc);

		auto passRsrcs = rsrcMngr.RequestPassRsrcs(passNodeIdx);
		passFuncs[passNodeIdx](passRsrcs);

		for (const auto& rsrc : passinfo.destructRsrcs)
			rsrcMngr.Destruct(rsrc);
	}
}
