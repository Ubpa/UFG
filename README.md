# UFG
Ubpa Frame Graph

## Example

```c++
#include <UGraphviz/UGraphviz.h>

#include <iostream>

using namespace Ubpa;

using namespace std;

int main() {
	UGraphviz::Graph graph("hello world", true);

	auto& registry = graph.GetRegistry();

	auto v_a = registry.RegisterNode("a");
	auto v_b = registry.RegisterNode("b");
	auto v_c = registry.RegisterNode("c");
	auto v_d = registry.RegisterNode("d");

	auto e_ab = registry.RegisterEdge(v_a, v_b);
	auto e_ac = registry.RegisterEdge(v_a, v_c);
	auto e_bd = registry.RegisterEdge(v_b, v_d);
	auto e_cd = registry.RegisterEdge(v_c, v_d);

	graph
		.AddEdge(e_ab)
		.AddEdge(e_ac)
		.AddEdge(e_bd)
		.AddEdge(e_cd);

	cout << graph.Dump() << endl;

	return 0;
}
```

