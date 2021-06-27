#include <UFG/UFG.hpp>

#include "ThreadPool.hpp"

#include <iostream>
#include <cassert>
#include <mutex>
#include <functional>
#include <thread>

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
	using State = int;
	using Buffer = float*;
	static constexpr State state_common = 0;
	static constexpr State state_read = 1;
	static constexpr State state_write = 2;

	State state;
	Buffer buffer;

	inline static std::unordered_map<Buffer, State> GPURsrcStateMap;
};

class CommandList {
public:
	void Execute(std::string_view str) {
		commandbuffer.push_back([str]() {
			std::cout << "[Execute   ] " << str << endl;
		});
	};
	void Transition(std::string_view name, float* buffer, Resource::State src, Resource::State dst) {
		commandbuffer.push_back([=]() {
			assert(Resource::GPURsrcStateMap.at(buffer) == src);
			std::cout << "[Transition] " << name << " " << src << " -> " << dst << " @" << buffer << endl;
			Resource::GPURsrcStateMap.at(buffer) = dst;
		});
	};
	
	void Run() {
		for (const auto& cmd : commandbuffer)
			cmd();
		commandbuffer.clear();
	}
private:
	std::vector<std::function<void()>> commandbuffer;
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
			cout << "[Construct ] Import  | " << name << " @" << "(" << rsrc.state << ")" << rsrc.buffer << endl;
		}
		else {
			auto type = temporals[rsrcNodeIndex];
			auto& typefrees = pool[type];
			if (typefrees.empty()) {
				rsrc.state = Resource::state_common;
				rsrc.buffer = new float[type.size];
				cout << "[Construct ] Create  | " << name << " @" << "(" << rsrc.state << ")" << rsrc.buffer << endl;
				Resource::GPURsrcStateMap[rsrc.buffer] = rsrc.state;
			}
			else {
				rsrc = typefrees.back();
				typefrees.pop_back();
				cout << "[Construct ] Reuse   | " << name << " @" << "(" << rsrc.state << ")" << rsrc.buffer << endl;
			}
		}
		actives[rsrcNodeIndex] = rsrc;
	}

	void Move(std::string_view dstName, size_t dstRsrcNodeIndex,
		std::string_view srcName, size_t srcRsrcNodeIndex)
	{
		auto rsrc = actives[srcRsrcNodeIndex];
		cout << "[Move      ] " << dstName << " <- " << srcName << " @" << "(" << rsrc.state << ")" << rsrc.buffer << endl;
		actives[dstRsrcNodeIndex] = actives[srcRsrcNodeIndex];
		actives.erase(srcRsrcNodeIndex);
		if (importeds.contains(srcRsrcNodeIndex))
			importeds[dstRsrcNodeIndex] = importeds[srcRsrcNodeIndex];
		else
			temporals[dstRsrcNodeIndex] = temporals[srcRsrcNodeIndex];
	}

	void Destruct(CommandList& cmdlist, std::string_view name, size_t rsrcNodeIndex) {
		auto rsrc = actives[rsrcNodeIndex];
		if (!IsImported(rsrcNodeIndex)) {
			pool[temporals[rsrcNodeIndex]].push_back(rsrc); // TODO
			cout << "[Destruct  ] Recycle | " << name << " @" << "(" << rsrc.state << ")" << rsrc.buffer << endl;
		}
		else {
			cmdlist.Transition(name, rsrc.buffer, rsrc.state, importeds.at(rsrcNodeIndex).state);
			cout << "[Destruct  ] Import  | " << name << " @" << "(" << rsrc.state << ")" << rsrc.buffer << endl;
		}

		actives.erase(rsrcNodeIndex);
	}

	std::map<size_t, Resource::Buffer> Request(CommandList& cmdlist, const UFG::FrameGraph& fg, size_t passNodeIndex) {
		std::map<size_t, Resource::Buffer> rst;
		auto target = passRsrcStateMap.find(passNodeIndex);
		if (target == passRsrcStateMap.end())
			return rst;
		const auto& rsrcStateMap = target->second;

		for (const auto& [rsrc, state] : rsrcStateMap) {
			auto& active_rsrc = actives.at(rsrc);
			if (active_rsrc.state != state) {
				cmdlist.Transition(fg.GetResourceNodes()[rsrc].Name(), active_rsrc.buffer, active_rsrc.state, state);
				active_rsrc.state = state;
			}
			rst.emplace(rsrc, active_rsrc.buffer);
		}
		return rst;
	}

	ResourceMngr& RegisterImportedRsrc(size_t rsrcNodeIndex, Resource rsrc) {
		importeds[rsrcNodeIndex] = rsrc;
		Resource::GPURsrcStateMap[rsrc.buffer] = rsrc.state;
		return *this;
	}

	ResourceMngr& RegisterTemporalRsrc(size_t rsrcNodeIndex, RsrcType type) {
		temporals[rsrcNodeIndex] = type;
		return *this;
	}

	ResourceMngr& RegisterPassRsrcState(size_t passNodeIndex, size_t rsrcNodeIndex, Resource::State state) {
		passRsrcStateMap[passNodeIndex][rsrcNodeIndex] = state;
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
	// passNodeIndex -> rsrcNodeIndex -> rsrc state
	std::unordered_map<size_t, std::unordered_map<size_t, Resource::State>> passRsrcStateMap;
};

