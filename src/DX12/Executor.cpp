#include <UFG/DX12/Executor.h>

#include <UFG/DX12/RsrcMngr.h>

using namespace Ubpa::UFG;

void DX12::Executor::Execute(const Compiler::Result& crst, RsrcMngr& rsrcMngr)
{
	rsrcMngr.DHReserve();
	rsrcMngr.AllocateHandle();

	auto target = crst.idx2info.find(static_cast<size_t>(-1));
	if (target != crst.idx2info.end()) {
		const auto& passinfo = target->second;
		for (const auto& rsrc : passinfo.constructRsrcs)
			rsrcMngr.Construct(rsrc);
		for (const auto& rsrc : passinfo.destructRsrcs)
			rsrcMngr.Destruct(rsrc);
		for (const auto& rsrc : passinfo.moveRsrcs) {
			auto dst = crst.moves_src2dst.find(rsrc)->second;
			rsrcMngr.Move(dst, rsrc);
		}
	}

	for (auto passNodeIdx : crst.sortedPasses) {
		const auto& passinfo = crst.idx2info.find(passNodeIdx)->second;

		for (const auto& rsrc : passinfo.constructRsrcs)
			rsrcMngr.Construct(rsrc);

		auto passRsrcs = rsrcMngr.RequestPassRsrcs(passNodeIdx);
		passFuncs[passNodeIdx](passRsrcs);

		for (const auto& rsrc : passinfo.destructRsrcs)
			rsrcMngr.Destruct(rsrc);

		for (const auto& rsrc : passinfo.moveRsrcs) {
			auto dst = crst.moves_src2dst.find(rsrc)->second;
			rsrcMngr.Move(dst, rsrc);
		}
	}
}
