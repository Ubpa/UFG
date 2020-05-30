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

		ResourceMngr* GetResourceMngr() const noexcept { return rsrcMngr; }
		const std::vector<Pass>& GetPasses() const noexcept { return passes; }
		const CompileResult& GetCompileResult() const noexcept { return compileResult; }
		const std::map<std::string, Resource>& GetImports() const noexcept { return imports; }

		void Import(std::string name, Resource resource) { imports.emplace(std::move(name), resource); }

		void AddPass(Pass pass) { passes.emplace_back(std::move(pass)); }
		void Compile();
		// run after compile
		void Execute();

		void Clear();

	private:
		ResourceMngr* rsrcMngr;
		CompileResult compileResult;
		std::vector<Pass> passes;
		std::map<std::string, Resource> imports;
	};
}