class Executor {
	ThreadPool threadpool;

public:
	Executor() {
		threadpool.Init(std::thread::hardware_concurrency());
	}

	virtual void Execute(
		const UFG::FrameGraph& fg,
		const UFG::Compiler::Result& crst,
		ResourceMngr& rsrcMngr)
	{
		const size_t cmdlist_num = fg.GetPassNodes().size();
		struct CommandListInfo {
			std::mutex mutex_done;
			bool done{ false };
			CommandList cmdlist;
			std::condition_variable cv;
		};
		std::vector<CommandListInfo> cmdlistInfos(cmdlist_num);
		
		if (auto target = crst.pass2info.find(static_cast<size_t>(-1)); target != crst.pass2info.end()) {
			CommandList init_cmdlist;
			const auto& info = target->second;
			for (auto rsrc : info.construct_resources)
				rsrcMngr.Construct(fg.GetResourceNodes()[rsrc].Name(), rsrc);
			for (auto rsrc : info.move_resources) {
				auto src = rsrc;
				auto dst = crst.moves_src2dst.at(src);
				auto src_name = fg.GetResourceNodes()[src].Name();
				auto dst_name = fg.GetResourceNodes()[dst].Name();
				rsrcMngr.Move(dst_name, dst, src_name, src);
			}
			for (auto rsrc : info.destruct_resources)
				rsrcMngr.Destruct(init_cmdlist, fg.GetResourceNodes()[rsrc].Name(), rsrc);
			init_cmdlist.Run();
		}

		for (auto pass : crst.sorted_passes) {
			auto& cmdlistInfo = cmdlistInfos[crst.pass2order[pass]];
			const auto& passInfo = crst.pass2info.at(pass);
			for (auto rsrc : passInfo.construct_resources)
				rsrcMngr.Construct(fg.GetResourceNodes()[rsrc].Name(), rsrc);

			auto passRsrcs = rsrcMngr.Request(cmdlistInfo.cmdlist, fg, pass);
			
			threadpool.Summit([passRsrcs, cmdlist = &cmdlistInfo.cmdlist, name = fg.GetPassNodes()[pass].Name()]() {
				cmdlist->Execute(name);
				// record commands ...
			});

			for(auto rsrc : passInfo.move_resources) {
				auto src = rsrc;
				auto dst = crst.moves_src2dst.at(src);
				auto src_name = fg.GetResourceNodes()[src].Name();
				auto dst_name = fg.GetResourceNodes()[dst].Name();
				rsrcMngr.Move(dst_name, dst, src_name, src);
			}
			for (auto rsrc : passInfo.destruct_resources)
				rsrcMngr.Destruct(cmdlistInfo.cmdlist, fg.GetResourceNodes()[rsrc].Name(), rsrc);

			{ // commit
				std::lock_guard<std::mutex> lk(cmdlistInfo.mutex_done);
				cmdlistInfo.done = true;
				cmdlistInfo.cv.notify_one();
			}
		}

		size_t cnt = 0;
		while (cnt < cmdlist_num) {
			std::unique_lock<std::mutex> lk(cmdlistInfos[cnt].mutex_done);
			if (!cmdlistInfos[cnt].done)
				cmdlistInfos[cnt].cv.wait(lk);
			CommandList cmdlist = std::move(cmdlistInfos[cnt].cmdlist);
			lk.unlock();

			cmdlist.Run();

			++cnt;
		}
	}
};

