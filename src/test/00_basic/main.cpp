#include <UFG/UFG.hpp>

#include <iostream>
#include <cassert>

using namespace std;
using namespace Ubpa;

class ResourceMngr {
public:
	void Construct(const UFG::FrameGraph& fg, size_t rsrcNodeIdx) {
		cout << "[Construct] " << fg.GetResourceNodes()[rsrcNodeIdx].Name() << endl;
	}

	void Destruct(const UFG::FrameGraph& fg, size_t rsrcNodeIdx) {
		cout << "[Destruct]  " << fg.GetResourceNodes()[rsrcNodeIdx].Name() << endl;
	}

	void Move(const UFG::FrameGraph& fg, size_t dstRsrcNodeIdx, size_t srcRsrcNodeIdx) {
		cout << "[Move]      " << fg.GetResourceNodes()[dstRsrcNodeIdx].Name()
			<< " <- " << fg.GetResourceNodes()[srcRsrcNodeIdx].Name() << endl;
	}
};

class Executor {
public:
	virtual void Execute(
		const UFG::FrameGraph& fg,
		const UFG::Compiler::Result& crst,
		ResourceMngr& rsrcMngr)
	{
		auto sorted_passes = crst.passgraph.TopoSort();
		if (!sorted_passes)
			return;

		std::map<size_t, size_t> remain_reader_cnt_map; // resource idx -> reader cnt
		for (auto pass : *sorted_passes) {
			// construct writed resources
			for (auto output : fg.GetPassNodes()[pass].Outputs()) {
				if (crst.moves_dst2src.contains(output))
					continue;

				rsrcMngr.Construct(fg, output);
			}

			// execute
			cout << "[Execute]   " << fg.GetPassNodes()[pass].Name() << endl;

			// count down readers
			for (auto input : fg.GetPassNodes()[pass].Inputs()) {
				if (!remain_reader_cnt_map.contains(input))
					remain_reader_cnt_map.emplace(input, crst.rsrc2info.at(input).readers.size());
				auto& cnt = remain_reader_cnt_map[input];
				--cnt;
				if (cnt == 0) {
					// destruct or move
					if (auto target = crst.moves_src2dst.find(input); target != crst.moves_src2dst.end())
						rsrcMngr.Move(fg, input, target->second);
					else
						rsrcMngr.Destruct(fg, input);
				}
			}
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
		cout << "- " << i << " : " << fg.GetResourceNodes()[i].Name() << endl;
	cout << "[Pass]" << endl;
	for (size_t i = 0; i < fg.GetPassNodes().size(); i++)
		cout << "- " << i << " : " << fg.GetPassNodes()[i].Name() << endl;

	UFG::Compiler compiler;

	auto crst = compiler.Compile(fg);
	assert(crst.has_value());

	cout << "------------------------[pass graph]------------------------" << endl;
	cout << crst->passgraph.ToGraphvizGraph(fg).Dump() << endl;

	cout << "------------------------[resource info]------------------------" << endl;
	for (const auto& [idx, info] : crst->rsrc2info) {
		cout << "- " << fg.GetResourceNodes()[idx].Name() << endl
			<< "  - writer: " << fg.GetPassNodes()[info.writer].Name() << endl;

		if (!info.readers.empty()) {
			cout << "  - readers" << endl;
			for (auto reader : info.readers)
				cout << "    * " << fg.GetPassNodes()[reader].Name() << endl;
		}

		cout << endl;
	}

	cout << "------------------------[Graphviz]------------------------" << endl;

	auto g = fg.ToGraphvizGraph();
	cout << g.Dump() << endl;

	cout << "------------------------[Execute]------------------------" << endl;

	ResourceMngr rsrcMngr;
	Executor executor;
	executor.Execute(fg, *crst, rsrcMngr);

	return 0;
}
