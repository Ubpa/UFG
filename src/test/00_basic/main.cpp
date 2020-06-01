#include <UFG/UFG.h>

#include <iostream>

using namespace std;
using namespace Ubpa;

class ResourceMngr {
public:
	void Construct(const std::string& resource_name) {
		cout << "[Construct] " << resource_name << endl;
	}

	void Destruct(const std::string& resource_name) {
		cout << "[Destruct] " << resource_name << endl;
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
				rsrcMngr.Construct(fg.GetResourceNode(rsrc).Name());

			cout << "[Execute] " << passnodes[i].Name() << endl;

			for (const auto& rsrc : passinfo.destructRsrcs)
				rsrcMngr.Destruct(fg.GetResourceNode(rsrc).Name());
		}
	}
};

int main() {
	FG::FrameGraph fg;

	size_t depthbuffer = fg.AddResourceNode("Depth Buffer");
	size_t gbuffer1 = fg.AddResourceNode("GBuffer1");
	size_t gbuffer2 = fg.AddResourceNode("GBuffer2");
	size_t gbuffer3 = fg.AddResourceNode("GBuffer3");
	size_t lightingbuffer = fg.AddResourceNode("Lighting Buffer");
	size_t finaltarget = fg.AddResourceNode("Final Target");
	size_t debugoutput = fg.AddResourceNode("Debug Output");

	FG::PassNode depth{
		"Depth pass",
		{},
		{depthbuffer}
	};

	FG::PassNode gbuffer{
		"GBuffer pass",
		{depthbuffer},
		{gbuffer1,gbuffer2,gbuffer3}
	};

	FG::PassNode lighting{
		"Lighting",
		{gbuffer1,gbuffer2,gbuffer3},
		{lightingbuffer}
	};

	FG::PassNode post{
		"Post",
		{lightingbuffer},
		{finaltarget}
	};

	FG::PassNode debug{
		"Debug View",
		{gbuffer3},
		{debugoutput}
	};

	fg.AddPassNode(depth);
	fg.AddPassNode(gbuffer);
	fg.AddPassNode(lighting);
	fg.AddPassNode(post);
	fg.AddPassNode(debug);

	FG::Compiler compiler;

	auto [success, crst] = compiler.Compile(fg);
	
	cout << "------------------------[pass order]------------------------" << endl;
	for (auto i : crst.sortedPasses)
		cout << i << ": " << fg.GetPassNodes().at(i).Name() << endl;

	cout << "------------------------[resource info]------------------------" << endl;
	for (const auto& [idx, info] : crst.rsrc2info) {
		cout << "- " << fg.GetResourceNode(idx).Name() << endl
			<< "   - writer: " << fg.GetPassNodes().at(info.writer).Name() << endl;

		if (!info.readers.empty()) {
			cout << "   - readers" << endl;
			for (auto reader : info.readers)
				cout << "     - " << fg.GetPassNodes().at(reader).Name() << endl;
		}

		if (info.last != static_cast<size_t>(-1))
			cout << "   - last: " << fg.GetPassNodes().at(info.last).Name() << endl;

		cout << "  - lifetime: " << fg.GetPassNodes().at(info.writer).Name() << " - ";
		if (info.last != static_cast<size_t>(-1))
			cout << fg.GetPassNodes().at(info.last).Name();
		cout << endl;
	}

	cout << "------------------------[pass graph]------------------------" << endl;
	cout << "digraph {" << endl;
	cout << "  node[style=filled fontcolor=white fontname=consolas];" << endl;
	for (const auto& pass : fg.GetPassNodes())
		cout << "  \"" << pass.Name() << "\" [shape = box color=\"#F79646\"];" << endl;
	for (const auto& [idx, info] : crst.rsrc2info)
		cout << "  \"" << fg.GetResourceNode(idx).Name() << "\" [shape = ellipse color=\"#6597AD\"];" << endl;
	for (const auto& pass : fg.GetPassNodes()) {
		for (const auto& input : pass.Inputs())
			cout << "  \"" << fg.GetResourceNode(input).Name() << "\" -> \"" << pass.Name()
			<< "\" [color=\"#9BBB59\"];" << endl;
		for (const auto& output : pass.Outputs())
			cout << "  \"" << pass.Name() << "\" -> \"" << fg.GetResourceNode(output).Name() << "\" [color=\"#B54E4C\"];" << endl;
	}
	cout << "}" << endl;

	cout << "------------------------[Execute]------------------------" << endl;

	ResourceMngr rsrcMngr;
	Executor executor;
	executor.Execute(fg, crst, rsrcMngr);

	return 0;
}
