#include <UFG/FrameGraph.h>

#include <iostream>

#include <algorithm>

#include <cassert>

using namespace Ubpa::FG;
using namespace std;

FrameGraph::~FrameGraph() {
	delete rsrcMngr;
}

void FrameGraph::Compile() {
	compileResult.Clear();

	for (size_t i = 0; i < passes.size(); i++) {
		const auto& pass = passes[i];
		for (const auto& input : pass.Inputs())
			compileResult.resource2info[input.name].readers.push_back(i);
		for (const auto& output : pass.Outputs())
			compileResult.resource2info[output.name].writer = i;
	}
	
	for (const auto& [name, info] : compileResult.resource2info) {
		auto& adj = compileResult.passgraph.adjList[info.writer];
		for (const auto& reader : info.readers)
			adj.insert(reader);
	}
	
	auto [success, sortedPasses] = compileResult.passgraph.TopoSort();
	assert(success);

	compileResult.sortedPasses = std::move(sortedPasses);

	std::unordered_map<size_t, size_t> index2order;
	for (size_t i = 0; i < compileResult.sortedPasses.size(); i++)
		index2order.emplace(compileResult.sortedPasses[i], i);
	for (auto& [name, info] : compileResult.resource2info) {
		if (imports.find(name) != imports.end())
			info.last = static_cast<size_t>(-1);
		else if (!info.readers.empty()) {
			size_t last = 0;
			for (const auto& reader : info.readers)
				last = std::max(last, index2order.find(reader)->second);
			info.last = last;
		}
		else
			info.last = info.writer;
	}
}

void FrameGraph::Execute() {
	struct StatedRsrc {
		Resource rsrc;
		size_t raw_state;
		const void* impl_type;
	};
	map<string, StatedRsrc> srsrcMap;
	map<size_t, vector<string>> needRecycle;

	for (const auto& [name, info] : compileResult.resource2info) {
		if (info.last == static_cast<size_t>(-1))
			continue;

		needRecycle[info.last].push_back(name);
	}

	for (auto i : compileResult.sortedPasses) {
		const auto& pass = passes[i];
		
		map<string, Resource> passResources;

		auto findRsrc = [&](const Named<ResourceImplDesc>& desc) -> Resource {
			Resource rsrc;
			auto target = srsrcMap.find(desc.name);
			if (target == srsrcMap.end()) {
				auto import_target = imports.find(desc.name);
				if (import_target != imports.end()) {
					rsrc.raw_ptr = import_target->second.raw_ptr;
					rsrc.impl_ptr = rsrcMngr->RequestImpl(rsrc.raw_ptr, desc.impl_type);
				}
				else
					rsrc = rsrcMngr->Request(GetResourceRawDesc(desc.name).raw_type, desc.raw_state, desc.impl_type);
				srsrcMap[desc.name] = { rsrc, desc.raw_state, desc.impl_type };
			}
			else {
				auto& srsrc = target->second;
				if (srsrc.raw_state != desc.raw_state) {
					rsrcMngr->Transition(srsrc.rsrc.raw_ptr, srsrc.raw_state, desc.raw_state);
					srsrc.raw_state = desc.raw_state;
				}
				if (srsrc.impl_type != desc.impl_type) {
					rsrcMngr->RequestImpl(srsrc.rsrc.raw_ptr, desc.impl_type);
					srsrc.impl_type = desc.impl_type;
				}
				rsrc = srsrc.rsrc;
			}

			return rsrc;
		};

		for (auto input : pass.Inputs())
			passResources[input.name] = findRsrc(input);
		for (auto output : pass.Outputs())
			passResources[output.name] = findRsrc(output);

		pass.Execute(passResources);

		auto target = needRecycle.find(i);
		if (target != needRecycle.end()) {
			for (const auto& name : target->second) {
				const auto& srsrc = srsrcMap[name];
				rsrcMngr->Recycle(GetResourceRawDesc(name).raw_type, srsrc.rsrc.raw_ptr, srsrc.raw_state);
				srsrcMap.erase(name);
			}
		}
	}

	// restore state
	for (const auto& [name, view] : imports)
		rsrcMngr->Transition(view.raw_ptr, srsrcMap[name].raw_state, view.raw_init_state);
}

void FrameGraph::Clear() {
	compileResult.Clear();
	imports.clear();
	passes.clear();
}
