#include <UFG/FrameGraph.h>

#include <iostream>

using namespace Ubpa;
using namespace std;

// raw type
struct ImgSize {
	size_t width;
	size_t height;
};

// impl type
struct Usage {
	string str;
};

struct Texture {
	size_t n;
};

class TestResourceMngr : public FG::ResourceMngr {
public:
	virtual void Transition(void* raw_ptr, size_t from, size_t to) override {
		cout << "[Transition] " << raw_ptr << ": " << from << "->" << to << endl;
	}

	virtual void* RequestImpl(void* ptr_raw, const void* impl_type) override {
		size_t num;

		auto key = make_tuple(ptr_raw, impl_type);
		auto target = implMap.find(key);
		if (target != implMap.end())
			num = target->second;
		else {
			num = implMap.size();
			implMap[key] = num;
		}

		cout << "[Request Impl] " << reinterpret_cast<const Usage*>(impl_type)->str << " " << num << endl;
		return reinterpret_cast<void*&&>(Texture{ num++ });
	}

protected:
	virtual void* CreateRaw(const void* raw_type, size_t raw_state) override {
		static size_t num = 0;
		cout << "[Create Raw] " << num << endl;
		return reinterpret_cast<void*>(num++);
	}

	std::map<std::tuple<void*, const void*>, size_t> implMap;
};

int main() {
	ImgSize imgsize{ 1024,768 };
	Usage srv{ "srv" };
	Usage rtv{ "rtv" };

	size_t write = 0;
	size_t read = 1;

	FG::Pass depthpass{
		{},
		{{&rtv, write, "Depth Buffer"}},
		[](const map<string, FG::Resource>& resources) {
			cout << "- Depth pass" << endl;
			for (const auto& [name, rsrc] : resources) {
				cout << "  - " << name << endl
					<< "    - raw_ptr : " << rsrc.raw_ptr << endl
					<< "    - impl_ptr : " << rsrc.impl_ptr << endl;
			}
		},
		"Depth pass" };

	FG::Pass gbufferpass{
		{{&srv, read, "Depth Buffer"}},
		{{&rtv, write, "GBuffer1"}, {&rtv, write, "GBuffer2"}, {&rtv, write, "GBuffer3"}},
		[](const map<string, FG::Resource>& resources) {
			cout << "- GBuffer pass" << endl;
			for (const auto& [name, rsrc] : resources) {
				cout << "  - " << name << endl
					<< "    - raw_ptr : " << rsrc.raw_ptr << endl
					<< "    - impl_ptr : " << rsrc.impl_ptr << endl;
			}
		},
		"GBuffer pass" };

	FG::Pass lighting{
		{{&srv, read, "GBuffer1"}, {&srv, read, "GBuffer2"}, {&srv, read, "GBuffer3"}, {&srv, read, "Depth Buffer"}},
		{{&rtv, write, "Lighting Buffer"}},
		[](const map<string, FG::Resource>& resources) {
			cout << "- Lighting" << endl;
			for (const auto& [name, rsrc] : resources) {
				cout << "  - " << name << endl
					<< "    - raw_ptr : " << rsrc.raw_ptr << endl
					<< "    - impl_ptr : " << rsrc.impl_ptr << endl;
			}
		},
		"Lighting" };

	FG::Pass post{
		{{&srv, read, "Lighting Buffer"}},
		{{&rtv, write, "Final Target"}},
		[](const map<string, FG::Resource>& resources) {
			cout << "- Post" << endl;
			for (const auto& [name, rsrc] : resources) {
				cout << "  - " << name << endl
					<< "    - raw_ptr : " << rsrc.raw_ptr << endl
					<< "    - impl_ptr : " << rsrc.impl_ptr << endl;
			}
		},
		"Post" };

	FG::Pass debug{
		{{&srv, read, "GBuffer3"}},
		{{&rtv, write, "Debug Output"}},
		[](const map<string, FG::Resource>& resources) {
			cout << "- Debug View" << endl;
			for (const auto& [name, rsrc] : resources) {
				cout << "  - " << name << endl
					<< "    - raw_ptr : " << rsrc.raw_ptr << endl
					<< "    - impl_ptr : " << rsrc.impl_ptr << endl;
			}
		},
		"Debug View" };

	FG::FrameGraph fg{ new TestResourceMngr };

	fg.AddResource({ &imgsize, "Depth Buffer" });
	fg.AddResource({ &imgsize, "GBuffer1" });
	fg.AddResource({ &imgsize, "GBuffer2" });
	fg.AddResource({ &imgsize, "GBuffer3" });
	fg.AddResource({ &imgsize, "Lighting Buffer" });
	fg.AddResource({ &imgsize, "Debug Output" });

	fg.ImportResource({ reinterpret_cast<void*>(static_cast<size_t>(-1)), read, "Final Target" });

	fg.AddPass(depthpass);
	fg.AddPass(gbufferpass);
	fg.AddPass(lighting);
	fg.AddPass(post);
	fg.AddPass(debug);

	fg.Compile();
	const auto& rst = fg.GetCompileResult();

	cout << "------------------------[pass order]------------------------" << endl;
	for (auto i : rst.sortedPasses)
		cout << i << ": " << fg.GetPasses().at(i).Name() << endl;

	cout << "------------------------[resource info]------------------------" << endl;
	for (const auto& [name, info] : rst.resource2info) {
		cout << "- " << name << endl
			<< "   - writer: " << fg.GetPasses().at(info.writer).Name() << endl;

		if (!info.readers.empty()) {
			cout << "   - readers" << endl;
			for (auto reader : info.readers)
				cout << "     - " << fg.GetPasses().at(reader).Name() << endl;
		}

		if(info.last != static_cast<size_t>(-1))
			cout << "   - last: " << fg.GetPasses().at(info.last).Name() << endl;

		cout << "  - lifetime: " << fg.GetPasses().at(info.writer).Name() << " - ";
		if (info.last != static_cast<size_t>(-1))
			cout << fg.GetPasses().at(info.last).Name();
		cout << endl;
	}

	cout << "------------------------[pass graph]------------------------" << endl;
	cout << "digraph {" << endl;
	cout << "  node[style=filled fontcolor=white fontname=consolas];" << endl;
	for (const auto& pass : fg.GetPasses())
		cout << "  \"" << pass.Name() << "\" [shape = box color=\"#F79646\"];" << endl;
	for (const auto& [name, info] : rst.resource2info) {
		if(fg.GetImports().find(name) != fg.GetImports().end())
			cout << "  \"" << name << "\" [shape = ellipse color=\"#AD534D\"];" << endl;
		else
			cout << "  \"" << name << "\" [shape = ellipse color=\"#6597AD\"];" << endl;
	}
	for (const auto& pass : fg.GetPasses()) {
		for (const auto& input : pass.Inputs())
			cout << "  \"" << input.name << "\" -> \"" << pass.Name()
			<< "\" [color=\"#9BBB59\"];" << endl;
		for (const auto& output : pass.Outputs())
			cout << "  \"" << pass.Name() << "\" -> \"" << output.name << "\" [color=\"#B54E4C\"];" << endl;
	}
	cout << "}" << endl;

	cout << "------------------------[Execute]------------------------" << endl;
	fg.Execute();

	return 0;
}
