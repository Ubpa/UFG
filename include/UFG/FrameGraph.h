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
		const std::map<std::string, ResourceImportView>& GetImports() const noexcept { return imports; }
		const ResourceRawDesc& GetResourceRawDesc(const std::string& name) const noexcept {
			return rsrcRawDescs.find(name)->second;
		}

		void ImportResource(Named<ResourceImportView> rsrc) { imports.emplace(std::move(rsrc.name), rsrc.DeNamed()); }

		void AddResource(Named<ResourceRawDesc> rsrc) {
			rsrcRawDescs[rsrc.name] = rsrc.DeNamed();
		}

		void AddPass(Pass pass) { passes.emplace_back(std::move(pass)); }
		void Compile();
		// run after compile
		void Execute();

		void Clear();

	private:
		ResourceMngr* rsrcMngr;
		CompileResult compileResult;
		std::vector<Pass> passes;
		std::map<std::string, ResourceImportView> imports;
		std::map<std::string, ResourceRawDesc> rsrcRawDescs;
	};
}