int main() {
	UFG::FrameGraph fg("test 02 parallel");

	size_t depthbuffer = fg.RegisterResourceNode("Depth Buffer");
	size_t depthbuffer2 = fg.RegisterResourceNode("Depth Buffer 2");
	size_t gbuffer1 = fg.RegisterResourceNode("GBuffer1");
	size_t gbuffer2 = fg.RegisterResourceNode("GBuffer2");
	size_t gbuffer3 = fg.RegisterResourceNode("GBuffer3");
	size_t lightingbuffer = fg.RegisterResourceNode("Lighting Buffer");
	size_t finaltarget = fg.RegisterResourceNode("Final Target");
	size_t debugoutput = fg.RegisterResourceNode("Debug Output");

	size_t depth_pass = fg.RegisterPassNode(
		"Depth pass",
		{},
		{ depthbuffer }
	);
	size_t depth_pass0 = fg.RegisterPassNode(
		"Depth Buffer Read Pass0",
		{ depthbuffer },
		{}
	);
	size_t depth_pass1 = fg.RegisterPassNode(
		"Depth Buffer Read Pass1",
		{ depthbuffer },
		{}
	);
	size_t depth_pass2 = fg.RegisterPassNode(
		"Depth Buffer Read Pass2",
		{ depthbuffer },
		{}
	);
	size_t depth_pass3 = fg.RegisterPassNode(
		"Depth Buffer Read Pass3",
		{ depthbuffer },
		{}
	);
	fg.RegisterMoveNode(depthbuffer2, depthbuffer);
	size_t gbuffer_pass = fg.RegisterPassNode(
		"GBuffer pass",
		{ },
		{ depthbuffer2,gbuffer1,gbuffer2,gbuffer3 }
	);
	size_t lighting_pass = fg.RegisterPassNode(
		"Lighting",
		{ depthbuffer2,gbuffer1,gbuffer2,gbuffer3 },
		{ lightingbuffer }
	);
	size_t post_pass = fg.RegisterPassNode(
		"Post",
		{ lightingbuffer },
		{ finaltarget }
	);
	size_t present_pass = fg.RegisterPassNode(
		"Present",
		{ finaltarget },
		{ }
	);
	size_t debug_pass = fg.RegisterPassNode(
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
		cout << "- " << fg.GetResourceNodes()[idx].Name() << endl
			<< "  - writer: " << fg.GetPassNodes()[info.writer].Name() << endl;

		if (!info.readers.empty()) {
			cout << "  - readers" << endl;
			for (auto reader : info.readers)
				cout << "    * " << fg.GetPassNodes()[reader].Name() << endl;
		}

		cout << "  - lifetime: " << fg.GetPassNodes()[crst.sorted_passes[info.first]].Name() << " - "
			<< fg.GetPassNodes()[crst.sorted_passes[info.last]].Name();

		cout << endl;
	}

	cout << "------------------------[Graphviz]------------------------" << endl;

	auto g = fg.ToGraphvizGraph();
	cout << g.Dump() << endl;

	cout << "------------------------[Graphviz2]------------------------" << endl;

	auto g2 = fg.ToGraphvizGraph2();
	cout << g2.Dump() << endl;

	cout << "------------------------[Execute]------------------------" << endl;

	Executor executor;
	for (size_t i = 0; i < 2; i++) {
		ResourceMngr rsrcMngr;

		rsrcMngr
			.RegisterImportedRsrc(finaltarget, { Resource::state_common, nullptr })

			.RegisterTemporalRsrc(depthbuffer, { 32 })
			.RegisterTemporalRsrc(depthbuffer2, { 32 })
			.RegisterTemporalRsrc(gbuffer1, { 32 })
			.RegisterTemporalRsrc(gbuffer2, { 32 })
			.RegisterTemporalRsrc(gbuffer3, { 32 })
			.RegisterTemporalRsrc(debugoutput, { 32 })
			.RegisterTemporalRsrc(lightingbuffer, { 32 })

			.RegisterPassRsrcState(depth_pass, depthbuffer, Resource::state_write)

			.RegisterPassRsrcState(depth_pass0, depthbuffer, Resource::state_read)
			.RegisterPassRsrcState(depth_pass1, depthbuffer, Resource::state_read)
			.RegisterPassRsrcState(depth_pass2, depthbuffer, Resource::state_read)
			.RegisterPassRsrcState(depth_pass3, depthbuffer, Resource::state_read)

			.RegisterPassRsrcState(gbuffer_pass, depthbuffer2, Resource::state_write)
			.RegisterPassRsrcState(gbuffer_pass, gbuffer1, Resource::state_write)
			.RegisterPassRsrcState(gbuffer_pass, gbuffer2, Resource::state_write)
			.RegisterPassRsrcState(gbuffer_pass, gbuffer3, Resource::state_write)

			.RegisterPassRsrcState(lighting_pass, depthbuffer2, Resource::state_read)
			.RegisterPassRsrcState(lighting_pass, gbuffer1, Resource::state_read)
			.RegisterPassRsrcState(lighting_pass, gbuffer2, Resource::state_read)
			.RegisterPassRsrcState(lighting_pass, gbuffer3, Resource::state_read)
			.RegisterPassRsrcState(lighting_pass, lightingbuffer, Resource::state_write)

			.RegisterPassRsrcState(post_pass, lightingbuffer, Resource::state_read)
			.RegisterPassRsrcState(post_pass, finaltarget, Resource::state_write)

			.RegisterPassRsrcState(present_pass, finaltarget, Resource::state_read)

			.RegisterPassRsrcState(debug_pass, gbuffer3, Resource::state_read)
			.RegisterPassRsrcState(debug_pass, debugoutput, Resource::state_write)
			;

		executor.Execute(fg, crst, rsrcMngr);
	}

	return 0;
}
