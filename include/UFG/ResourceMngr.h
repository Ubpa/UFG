#pragma once

#include "Resource.h"

#include <unordered_map>

namespace Ubpa::FG {
	class ResourceMngr {
	public:
		Resource Request(const Resource::Type* type, std::string name);
		void Recycle(const Resource& resource);
	protected:
		virtual void* Create(const Resource::Type* type) const = 0;
	private:
		std::unordered_map<const Resource::Type*, std::vector<void*>> frees;
	};
}
