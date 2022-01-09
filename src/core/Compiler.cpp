#include <UFG/Compiler.hpp>

#include <UFG/ResourceNode.hpp>

#include <algorithm>
#include <stack>
#include <cassert>
#include <stdexcept>

using namespace Ubpa;

using namespace Ubpa::UFG;
using namespace std;

std::optional<std::vector<size_t>> Compiler::Result::PassGraph::TopoSort() const {
	unordered_map<size_t, size_t> in_degree_map;

	for (const auto& [parent, children] : adjList)
		in_degree_map.emplace(parent, 0);

	for (const auto& [parent, children] : adjList) {
		for (const auto& child : children)
			in_degree_map[child] += 1;
	}

	stack<size_t> zero_in_degree_vertices;
	vector<size_t> sorted_vertices;

	for (const auto& [v, d] : in_degree_map) {
		if (d == 0)
			zero_in_degree_vertices.push(v);
	}

	while (!zero_in_degree_vertices.empty()) {
		auto v = zero_in_degree_vertices.top();
		zero_in_degree_vertices.pop();
		sorted_vertices.push_back(v);
		in_degree_map.erase(v);
		for (auto child : adjList.find(v)->second) {
			auto target = in_degree_map.find(child);
			if (target == in_degree_map.end())
				continue;
			target->second--;
			if (target->second == 0)
				zero_in_degree_vertices.push(child);
		}
	}

	if (!in_degree_map.empty())
		return {};

	return sorted_vertices;
}

