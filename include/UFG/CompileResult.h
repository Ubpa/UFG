#pragma once

#include <vector>
#include <unordered_map>
#include <set>

namespace Ubpa::FG {
	struct CompileResult {
		struct Info {
			// first == writer
			// size_t first; // index in sortedPass
			size_t last; // index in sortedPass

			std::vector<size_t> readers;
			size_t writer;
		};

		struct PassGraph {
			void Clear() { adjList.clear(); }
			std::tuple<bool, std::vector<size_t>> TopoSort() const;
			std::unordered_map<size_t, std::set<size_t>> adjList;
		};

		void Clear();

		std::unordered_map<std::string, Info> resource2info;
		PassGraph passgraph;
		std::vector<size_t> sortedPasses;
	};
}
