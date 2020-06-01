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
		const auto& passes = fg.GetPasses();
		for (auto i : crst.sortedPasses) {
			const auto& passinfo = crst.idx2info.find(i)->second;
			for (const auto& rsrc : passinfo.constructRsrcs)
				rsrcMngr.Construct(rsrc);

			cout << "[Execute] " << passes[i].Name() << endl;

			for (const auto& rsrc : passinfo.destructRsrcs)
				rsrcMngr.Destruct(rsrc);
		}
	}
};

int main() {
	FG::Pass depth{
		"Depth pass",
		{},
		{"Depth Buffer"}
	};

	FG::Pass gbuffer{
		"GBuffer pass",
		{"Depth Buffer"},
		{"GBuffer1", "GBuffer2", "GBuffer3"}
	};

	FG::Pass lighting{
		"Lighting",
		{"GBuffer1", "GBuffer2", "GBuffer3"},
		{"Lighting Buffer"}
	};

	FG::Pass post{
		"Post",
		{"Lighting Buffer"},
		{"Final Target"}
	};

	FG::Pass debug{
		"Debug View",
		{"GBuffer3"},
		{"Debug Output"}
	};

	FG::FrameGraph fg;

	fg.AddPass(depth);
	fg.AddPass(gbuffer);
	fg.AddPass(lighting);
	fg.AddPass(post);
	fg.AddPass(debug);

	FG::Compiler compiler;

	auto [success, crst] = compiler.Compile(fg);
	
	cout << "------------------------[pass order]------------------------" << endl;
	for (auto i : crst.sortedPasses)
		cout << i << ": " << fg.GetPasses().at(i).Name() << endl;

	cout << "------------------------[resource info]------------------------" << endl;
	for (const auto& [name, info] : crst.rsrc2info) {
		cout << "- " << name << endl
			<< "   - writer: " << fg.GetPasses().at(info.writer).Name() << endl;

		if (!info.readers.empty()) {
			cout << "   - readers" << endl;
			for (auto reader : info.readers)
				cout << "     - " << fg.GetPasses().at(reader).Name() << endl;
		}

		if (info.last != static_cast<size_t>(-1))
			cout << "   - last: " << fg.GetPasses().at(info.last).Name() << endl;

		cout << "  - lifetime: " << fg.GetPasses().at(info.writer).Name() << " - ";
		if (info.last != static_cast<size_t>(-1))
			cout << fg.GetPasses().at(info.last).Name();
		cout << endl;
	}

	cout << "------------------------[pass graph]------------------------" << endl;
	cout << "digraph {" << endl;
	cout << "  node[style=filled fontcolor=white fontname=consolas];" << endl;
	for (const auto& pass : fg.GetPasses())
		cout << "  \"" << pass.Name() << "\" [shape = box color=\"#F79646\"];" << endl;
	for (const auto& [name, info] : crst.rsrc2info) {
		/*if (fg.GetImports().find(name) != fg.GetImports().end())
			cout << "  \"" << name << "\" [shape = ellipse color=\"#AD534D\"];" << endl;
		else*/
			cout << "  \"" << name << "\" [shape = ellipse color=\"#6597AD\"];" << endl;
	}
	for (const auto& pass : fg.GetPasses()) {
		for (const auto& input : pass.Inputs())
			cout << "  \"" << input << "\" -> \"" << pass.Name()
			<< "\" [color=\"#9BBB59\"];" << endl;
		for (const auto& output : pass.Outputs())
			cout << "  \"" << pass.Name() << "\" -> \"" << output << "\" [color=\"#B54E4C\"];" << endl;
	}
	cout << "}" << endl;

	cout << "------------------------[Execute]------------------------" << endl;

	ResourceMngr rsrcMngr;
	Executor executor;
	executor.Execute(fg, crst, rsrcMngr);

	return 0;
}
