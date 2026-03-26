#!/usr/bin/python3

# Analyze graphheap JSON output from lua_graphheap() / lua_graphuserheap().
# Supports multiple output modes:
#   graph   - Graphviz directed graph visualization (default)
#   summary - Text summary of heap contents to stdout

import argparse
import collections
import json
import zlib

from typing import *

import graphviz

FIRST_USER_MEMCAT = 10

_FREE_COLOR = "#AAAAAA"


def _type_color(type_name: str) -> Tuple[str, str]:
    """Derive a stable border color and light fill from a type name via hash.
    Returns (border_hsv, fill_hsv) in graphviz "H,S,V" format."""
    h = zlib.crc32(type_name.encode()) & 0xFFFFFFFF
    hue = (h * 137.508) % 360
    hue_norm = hue / 360
    return (f"{hue_norm:.3f} 0.7 0.8", f"{hue_norm:.3f} 0.08 1.0")


class Node(NamedTuple):
    id: str
    type: str
    name: str
    fixed: bool
    color: str
    memcat: int
    size: int
    type_name: Optional[str] = None
    free: bool = False


class Edge(NamedTuple):
    src: Node
    dst: Node
    name: Optional[str]


def _format_size(size_bytes: int) -> str:
    if size_bytes >= 1024 * 1024:
        return f"{size_bytes / (1024 * 1024):.1f}MB"
    elif size_bytes >= 1024:
        return f"{size_bytes / 1024:.1f}KB"
    return f"{size_bytes}B"


def _node_label(node: Node) -> str:
    """Build a short label for display. Never show raw obj_ IDs."""
    name = node.name
    is_unnamed = name.startswith("obj_")

    type_label = node.type
    if node.type_name:
        type_label = f"{node.type} ({node.type_name})"

    memcat_label = "u" if node.memcat >= FIRST_USER_MEMCAT else str(node.memcat)
    size_str = _format_size(node.size)

    if is_unnamed:
        base = f"({type_label}) [{size_str}]"
    else:
        name = name.rsplit("@", 1)[0]
        if len(name) > 60:
            name = name[:60] + "..."
        base = f"({type_label}) {name} [{size_str}]"

    if node.free:
        base += " (free)"
    return base


def load_graph(path: str) -> Tuple[str, Dict[str, Node], List[Edge]]:
    with open(path, "rb") as f:
        graph_json = json.loads(f.read())

    graph_mode = graph_json.get("mode", "global")

    nodes: Dict[str, Node] = {}
    for node_json in graph_json["nodes"]:
        nodes[node_json["id"]] = Node(**node_json)

    edges: List[Edge] = []
    for edge_json in graph_json["edges"]:
        src = nodes[edge_json["src"]]
        dst = nodes[edge_json["dst"]]
        edges.append(Edge(src=src, dst=dst, name=edge_json["name"]))

    return graph_mode, nodes, edges


