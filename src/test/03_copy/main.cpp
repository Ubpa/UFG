#include <UFG/UFG.hpp>

#include <iostream>
#include <cassert>

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

	void Construct(std::string_view name, size_t rsrcNodeIndex) {
		Resource rsrc;

		if (IsImported(rsrcNodeIndex)) {
			rsrc = importeds[rsrcNodeIndex];
			cout << "[Construct] Import  | " << name << " @" << rsrc.buffer << endl;
		}
		else {
			auto type = temporals[rsrcNodeIndex];
			auto& typefrees = pool[type];
			if (typefrees.empty()) {
				rsrc.buffer = new float[type.size];
				cout << "[Construct] Create  | " << name << " @" << rsrc.buffer << endl;
			}
			else {
				rsrc = typefrees.back();
				typefrees.pop_back();
				cout << "[Construct] Reuse   | " << name << " @" << rsrc.buffer << endl;
			}
		}
		actives[rsrcNodeIndex] = rsrc;
	}

	void Move(std::string_view dstName, size_t dstRsrcNodeIndex,
		std::string_view srcName, size_t srcRsrcNodeIndex)
	{
		cout << "[Move]      " << dstName << " <- " << srcName << " @" << actives[srcRsrcNodeIndex].buffer << endl;
		actives[dstRsrcNodeIndex] = actives[srcRsrcNodeIndex];
		actives.erase(srcRsrcNodeIndex);
	}

	void Destruct(std::string_view name, size_t rsrcNodeIndex) {
		auto rsrc = actives[rsrcNodeIndex];
		if (!IsImported(rsrcNodeIndex)) {
			pool[temporals[rsrcNodeIndex]].push_back(actives[rsrcNodeIndex]);
			cout << "[Destruct]  Recycle | " << name << " @" << rsrc.buffer << endl;
		}
		else
			cout << "[Destruct]  Import  | " << name << " @" << rsrc.buffer << endl;

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
		std::map<size_t, size_t> remain_user_cnt_map; // resource idx -> user cnt
		for (const auto& [rsrc, info] : crst.rsrc2info) {
			size_t cnt = info.readers.size();
			if (info.writer != static_cast<size_t>(-1))
				++cnt;
			if (info.copy_in != static_cast<size_t>(-1))
				++cnt;
			remain_user_cnt_map.emplace(rsrc, cnt);
		}

		for (auto pass : crst.sorted_passes) {
			// construct writed resources
			
			for (auto rsrc : crst.pass2info.at(pass).construct_resources)
				rsrcMngr.Construct(fg.GetResourceNodes()[rsrc].Name(), rsrc);

			// execute
			cout << "[Execute]   " << fg.GetPassNodes()[pass].Name() << endl;

			// count down users

			auto destruct_or_move_resouce = [&](size_t rsrc) {
				if (auto target = crst.moves_src2dst.find(rsrc); target != crst.moves_src2dst.end()) {
					auto src = rsrc;
					auto dst = target->second;
					auto src_name = fg.GetResourceNodes()[src].Name();
					auto dst_name = fg.GetResourceNodes()[dst].Name();
					rsrcMngr.Move(dst_name, dst, src_name, src);
				}
				else
					rsrcMngr.Destruct(fg.GetResourceNodes()[rsrc].Name(), rsrc);
			};

			for (auto input : fg.GetPassNodes()[pass].Inputs()) {
				auto& cnt = remain_user_cnt_map[input];
				--cnt;
				if (cnt == 0)
					destruct_or_move_resouce(input);
			}
			for (auto output : fg.GetPassNodes()[pass].Outputs()) {
				auto& cnt = remain_user_cnt_map[output];
				--cnt;
				if (cnt == 0)
					destruct_or_move_resouce(output);
			}
		}
	}
};

