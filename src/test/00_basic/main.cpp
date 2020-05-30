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

	fg.Compile();

	return 0;
}