Compiler::Result Compiler::Compile(const FrameGraph& fg) {
	Result rst;
	auto passes = fg.GetPassNodes();

	// set every resource's readers, writer, copy-in
	for (size_t i = 0; i < passes.size(); i++) {
		const auto& pass = passes[i];
		switch (pass.GetType())
		{
		case PassNode::Type::General: {
			for (const auto& input : pass.Inputs())
				rst.rsrc2info[input].readers.push_back(i);
			for (const auto& output : pass.Outputs()) {
				size_t& writer = rst.rsrc2info[output].writer;
				if (writer != static_cast<size_t>(-1))
					throw std::logic_error("multi writers");
				writer = i;
			}
		} break;
		case PassNode::Type::Copy: {
			for (size_t idx = 0; idx < pass.Inputs().size(); idx++) {
				rst.rsrc2info[pass.Inputs()[idx]].readers.push_back(i);
				size_t& copy_in = rst.rsrc2info[pass.Outputs()[idx]].copy_in;
				if (copy_in != static_cast<size_t>(-1))
					throw std::logic_error("multi copy_ins");
				copy_in = i;
			}
		} break;
		default:
			assert(false); // no entry
			break;
		}
	}

	// set move map
	{
		std::unordered_set<size_t> movedsts;
		for (const auto& moveNode : fg.GetMoveNodes()) {
			auto src = moveNode.GetSourceNodeIndex();
			auto dst = moveNode.GetDestinationNodeIndex();
			if (rst.moves_src2dst.contains(src))
				throw std::logic_error("move out more than once");
			if (movedsts.contains(dst))
				throw std::logic_error("move in more than once");
			rst.moves_src2dst.emplace(src, dst);
			movedsts.insert(dst);
		}
	}

	// set copy map
	{
		std::unordered_set<size_t> copydsts;
		for (const auto& pass : fg.GetPassNodes()) {
			if (pass.GetType() != PassNode::Type::Copy)
				continue;

			for (size_t idx = 0; idx < pass.Inputs().size(); idx++)
			{
				auto src = pass.Inputs()[idx];
				auto dst = pass.Outputs()[idx];
				if (rst.copys_src2dst.contains(src))
					throw std::logic_error("copy out more than once");
				if (copydsts.contains(dst))
					throw std::logic_error("copy in more than once");
				rst.copys_src2dst.emplace(src, dst);
				copydsts.insert(dst);
			}
		}
	}

	// pruning continuous move without reading and writing
	std::set<size_t> deleteMoves;
	auto iter = rst.moves_src2dst.begin();
	while (iter != rst.moves_src2dst.end()) {
		auto& [src, dst] = *iter;
		if (deleteMoves.contains(src)) {
			++iter;
			continue;
		}

		const auto& info = rst.rsrc2info.at(dst);
		auto next = rst.moves_src2dst.find(dst);
		if (info.writer == static_cast<size_t>(-1)
			&& info.readers.empty()
			&& next != rst.moves_src2dst.end())
		{
			dst = next->second;
			deleteMoves.insert(dst);
		}
		else
			++iter;
	}
	for (auto idx : deleteMoves)
		rst.moves_src2dst.erase(idx);

	// init rst.passgraph.adjList
	rst.passgraph.adjList.reserve(passes.size());
	for (size_t i = 0; i < passes.size(); i++)
		rst.passgraph.adjList.try_emplace(i);

	// set resource inner orders
	for (const auto& [name, info] : rst.rsrc2info) {
		// 1. writer -> readers
		if (info.writer != static_cast<size_t>(-1)) {
			auto& adj = rst.passgraph.adjList[info.writer];
			for (const auto& reader : info.readers)
				adj.insert(reader);
		}

		// 2. readers -> copy_in
		if (info.copy_in != static_cast<size_t>(-1)) {
			for (const auto& reader : info.readers) {
				auto& adj = rst.passgraph.adjList[reader];
				adj.insert(info.copy_in);
			}
		}
		
		// 3. writer -> copy_in
		if (info.writer != static_cast<size_t>(-1)
			&& info.readers.empty()
			&& info.copy_in != static_cast<size_t>(-1))
		{
			rst.passgraph.adjList[info.writer].insert(info.copy_in);
		}
	}

	// set resouce move order
	for (const auto& [src, dst] : rst.moves_src2dst) {
		// [src]
		//   .
		//   .
		//   v
		// [dst]

		const auto& info_dst = rst.rsrc2info.at(dst);
		const auto& info_src = rst.rsrc2info.at(src);

		std::span<const size_t> first_accessers_dst;
		std::span<const size_t> final_accessers_src;

		if (info_dst.writer != static_cast<size_t>(-1))
			first_accessers_dst = { &info_dst.writer, 1 };
		else if (!info_dst.readers.empty())
			first_accessers_dst = info_dst.readers;
		else if (info_dst.copy_in != static_cast<size_t>(-1))
			first_accessers_dst = { &info_dst.copy_in, 1 };

		if (info_src.copy_in != static_cast<size_t>(-1))
			final_accessers_src = { &info_src.copy_in, 1 };
		else if (!info_src.readers.empty())
			final_accessers_src = info_src.readers;
		else if (info_src.writer != static_cast<size_t>(-1))
			final_accessers_src = { &info_src.writer, 1 };

		for (const auto& final_accesser : final_accessers_src) {
			rst.passgraph.adjList[final_accesser].insert(
				first_accessers_dst.begin(),
				first_accessers_dst.end()
			);
		}
	}

	{ // toposort
		auto option_sorted_passes = rst.passgraph.TopoSort();
		if (!option_sorted_passes)
			throw std::logic_error("not a DAG");
		rst.sorted_passes = std::move(*option_sorted_passes);
	}

	// moves_src2dst -> moves_dst2src
	for (const auto& [src, dst] : rst.moves_src2dst) {
		auto [iter, success] = rst.moves_dst2src.emplace(dst, src);
		assert(success);
	}

	// copys_src2dst -> copys_dst2src
	for (const auto& [src, dst] : rst.copys_src2dst) {
		auto [iter, success] = rst.copys_dst2src.emplace(dst, src);
		assert(success);
	}

	// set resource's first last and passinfo

	rst.pass2order = vector<size_t>(rst.sorted_passes.size());
	for (size_t i = 0; i < rst.sorted_passes.size(); i++)
		rst.pass2order[rst.sorted_passes[i]] = i;

	for (size_t i = 0; i < passes.size(); ++i)
		rst.pass2info.emplace(i, Result::PassInfo());

	for (auto& [rsrcNodeIdx, info] : rst.rsrc2info) {
		if (info.writer != static_cast<size_t>(-1))
			info.first = rst.pass2order[info.writer];
		else if (!info.readers.empty()) {
			size_t first = static_cast<size_t>(-1); // max size_t
			for (const auto& reader : info.readers)
				first = std::min(first, rst.pass2order[reader]);
			info.first = first;
		}
		else if (info.copy_in != static_cast<size_t>(-1))
			info.first = rst.pass2order[info.copy_in];
		//else [[do nothing]]; // info.first = static_cast<size_t>(-1)

		info.last = info.first;
		if (info.copy_in != static_cast<size_t>(-1))
			info.last = rst.pass2order[info.copy_in];
		else {
			for (const auto& reader : info.readers)
				info.last = std::max(info.last, rst.pass2order[reader]);
		}

		size_t firstPassIdx = info.first != static_cast<size_t>(-1) ? rst.sorted_passes[info.first]
			: static_cast<size_t>(-1);
		size_t lastPassIdx = info.last != static_cast<size_t>(-1) ? rst.sorted_passes[info.last]
			: static_cast<size_t>(-1);
		if (!rst.moves_dst2src.contains(rsrcNodeIdx))
			rst.pass2info[firstPassIdx].construct_resources.push_back(rsrcNodeIdx);
		if (rst.moves_src2dst.contains(rsrcNodeIdx))
			rst.pass2info[lastPassIdx].move_resources.push_back(rsrcNodeIdx);
		else
			rst.pass2info[lastPassIdx].destruct_resources.push_back(rsrcNodeIdx);
	}

	return rst;
}

UGraphviz::Graph Compiler::Result::PassGraph::ToGraphvizGraph(const FrameGraph& fg) const {
	UGraphviz::Graph graph("Compiler Result Pass Graph", true);

	auto& registry = graph.GetRegistry();

	graph
		.RegisterGraphNodeAttr("shape", "ellipse")
		.RegisterGraphNodeAttr("color", "#6597AD")
		.RegisterGraphEdgeAttr("color", "#B54E4C")
		.RegisterGraphNodeAttr("style", "filled")
		.RegisterGraphNodeAttr("fontcolor", "white")
		.RegisterGraphNodeAttr("fontname", "consolas");

	for (const auto& [src, dsts] : adjList)
		graph.AddNode(registry.RegisterNode(std::string{ fg.GetPassNodes()[src].Name() }));

	for (const auto& [src, dsts] : adjList) {
		auto idx_src = registry.GetNodeIndex(std::string{ fg.GetPassNodes()[src].Name() });
		for (auto dst : dsts) {
			auto idx_dst = registry.GetNodeIndex(std::string{ fg.GetPassNodes()[dst].Name() });
			graph.AddEdge(registry.RegisterEdge(idx_src, idx_dst));
		}
	}

	return graph;
}
