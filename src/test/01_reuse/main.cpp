#include <UFG/core/core.h>

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

	void Construct(const string& name, size_t rsrcNodeIndex) {
		Resource rsrc;

		if (IsImported(rsrcNodeIndex)) {
			rsrc = importeds[rsrcNodeIndex];
			cout << "[Construct] Import | " << name << " @" << rsrc.buffer << endl;
		}
		else {
			auto type = temporals[rsrcNodeIndex];
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
		actives[rsrcNodeIndex] = rsrc;
	}

	void Destruct(const string& name, size_t rsrcNodeIndex) {
		auto rsrc = actives[rsrcNodeIndex];
		if (!IsImported(rsrcNodeIndex)) {
			pool[temporals[rsrcNodeIndex]].push_back(actives[rsrcNodeIndex]);
			cout << "[Destruct] Recycle | " << name << " @" << rsrc.buffer << endl;
		}
		else
			cout << "[Destruct] Import | " << name << " @" << rsrc.buffer << endl;

		actives.erase(rsrcNodeIndex);
	}

	ResourceMngr& RegisterImportedRsrc(size_t rsrcNodeIndex, Resource rsrc) {
		importeds[rsrcNodeIndex] = rsrc;
		return *this;
	}

	ResourceMngr& RegisterTemporalRsrc(size_t rsrcNodeIndex, RsrcType type) {
		temporals[rsrcNodeIndex] = type;
		return *this;
	}

	bool IsImported(size_t rsrcNodeIndex) const noexcept {
		return importeds.find(rsrcNodeIndex) != importeds.end();
	}

private:
	// rsrcNodeIndex -> rsrc
	std::unordered_map<size_t, Resource> importeds;
	// rsrcNodeIndex -> type
	std::unordered_map<size_t, RsrcType> temporals;
	// type -> vector<rsrc>
	std::unordered_map<RsrcType, std::vector<Resource>> pool;
	// rsrcNodeIndex -> rsrc
	std::unordered_map<size_t, Resource> actives;
};

class Executor {
public:
	virtual void Execute(
		const UFG::FrameGraph& fg,
		const UFG::Compiler::Result& crst,
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
	UFG::FrameGraph fg("test 00 basic");

	size_t depthbuffer = fg.RegisterResourceNode("Depth Buffer");
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
	fg.RegisterPassNode(
		"GBuffer pass",
		{ depthbuffer },
		{ gbuffer1,gbuffer2,gbuffer3 }
	);
	fg.RegisterPassNode(
		"Lighting",
		{ depthbuffer,gbuffer1,gbuffer2,gbuffer3 },
		{ lightingbuffer }
	);
	fg.RegisterPassNode(
		"Post",
		{ lightingbuffer },
		{ finaltarget }
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

		if (info.inRsrcNodeIdx != static_cast<size_t>(-1))
			cout << "  - in resource node index : " << info.inRsrcNodeIdx << endl;

		if (info.outRsrcNodeIdx != static_cast<size_t>(-1))
			cout << "  - out resource node index : " << info.outRsrcNodeIdx << endl;

		cout << "  - lifetime: " << fg.GetPassNodes().at(crst.sortedPasses[info.first]).Name() << " - "
			<< fg.GetPassNodes().at(crst.sortedPasses[info.last]).Name();
		cout << endl;
	}

	cout << "------------------------[Graphviz]------------------------" << endl;

	auto g = fg.ToGraphvizGraph();
	cout << g.Dump() << endl;

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