class Graph:
    _GC_COLOR_ATTRS: dict = {
        "white": {"style": "filled", "fillcolor": "white", "fontcolor": "black"},
        "gray": {"style": "filled", "fillcolor": "lightgray", "fontcolor": "black"},
        "black": {"style": "filled", "fillcolor": "black", "fontcolor": "white"},
    }

    def __init__(self, nodes: Dict[str, Node], edges: List[Edge]):
        self.nodes = nodes
        self.edges = edges
        self._forward_edges: Dict[Node, Set[Edge]] = collections.defaultdict(set)
        self._reverse_edges: Dict[Node, Set[Edge]] = collections.defaultdict(set)

        for edge in self.edges:
            self._forward_edges[edge.src].add(edge)
            self._reverse_edges[edge.dst].add(edge)

    def generate_graphviz(self) -> graphviz.Digraph:
        graph = graphviz.Digraph(
            "object-references",
            "Object References",
            graph_attr={"rankdir": "TB", "bgcolor": "white", "fontsize": "10"},
            node_attr={"shape": "box", "fontsize": "9", "margin": "0.05,0.02", "style": "filled"},
            edge_attr={"fontsize": "8"},
        )
        for node in self.nodes.values():
            label = _node_label(node)

            if node.free:
                border_color = _FREE_COLOR
                fill_color = "#EEEEEE"
            else:
                border_color, fill_color = _type_color(node.type_name or node.type)

            graph.node(
                node.id,
                label=label,
                color=border_color,
                fillcolor=fill_color,
                penwidth="2" if node.fixed else "1",
            )
        for edge in self.edges:
            constraint = "true"
            if edge.name == "env":
                constraint = "false"

            edge_attrs = {"minlen": "2"}
            edge_name = edge.name or ""
            if edge.name == "metatable":
                edge_attrs["style"] = "dashed"
                edge_attrs["color"] = "gray50"
            elif edge_name.startswith("."):
                edge_attrs["fontcolor"] = "#4466AA"
                edge_attrs["color"] = "#4466AA"

            graph.edge(
                edge.src.id,
                edge.dst.id,
                label=f" {edge_name} " if edge_name else "",
                constraint=constraint,
                **edge_attrs,
            )
        return graph

    def remove_fixed_leaves(self):
        """
        Remove fixed objects that don't participate in a non-fixed reference cycle

        If we're debugging invalid reference cycles, we're unlikely to be interested
        in anything else.
        """
        removed_any = True
        while removed_any:
            removed_any = False
            for node in list(self.nodes.values()):
                if not node.fixed:
                    continue
                if any(e.name != "env" for e in self._forward_edges[node]):
                    continue
                self._remove_node_edges(node)
                del self.nodes[node.id]
                removed_any = True

    def _remove_node_edges(self, node: Node):
        node_reverse = self._reverse_edges[node]
        node_forward = self._forward_edges[node]
        for edge in list(node_reverse):
            self._forward_edges[edge.src].discard(edge)
            node_reverse.discard(edge)
            self.edges.remove(edge)

        assert not node_reverse

        for edge in list(node_forward):
            node_forward.discard(edge)
            self._reverse_edges[edge.dst].discard(edge)
            self.edges.remove(edge)

        assert not node_forward


def mode_graph(graph_mode: str, nodes: Dict[str, Node], edges: List[Edge]):
    graph = Graph(nodes, edges)
    if graph_mode == "global":
        graph.remove_fixed_leaves()
    graph.generate_graphviz().view()


def mode_summary(graph_mode: str, nodes: Dict[str, Node], edges: List[Edge]):
    total_count = len(nodes)
    total_size = sum(n.size for n in nodes.values())

    # Breakdown by type
    by_type: Dict[str, Tuple[int, int]] = collections.defaultdict(lambda: (0, 0))
    for node in nodes.values():
        count, size = by_type[node.type]
        by_type[node.type] = (count + 1, size + node.size)

    # Top-N largest objects
    sorted_nodes = sorted(nodes.values(), key=lambda n: n.size, reverse=True)

    # Free-list stats
    free_count = sum(1 for n in nodes.values() if n.free)
    free_size = sum(n.size for n in nodes.values() if n.free)

    print(f"Graph mode: {graph_mode}")
    print(f"Total: {total_count} objects, {_format_size(total_size)}")
    if graph_mode == "user" and free_count > 0:
        live_size = total_size - free_size
        print(f"  Live: {total_count - free_count} objects, {_format_size(live_size)}")
        print(f"  Free (bytecode constants): {free_count} objects, {_format_size(free_size)}")
    print()

    print("By type:")
    for type_name, (count, size) in sorted(by_type.items(), key=lambda x: x[1][1], reverse=True):
        print(f"  {type_name:12s}  {count:6d} objects  {_format_size(size):>8s}")
    print()

    print("Top 20 largest objects:")
    for node in sorted_nodes[:20]:
        name = node.name
        if len(name) > 60:
            name = name[:60] + "..."
        type_label = node.type
        if node.type_name:
            type_label = f"{node.type} ({node.type_name})"
        free_tag = " (free)" if node.free else ""
        print(f"  {_format_size(node.size):>8s}  ({type_label}) {name}{free_tag}")


def main():
    parser = argparse.ArgumentParser(description="Analyze graphheap JSON output")
    parser.add_argument("input", help="Path to graphheap JSON file")
    parser.add_argument(
        "--mode", "-m",
        choices=["graph", "summary"],
        default="graph",
        help="Output mode (default: graph)",
    )
    args = parser.parse_args()

    graph_mode, nodes, edges = load_graph(args.input)

    if args.mode == "graph":
        mode_graph(graph_mode, nodes, edges)
    elif args.mode == "summary":
        mode_summary(graph_mode, nodes, edges)


if __name__ == "__main__":
    main()
