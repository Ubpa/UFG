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

	// set every resource's readers and writers
	for (size_t i = 0; i < passes.size(); i++) {
		const auto& pass = passes[i];
		for (const auto& input : pass.Inputs())
			rst.rsrc2info[input].readers.push_back(i);
		for (const auto& output : pass.Outputs()) {
			auto& info = rst.rsrc2info[output];
			if (info.writer != static_cast<size_t>(-1))
				throw std::logic_error("multi writers");
			rst.rsrc2info[output].writer = i;
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

	// set resource read/write orders: unique-write-pass -> multi-read-passes
	for (const auto& [name, info] : rst.rsrc2info) {
		if (info.writer == static_cast<size_t>(-1))
			continue;

		auto& adj = rst.passgraph.adjList[info.writer];
		for (const auto& reader : info.readers)
			adj.insert(reader);
	}

	// set resouce move order
	for (auto [src, dst] : rst.moves_src2dst) {
		// [src]
		//   .
		//   .
		//   v
		// [dst]

		auto info_dst = rst.rsrc2info.at(dst);
		auto info_src = rst.rsrc2info.at(src);

		if (info_dst.writer != static_cast<size_t>(-1)) {
			//           [src]
			//             .
			//             .
			//             v
			// writer -> [dst]

			if (!info_src.readers.empty()) {
				// readers <- [src]
				//    |         .
				//    |         .
				//    v         v
				// writer  -> [dst]

				for (auto reader_src : info_src.readers)
					rst.passgraph.adjList[reader_src].insert(info_dst.writer);
			}
			else if (info_src.writer != static_cast<size_t>(-1)) {
				// writer -> [src]
				//    |        .
				//    |        .
				//    v        v
				// writer -> [dst]

				rst.passgraph.adjList[info_src.writer].insert(info_dst.writer);
			}
			// else [do nothing]
		}
		else if (!info_dst.readers.empty()) {
			//            [src]
			//              .
			//              .
			//              v
			// readers <- [dst]

			if (!info_src.readers.empty()) {
				// readers <- [src]
				//    |         .
				//    |         .
				//    v         v
				// readers <- [dst]

				for (auto reader_src : info_src.readers) {
					rst.passgraph.adjList[reader_src].insert(
						info_dst.readers.begin(),
						info_dst.readers.end()
					);
				}
			}
			else if (info_src.writer != static_cast<size_t>(-1)) {
				// writer  -> [src]
				//    |         .
				//    |         .
				//    v         v
				// readers <- [dst]

				rst.passgraph.adjList[info_src.writer].insert(
					info_dst.readers.begin(),
					info_dst.readers.end()
				);
			}
			// else [[do nothing]];
		}
		// else [[do nothing]];
	}

	auto sortedPasses = rst.passgraph.TopoSort();
	if (!sortedPasses)
		throw std::logic_error("not a DAG");

	// move_src2dst -> move_dst2src
	for (const auto& [src, dst] : rst.moves_src2dst) {
		auto [iter, success] = rst.moves_dst2src.emplace(dst, src);
		assert(success);
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
			auto idx_dst = registry.GetNodeIndex(fg.GetPassNodes()[dst].Name());
			graph.AddEdge(registry.RegisterEdge(idx_src, idx_dst));
		}
	}

	return graph;
}
