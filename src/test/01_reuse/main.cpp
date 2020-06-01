#include <UFG/UFG.h>

#include <iostream>

using namespace std;
using namespace Ubpa;

struct RsrcType {
	size_t size;
	bool operator==(const RsrcType& rhs) const noexcept {
		return size == rhs.size;
	}
};

namespace std {
	template<>
	struct hash<RsrcType> {
		bool operator()(const RsrcType& type) const noexcept {
			return hash<size_t>{}(type.size);
		}
	};
}

struct Resource {
	float* buffer;
};

class ResourceMngr {
public:
	~ResourceMngr() {
		for (const auto& [type, rsrcs] : pool) {
			for (auto rsrc : rsrcs)
				delete[] rsrc.buffer;
		}
	}

	void Construct(const string& name, size_t rsrcNodeIdx) {
		Resource rsrc;

		if (IsImported(rsrcNodeIdx)) {
			rsrc = importeds[rsrcNodeIdx];
			cout << "[Construct] Import | " << name << " @" << rsrc.buffer << endl;
		}
		else {
			auto type = temporals[rsrcNodeIdx];
			auto& typefrees = pool[type];
			if (typefrees.empty()) {
				rsrc.buffer = new float[type.size];
				cout << "[Construct] Create | " << name << " @" << rsrc.buffer << endl;
			}
			else {
				rsrc = typefrees.back();
				typefrees.pop_back();
				cout << "[Construct] Init | " << name << " @" << rsrc.buffer << endl;
			}
		}
		actives[rsrcNodeIdx] = rsrc;
	}

	void Destruct(const string& name, size_t rsrcNodeIdx) {
		auto rsrc = actives[rsrcNodeIdx];
		if (!IsImported(rsrcNodeIdx)) {
			pool[temporals[rsrcNodeIdx]].push_back(actives[rsrcNodeIdx]);
			cout << "[Destruct] Recycle | " << name << " @" << rsrc.buffer << endl;
		}
		else
			cout << "[Destruct] Import | " << name << " @" << rsrc.buffer << endl;

		actives.erase(rsrcNodeIdx);
	}

	ResourceMngr& RegisterImportedRsrc(size_t rsrcNodeIdx, Resource rsrc) {
		importeds[rsrcNodeIdx] = rsrc;
		return *this;
	}

	ResourceMngr& RegisterTemporalRsrc(size_t rsrcNodeIdx, RsrcType type) {
		temporals[rsrcNodeIdx] = type;
		return *this;
	}

	bool IsImported(size_t rsrcNodeIdx) const noexcept {
		return importeds.find(rsrcNodeIdx) != importeds.end();
	}

private:
	// rsrcNodeIdx -> rsrc
	std::unordered_map<size_t, Resource> importeds;
	// rsrcNodeIdx -> type
	std::unordered_map<size_t, RsrcType> temporals;
	// type -> vector<rsrc>
	std::unordered_map<RsrcType, std::vector<Resource>> pool;
	// rsrcNodeIdx -> rsrc
	std::unordered_map<size_t, Resource> actives;
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
				rsrcMngr.Construct(fg.GetResourceNodes().at(rsrc).Name(), rsrc);

			cout << "[Execute] " << passnodes[i].Name() << endl;

			for (const auto& rsrc : passinfo.destructRsrcs)
				rsrcMngr.Destruct(fg.GetResourceNodes().at(rsrc).Name(), rsrc);
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
		{ depthbuffer,gbuffer1,gbuffer2,gbuffer3 },
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

	cout << "------------------------[pass graph]------------------------" << endl;
	cout << "digraph {" << endl;
	cout << "  node[style=filled fontcolor=white fontname=consolas];" << endl;
	for (const auto& pass : fg.GetPassNodes())
		cout << "  \"" << pass.Name() << "\" [shape = box color=\"#F79646\"];" << endl;
	for (const auto& [idx, info] : crst.rsrc2info)
		cout << "  \"" << fg.GetResourceNodes().at(idx).Name() << "\" [shape = ellipse color=\"#6597AD\"];" << endl;
	for (const auto& pass : fg.GetPassNodes()) {
		for (const auto& input : pass.Inputs())
			cout << "  \"" << fg.GetResourceNodes().at(input).Name() << "\" -> \"" << pass.Name()
			<< "\" [color=\"#9BBB59\"];" << endl;
		for (const auto& output : pass.Outputs())
			cout << "  \"" << pass.Name() << "\" -> \"" << fg.GetResourceNodes().at(output).Name() << "\" [color=\"#B54E4C\"];" << endl;
	}
	cout << "}" << endl;

	cout << "------------------------[Execute]------------------------" << endl;

	ResourceMngr rsrcMngr;

	rsrcMngr
		.RegisterImportedRsrc(finaltarget, { nullptr })
		.RegisterTemporalRsrc(depthbuffer, { 32 })
		.RegisterTemporalRsrc(gbuffer1, { 32 })
		.RegisterTemporalRsrc(gbuffer2, { 32 })
		.RegisterTemporalRsrc(gbuffer3, { 32 })
		.RegisterTemporalRsrc(debugoutput, { 32 })
		.RegisterTemporalRsrc(lightingbuffer, { 32 });

	Executor executor;
	executor.Execute(fg, crst, rsrcMngr);

	return 0;
}
