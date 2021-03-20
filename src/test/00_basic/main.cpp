#include <UFG/UFG.hpp>

#include <iostream>
#include <cassert>

using namespace std;
using namespace Ubpa;

class ResourceMngr {
public:
	void Construct(const UFG::FrameGraph& fg, size_t rsrcNodeIdx) {
		cout << "[Construct] " << fg.GetResourceNodes().at(rsrcNodeIdx).Name() << endl;
	}

	void Destruct(const UFG::FrameGraph& fg, size_t rsrcNodeIdx) {
		cout << "[Destruct]  " << fg.GetResourceNodes().at(rsrcNodeIdx).Name() << endl;
	}

	void Move(const UFG::FrameGraph& fg, size_t dstRsrcNodeIdx, size_t srcRsrcNodeIdx) {
		cout << "[Move]      " << fg.GetResourceNodes().at(dstRsrcNodeIdx).Name()
			<< " <- " << fg.GetResourceNodes().at(srcRsrcNodeIdx).Name() << endl;
	}
};

class Executor {
public:
	virtual void Execute(
		const UFG::FrameGraph& fg,
		const UFG::Compiler::Result& crst,
		ResourceMngr& rsrcMngr)
	{
		auto target = crst.idx2info.find(static_cast<size_t>(-1));
		if (target != crst.idx2info.end()) {
			const auto& passinfo = target->second;
			for (const auto& rsrc : passinfo.constructRsrcs)
				rsrcMngr.Construct(fg, rsrc);
			for (const auto& rsrc : passinfo.destructRsrcs)
				rsrcMngr.Destruct(fg, rsrc);
			for (const auto& rsrc : passinfo.moveRsrcs)
				rsrcMngr.Move(fg, crst.moves_src2dst.find(rsrc)->second, rsrc);
		}

		const auto& passnodes = fg.GetPassNodes();
		for (auto i : crst.sortedPasses) {
			const auto& passinfo = crst.idx2info.find(i)->second;

			for (const auto& rsrc : passinfo.constructRsrcs)
				rsrcMngr.Construct(fg, rsrc);

			cout << "[Execute]   " << passnodes[i].Name() << endl;

			for (const auto& rsrc : passinfo.destructRsrcs)
				rsrcMngr.Destruct(fg, rsrc);

			for (const auto& rsrc : passinfo.moveRsrcs)
				rsrcMngr.Move(fg, crst.moves_src2dst.find(rsrc)->second, rsrc);
		}
	}
};

int main() {
	UFG::FrameGraph fg("test 00 basic");

	size_t depthbuffer = fg.RegisterResourceNode("Depth Buffer");
	size_t depthbuffer2 = fg.RegisterResourceNode("Depth Buffer 2");
	size_t gbuffer1 = fg.RegisterResourceNode("GBuffer1");
	size_t gbuffer2 = fg.RegisterResourceNode("GBuffer2");
	size_t gbuffer3 = fg.RegisterResourceNode("GBuffer3");
	size_t lightingbuffer = fg.RegisterResourceNode("Lighting Buffer");
	size_t finaltarget = fg.RegisterResourceNode("Final Target");
	size_t debugoutput = fg.RegisterResourceNode("Debug Output");

	fg.RegisterPassNode(
		"Depth pass",
		{},
		{ depthbuffer }
	);
	fg.RegisterMoveNode(depthbuffer2, depthbuffer);
	fg.RegisterPassNode(
		"GBuffer pass",
		{ },
		{ depthbuffer2,gbuffer1,gbuffer2,gbuffer3 }
	);
	fg.RegisterPassNode(
		"Lighting",
		{ depthbuffer2,gbuffer1,gbuffer2,gbuffer3 },
		{ lightingbuffer }
	);
	fg.RegisterPassNode(
		"Post",
		{ lightingbuffer },
		{ finaltarget }
	);
	fg.RegisterPassNode(
		"Present",
		{ finaltarget },
		{ }
	);
	fg.RegisterPassNode(
		"Debug View",
		{ gbuffer3 },
		{ debugoutput }
	);

	cout << "------------------------[frame graph]------------------------" << endl;
	cout << "[Resource]" << endl;
	for (size_t i = 0; i < fg.GetResourceNodes().size(); i++)
		cout << "- " << i << " : " << fg.GetResourceNodes().at(i).Name() << endl;
	cout << "[Pass]" << endl;
	for (size_t i = 0; i < fg.GetPassNodes().size(); i++)
		cout << "- " << i << " : " << fg.GetPassNodes().at(i).Name() << endl;

	UFG::Compiler compiler;

	auto [success, crst] = compiler.Compile(fg);

	cout << "------------------------[pass graph]------------------------" << endl;
	cout << crst.passgraph.ToGraphvizGraph(fg).Dump() << endl;

	cout << "------------------------[pass order]------------------------" << endl;
	for (size_t i = 0; i < crst.sortedPasses.size(); i++)
		cout << i << ": " << fg.GetPassNodes().at(crst.sortedPasses[i]).Name() << endl;

	cout << "------------------------[resource info]------------------------" << endl;
	for (const auto& [idx, info] : crst.rsrc2info) {
		cout << "- " << fg.GetResourceNodes().at(idx).Name() << endl
			<< "  - writer: " << fg.GetPassNodes().at(info.writer).Name() << endl;

		if (!info.readers.empty()) {
			cout << "  - readers" << endl;
			for (auto reader : info.readers)
				cout << "    * " << fg.GetPassNodes().at(reader).Name() << endl;
		}

		cout << "  - lifetime: " << fg.GetPassNodes().at(crst.sortedPasses[info.first]).Name() << " - "
			<< fg.GetPassNodes().at(crst.sortedPasses[info.last]).Name();
		cout << endl;
	}

	cout << "------------------------[Graphviz]------------------------" << endl;

	auto g = fg.ToGraphvizGraph();
	cout << g.Dump() << endl;

	cout << "------------------------[Execute]------------------------" << endl;

	ResourceMngr rsrcMngr;
	Executor executor;
	executor.Execute(fg, crst, rsrcMngr);

	return 0;
}