int main() {
	UFG::FrameGraph fg("test 01 reuse");

	size_t depthbuffer = fg.RegisterResourceNode("Depth Buffer");
	size_t depthbuffer2 = fg.RegisterResourceNode("Depth Buffer 2");
	size_t gbuffer1 = fg.RegisterResourceNode("GBuffer1");
	size_t gbuffer2 = fg.RegisterResourceNode("GBuffer2");
	size_t gbuffer3 = fg.RegisterResourceNode("GBuffer3");
	size_t lightingbuffer = fg.RegisterResourceNode("Lighting Buffer");
	size_t prevacclightingbuffer = fg.RegisterResourceNode("Prev Acc Lighting Buffer");
	size_t acclightingbuffer = fg.RegisterResourceNode("Acc Lighting Buffer");
	size_t finaltarget = fg.RegisterResourceNode("Final Target");
	size_t debugoutput = fg.RegisterResourceNode("Debug Output");

	fg.RegisterGeneralPassNode(
		"Depth pass",
		{},
		{ depthbuffer }
	);
	fg.RegisterMoveNode(depthbuffer2, depthbuffer);
	fg.RegisterGeneralPassNode(
		"GBuffer pass",
		{ },
		{ depthbuffer2,gbuffer1,gbuffer2,gbuffer3 }
	);
	fg.RegisterGeneralPassNode(
		"Lighting",
		{ depthbuffer2,gbuffer1,gbuffer2,gbuffer3 },
		{ lightingbuffer }
	);
	fg.RegisterGeneralPassNode(
		"TAA",
		{ prevacclightingbuffer,lightingbuffer },
		{ acclightingbuffer }
	);
	fg.RegisterCopyPassNode(
		{ acclightingbuffer },
		{ prevacclightingbuffer }
	);
	fg.RegisterGeneralPassNode(
		"Post",
		{ acclightingbuffer },
		{ finaltarget }
	);
	fg.RegisterGeneralPassNode(
		"Present",
		{ finaltarget },
		{ }
	);
	fg.RegisterGeneralPassNode(
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

	cout << "------------------------[pass graph]------------------------" << endl;
	cout << crst.passgraph.ToGraphvizGraph(fg).Dump() << endl;

	cout << "------------------------[resource info]------------------------" << endl;
	for (const auto& [idx, info] : crst.rsrc2info) {
		cout << "- " << fg.GetResourceNodes()[idx].Name() << endl;

		if (info.writer != static_cast<size_t>(-1))
			cout << "  - writer: " << fg.GetPassNodes()[info.writer].Name() << endl;

		if (!info.readers.empty()) {
			cout << "  - readers" << endl;
			for (auto reader : info.readers)
				cout << "    * " << fg.GetPassNodes()[reader].Name() << endl;
		}

		if (info.copy_in != static_cast<size_t>(-1))
			cout << "  - copy-in:" << fg.GetPassNodes()[info.copy_in].Name() << endl;

		cout << "  - lifetime: " << fg.GetPassNodes()[crst.sorted_passes[info.first]].Name()
			<< " - " << fg.GetPassNodes()[crst.sorted_passes[info.last]].Name();

		cout << endl;
	}

	cout << "------------------------[Graphviz]------------------------" << endl;

	auto g = fg.ToGraphvizGraph();
	cout << g.Dump() << endl;

	cout << "------------------------[Graphviz2]------------------------" << endl;

	auto g2 = fg.ToGraphvizGraph2();
	cout << g2.Dump() << endl;

	cout << "------------------------[Execute]------------------------" << endl;

	ResourceMngr rsrcMngr;

	rsrcMngr
		.RegisterImportedRsrc(finaltarget, { (float*)0 })
		.RegisterImportedRsrc(prevacclightingbuffer, { (float*)1 })

		.RegisterTemporalRsrc(depthbuffer, { 32 })
		.RegisterTemporalRsrc(depthbuffer2, { 32 })
		.RegisterTemporalRsrc(gbuffer1, { 32 })
		.RegisterTemporalRsrc(gbuffer2, { 32 })
		.RegisterTemporalRsrc(gbuffer3, { 32 })
		.RegisterTemporalRsrc(debugoutput, { 32 })
		.RegisterTemporalRsrc(lightingbuffer, { 32 })
		.RegisterTemporalRsrc(acclightingbuffer, { 32 });

	Executor executor;
	executor.Execute(fg, crst, rsrcMngr);

	return 0;
}
