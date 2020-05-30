#include <UFG/ResourceMngr.h>

using namespace Ubpa;
using namespace Ubpa::FG;
using namespace std;

Resource ResourceMngr::Request(const void* type, string name) {
	auto& ptrs = frees[type];

	void* ptr;

	if (ptrs.empty())
		ptr = Create(type);
	else {
		ptr = ptrs.back();
		ptrs.pop_back();
	}

	return { type, ptr, std::move(name) };
}

void ResourceMngr::Recycle(const Resource& resource) {
	frees[resource.GetType()].push_back(resource.GetPtr());
}
