#include <UFG/core/core.h>

#include <iostream>

using namespace std;
using namespace Ubpa;

class ResourceMngr {
public:
	void Construct(const FG::FrameGraph& fg, size_t rsrcNodeIdx) {
		cout << "[Construct] " << fg.GetResourceNodes().at(rsrcNodeIdx).Name() << endl;
	}

	void Destruct(const FG::FrameGraph& fg, size_t rsrcNodeIdx) {
		cout << "[Destruct] " << fg.GetResourceNodes().at(rsrcNodeIdx).Name() << endl;
	}
};

class Executor {
public:
	virtual void Execute(
		const FG::FrameGraph& fg,
		const FG::Compiler::Result& crst,
		ResourceMngr& rsrcMngr)
	{
		const auto& passnodes = fg.GetPassNodes();
		for (auto i : crst.sortedPasses) {
			const auto& passinfo = crst.idx2info.find(i)->second;

			for (const auto& rsrc : passinfo.constructRsrcs)
				rsrcMngr.Construct(fg, rsrc);

			cout << "[Execute] " << passnodes[i].Name() << endl;

			for (const auto& rsrc : passinfo.destructRsrcs)
				rsrcMngr.Destruct(fg, rsrc);
		}
	}
};

int main() {
	FG::FrameGraph fg;
	fg.Name = "test 00 basic";

	size_t depthbuffer = fg.AddResourceNode("Depth Buffer");
	size_t gbuffer1 = fg.AddResourceNode("GBuffer1");
	size_t gbuffer2 = fg.AddResourceNode("GBuffer2");
	size_t gbuffer3 = fg.AddResourceNode("GBuffer3");
	size_t lightingbuffer = fg.AddResourceNode("Lighting Buffer");
	size_t finaltarget = fg.AddResourceNode("Final Target");
	size_t debugoutput = fg.AddResourceNode("Debug Output");

	fg.AddPassNode(
		"Depth pass",
		{},
		{ depthbuffer }
	);
	fg.AddPassNode(
		"GBuffer pass",
		{ depthbuffer },
		{ gbuffer1,gbuffer2,gbuffer3 }
	);
	fg.AddPassNode(
		"Lighting",
		{ gbuffer1,gbuffer2,gbuffer3 },
		{ lightingbuffer }
	);
	fg.AddPassNode(
		"Post",
		{ lightingbuffer },
		{ finaltarget }
	);
	fg.AddPassNode(
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

	FG::Compiler compiler;

	auto [success, crst] = compiler.Compile(fg);

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
