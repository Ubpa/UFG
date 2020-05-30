#pragma once

#include "Resource.h"

#include <unordered_map>

namespace Ubpa::FG {
	class ResourceMngr {
	public:
		Resource Request(const void* type, std::string name);
		void Recycle(const Resource& resource);
	protected:
		virtual void* Create(const void* type) const = 0;
	private:
		std::unordered_map<const void*, std::vector<void*>> frees;
	};
}
