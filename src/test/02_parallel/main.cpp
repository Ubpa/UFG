#include <UFG/UFG.hpp>

#include <iostream>
#include <cassert>
#include <mutex>
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

class JobQueue {
public:
	JobQueue(const UFG::Compiler::Result::PassGraph* graph) : graph{ graph } {}

	void Init() {
		in_degree_map.clear();
		zero_in_degree_vertices.clear();

		cnt = graph->adjList.size();

		for (const auto& [parent, children] : graph->adjList)
			in_degree_map.emplace(parent, 0);

		for (const auto& [parent, children] : graph->adjList) {
			for (const auto& child : children)
				in_degree_map[child] += 1;
		}

		for (const auto& [v, d] : in_degree_map) {
			if (d == 0)
				zero_in_degree_vertices.push_back(v);
		}
	}

	bool IsDone() const noexcept { return cnt == 0; }

	size_t Request() {
		if (zero_in_degree_vertices.empty())
			return static_cast<size_t>(-1);

		size_t idx = zero_in_degree_vertices.back();
		zero_in_degree_vertices.pop_back();
		in_degree_map.erase(idx);
		--cnt;

		return idx;
	}

	void Complete(size_t idx) {
		for (auto child : graph->adjList.at(idx)) {
			auto& degree = in_degree_map.at(child);
			--degree;
			if (degree == 0)
				zero_in_degree_vertices.push_back(child);
		}
	}

private:
	std::unordered_map<size_t, size_t> in_degree_map;
	std::vector<size_t> zero_in_degree_vertices;
	size_t cnt{ 0 };
	const UFG::Compiler::Result::PassGraph* graph;
};

class Executor {
public:
	virtual void Execute(
		const UFG::FrameGraph& fg,
		const UFG::Compiler::Result& crst,
		ResourceMngr& rsrcMngr)
	{
		std::map<size_t, size_t> remain_user_cnt_map; // resource idx -> user cnt

		JobQueue job_queue(&crst.passgraph);
		job_queue.Init();

		std::mutex mutex_rsrc;
		std::mutex mutex_job_queue;
		std::condition_variable cv;

		for (const auto& [rsrc, info] : crst.rsrc2info) {
			size_t cnt = info.readers.size();
			if (info.writer != static_cast<size_t>(-1))
				++cnt;
			remain_user_cnt_map.emplace(rsrc, cnt);
		}

		const size_t N = std::thread::hardware_concurrency();
		auto work = [&]() {
			while (true) {
				size_t pass;

				// request pass

				{ // thread-safe
					std::unique_lock<std::mutex> lk(mutex_job_queue);
					while (true) {
						if (job_queue.IsDone()) {
							cv.notify_one();
							return;
						}

						pass = job_queue.Request();
						if (pass == static_cast<size_t>(-1))
							cv.wait(lk);
						else
							break;
					}
				}
				cv.notify_one();

				// construct writed resources
				{ // thread-safe
					std::lock_guard<std::mutex> guard(mutex_rsrc);
					for (auto output : fg.GetPassNodes()[pass].Outputs()) {
						if (crst.moves_dst2src.contains(output))
							continue;

						rsrcMngr.Construct(fg.GetResourceNodes()[output].Name(), output);
					}
				}

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

				{ // thread-safe

					std::lock_guard<std::mutex> guard(mutex_rsrc);
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

				// complete pass
				{ // thread-safe
					std::lock_guard<std::mutex> guard(mutex_job_queue);
					job_queue.Complete(pass);
				}
				cv.notify_one();
			}
		};

		std::vector<std::thread> workers;
		for (size_t i = 0; i < N; i++)
			workers.emplace_back(work);

		for (auto& worker : workers)
			worker.join();
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
	size_t depthbuffer_reader0 = fg.RegisterResourceNode("Depth Buffer Read Pass0");
	size_t depthbuffer_reader1 = fg.RegisterResourceNode("Depth Buffer Read Pass1");
	size_t depthbuffer_reader2 = fg.RegisterResourceNode("Depth Buffer Read Pass2");
	size_t depthbuffer_reader3 = fg.RegisterResourceNode("Depth Buffer Read Pass3");

	fg.RegisterPassNode(
		"Depth pass",
		{},
		{ depthbuffer }
	);
	fg.RegisterPassNode(
		"Depth Buffer Read Pass0",
		{ depthbuffer },
		{}
	);
	fg.RegisterPassNode(
		"Depth Buffer Read Pass1",
		{ depthbuffer },
		{}
	);
	fg.RegisterPassNode(
		"Depth Buffer Read Pass2",
		{ depthbuffer },
		{}
	);
	fg.RegisterPassNode(
		"Depth Buffer Read Pass3",
		{ depthbuffer },
		{}
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
		.RegisterTemporalRsrc(depthbuffer2, { 32 })
		.RegisterTemporalRsrc(gbuffer1, { 32 })
		.RegisterTemporalRsrc(gbuffer2, { 32 })
		.RegisterTemporalRsrc(gbuffer3, { 32 })
		.RegisterTemporalRsrc(debugoutput, { 32 })
		.RegisterTemporalRsrc(lightingbuffer, { 32 });

	Executor executor;
	executor.Execute(fg, crst, rsrcMngr);

	return 0;
}
