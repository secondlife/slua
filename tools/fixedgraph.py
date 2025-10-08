# TODO: This is mostly for debugging, probably not relevant.

import collections
import json
import sys

from typing import *

import graphviz


class Node(NamedTuple):
    id: str
    type: str
    name: str
    fixed: bool
    synthesized: bool
    color: str
    memcat: int


class Edge(NamedTuple):
    src: Node
    dst: Node
    name: Optional[str]


class Graph:
    _COLOR_ATTRS: dict = {
        "white": {"fill": "white", "fontcolor": "black"},
        "gray": {"fill": "lightgray", "fontcolor": "black"},
        "black": {"fill": "black", "fontcolor": "white"},
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
            graph_attr={"rankdir": "LR", "bgcolor": "white"},
            node_attr={"shape": "box"},
        )
        for node in self.nodes.values():
            name = node.name.rsplit("@", 1)[0]
            if len(name) > 100:
                name = name[:100] + "..."

            color = "red"
            if node.synthesized:
                if node.fixed:
                    color = "yellow"
                else:
                    color = "purple"
            elif node.fixed:
                color = "green"

            graph.node(
                node.id,
                label=f"({node.type}) {name} ({node.memcat})",
                # Border color
                color=color,
                # Apply attributes associated with this GC color
                **self._COLOR_ATTRS[node.color],
            )
        for edge in self.edges:
            constraint = True
            if edge.name == "env":
                # env should be ranked higher than the closures referencing it.
                constraint = False

            graph.edge(
                edge.src.id,
                edge.dst.id,
                label=edge.name or "",
                # TODO: add more as it makes sense
                constraint="true" if constraint else "false"
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
                    # Not interested in pruning non-fixed nodes
                    continue
                if any(e.name != "env" for e in self._forward_edges[node]):
                    # If a node has any non-env reference we shouldn't remove it.
                    # We still consider functions with an env reference to be a leaf,
                    # since we assume `safeenv`.
                    continue
                self._remove_node_edges(node)
                del self.nodes[node.id]
                removed_any = True

    def _remove_node_edges(self, node: Node):
        node_reverse = self._reverse_edges[node]
        node_forward = self._forward_edges[node]
        for edge in list(node_reverse):
            # Remove edge pointing to this node
            self._forward_edges[edge.src].remove(edge)
            # Now remove the reverse entry
            node_reverse.remove(edge)
            # Remove the main edge entry
            self.edges.remove(edge)

        assert not node_reverse

        # Remove any forward references stemming from this node
        for edge in list(node_forward):
            # Remove edge stemming from this node
            node_forward.remove(edge)
            # Now remove the reverse entry
            self._reverse_edges[edge.dst].remove(edge)
            # Remove the main edge entry
            self.edges.remove(edge)

        assert not node_forward


def main():
    with open(sys.argv[1], "rb") as f:
        buf = f.read()

    graph_json = json.loads(buf)

    nodes: Dict[str, Node] = {}
    for node_json in graph_json["nodes"]:
        nodes[node_json["id"]] = Node(**node_json)

    edges: List[Edge] = []
    for edge_json in graph_json["edges"]:
        src = nodes[edge_json["src"]]
        dst = nodes[edge_json["dst"]]
        edges.append(Edge(src=src, dst=dst, name=edge_json["name"]))

    graph = Graph(nodes, edges)
    graph.remove_fixed_leaves()
    graph.generate_graphviz().view()


if __name__ == "__main__":
    main()
