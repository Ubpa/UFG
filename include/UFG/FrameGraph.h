#pragma once

#include "Pass.h"
#include "Resource.h"
#include "CompileResult.h"
#include "ResourceMngr.h"

namespace Ubpa::FG {
	class FrameGraph {
	public:
		FrameGraph(ResourceMngr* rsrcMngr) : rsrcMngr{ rsrcMngr } {}
		~FrameGraph();
		void AddPass(Pass pass) { passes.emplace_back(std::move(pass)); }
		const std::vector<Pass>& GetPasses() const noexcept { return passes; }
		const CompileResult& Compile();
		// run after compile
		void Execute();
	private:
		ResourceMngr* rsrcMngr;
		CompileResult compileResult;
		std::vector<Pass> passes;
	};
}
