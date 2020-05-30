#include <UFG/FrameGraph.h>

#include <iostream>

using namespace Ubpa;
using namespace std;

struct Type : FG::Resource::Type {
	Type(size_t width, size_t height)
		: width{ width }, height{ height } {}
	size_t width;
	size_t height;
};

int main() {
	Type type{ 1024,768 };
	size_t write = 0;
	size_t read = 1;

	FG::ResourceDecs depthbuffer_write{ &type, "Depth Buffer", write };
	FG::ResourceDecs gbuffer1_write{ &type, "GBuffer1", write };
	FG::ResourceDecs gbuffer2_write{ &type, "GBuffer2", write };
	FG::ResourceDecs gbuffer3_write{ &type, "GBuffer3", write };
	FG::ResourceDecs lightingbuffer_write{ &type, "Lighting Buffer", write };
	FG::ResourceDecs finaltarget_write{ &type, "Final Target", write };

	FG::ResourceDecs depthbuffer_read{ &type, "Depth Buffer", read };
	FG::ResourceDecs gbuffer1_read{ &type, "GBuffer1", read };
	FG::ResourceDecs gbuffer2_read{ &type, "GBuffer2", read };
	FG::ResourceDecs gbuffer3_read{ &type, "GBuffer3", read };
	FG::ResourceDecs lightingbuffer_read{ &type, "Lighting Buffer", read };

	FG::Pass depthpass{
		{},
		{depthbuffer_write},
		[]() {cout << "Depth pass" << endl; },
		"Depth pass" };
	FG::Pass gbufferpass{
		{depthbuffer_read},
		{gbuffer1_write, gbuffer2_write, gbuffer3_write},
		[]() {cout << "GBuffer pass" << endl; },
		"GBuffer pass" };
	FG::Pass lighting{
		{gbuffer1_read, gbuffer2_read, gbuffer3_read, depthbuffer_read},
		{lightingbuffer_write},
		[]() {cout << "Lighting" << endl; },
		"Lighting" };
	FG::Pass post{
		{lightingbuffer_read},
		{finaltarget_write},
		[]() {cout << "Post" << endl; },
		"Post" };

	FG::FrameGraph fg;

	fg.AddPass(depthpass);
	fg.AddPass(gbufferpass);
	fg.AddPass(lighting);
	fg.AddPass(post);

	auto rst = fg.Compile();

	cout << "[pass order]" << endl;
	for (auto i : rst.sortedPasses)
		cout << i << ": " << fg.GetPasses().at(i).Name() << endl;

	cout << "[resource info]" << endl;
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

	cout << "[pass graph]" << endl;
	cout << "digraph {" << endl;
	for (const auto& pass : fg.GetPasses())
		cout << "  \"" << pass.Name() << "\" [shape = box];" << endl;
	for (const auto& [name, info] : rst.resource2info)
		cout << "  \"" << name << "\" [shape = ellipse];" << endl;
	for (const auto& pass : fg.GetPasses()) {
		for (const auto& input : pass.Inputs())
			cout << "  \"" << input.name << "\" -> \"" << pass.Name() << "\" [color = green];" << endl;
		for (const auto& output : pass.Outputs())
			cout << "  \"" << pass.Name() << "\" -> \"" << output.name << "\" [color = red];" << endl;
	}
	cout << "}" << endl;


	return 0;
}
