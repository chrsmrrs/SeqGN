

#include "AuxiliaryMethods.h"
#include "GenerateTwo.h"

namespace GenerateTwo {
    GenerateTwo::GenerateTwo(const GraphDatabase &graph_database) : m_graph_database(
            graph_database),
                                                                    m_label_to_index(),
                                                                    m_num_labels(0) {}


    GramMatrix
    GenerateTwo::compute_gram_matrix(const uint num_iterations, const bool use_labels, const bool use_edge_labels,
                                     const string algorithm, const bool compute_gram) {
        vector<ColorCounter> color_counters;
        color_counters.reserve(m_graph_database.size());

        // Compute labels for each graph in graph database.
        for (Graph &graph: m_graph_database) {
            color_counters.push_back(compute_colors(graph, num_iterations, use_labels, use_edge_labels, algorithm));
        }

        size_t num_graphs = m_graph_database.size();
        vector<S> nonzero_compenents;

        // Compute feature vectors.
        ColorCounter c;
        for (Node i = 0; i < num_graphs; ++i) {
            c = color_counters[i];

            for (const auto &j: c) {
                Label key = j.first;
                uint value = j.second;
                uint index = m_label_to_index.find(key)->second;
                nonzero_compenents.push_back(S(i, index, value));
            }
        }

        // Compute Gram matrix or feature vectore
        GramMatrix feature_vectors(num_graphs, m_num_labels);
        feature_vectors.setFromTriplets(nonzero_compenents.begin(), nonzero_compenents.end());

        if (not compute_gram) {
            return feature_vectors;
        } else {
            GramMatrix gram_matrix(num_graphs, num_graphs);
            gram_matrix = feature_vectors * feature_vectors.transpose();

            return gram_matrix;
        }
    }


    ColorCounter GenerateTwo::compute_colors(const Graph &g, const uint num_iterations, const bool use_labels,
                                             const bool use_edge_labels,
                                             const string algorithm) {

        Graph tuple_graph(false);
        if (algorithm == "local" or algorithm == "localp") {
            tuple_graph = generate_local_graph(g, use_labels, use_edge_labels);
        } else if (algorithm == "local1" or algorithm == "local1p") {
            tuple_graph = generate_local_graph_1(g, use_labels, use_edge_labels);
        } else if (algorithm == "wl") {
            tuple_graph = generate_global_graph(g, use_labels, use_edge_labels);
        } else if (algorithm == "malkin") {
            tuple_graph = generate_global_graph_malkin(g, use_labels, use_edge_labels);
        }

        unordered_map<Node, TwoTuple> node_to_two_tuple;
        if (algorithm == "localp" or algorithm == "local1p") {
            node_to_two_tuple = tuple_graph.get_node_to_two_tuple();
        }

        size_t num_nodes = tuple_graph.get_num_nodes();

        Labels coloring;

        ColorCounter color_map_1;
        ColorCounter color_map_2;

        Labels coloring_temp;

        coloring.reserve(num_nodes);
        coloring_temp.reserve(num_nodes);
        coloring = tuple_graph.get_labels();
        coloring_temp = coloring;

        EdgeLabels edge_labels = tuple_graph.get_edge_labels();
        EdgeLabels local = tuple_graph.get_local();

        ColorCounter color_map;
        unordered_map<Label, bool> check_1;
        unordered_map<Label, bool> check_2;

        if ((algorithm == "localp" or algorithm == "local1p") and num_iterations == 0) {
            for (Node v = 0; v < num_nodes; ++v) {
                Nodes neighbors(tuple_graph.get_neighbours(v));

                for (const Node &n: neighbors) {
                    const auto t = edge_labels.find(make_tuple(v, n));

                    TwoTuple p = node_to_two_tuple.find(n)->second;
                    Node a = std::get<0>(p);
                    Node b = std::get<1>(p);

                    if (t->second == 1) {
                        Label l = b;
                        l = AuxiliaryMethods::pairing(l, 1);
                        l = AuxiliaryMethods::pairing(l, coloring[n]);

                        Label e = a;
                        e = AuxiliaryMethods::pairing(e, b);
                        e = AuxiliaryMethods::pairing(e, 1);
                        const auto is = check_1.find(e);

                        if (is == check_1.end()) {
                            const auto it = color_map_1.find(l);

                            if (it == color_map_1.end()) {
                                color_map_1.insert({{l, 1}});
                            } else {
                                it->second++;
                            }

                            check_1.insert({{e, true}});
                        }
                    }

                    if (t->second == 2) {
                        Label l = a;
                        l = AuxiliaryMethods::pairing(l, 2);
                        l = AuxiliaryMethods::pairing(l, coloring[n]);

                        Label e = a;
                        e = AuxiliaryMethods::pairing(e, b);
                        e = AuxiliaryMethods::pairing(e, 2);
                        const auto is = check_2.find(e);

                        if (is == check_2.end()) {
                            const auto it = color_map_2.find(l);

                            if (it == color_map_2.end()) {
                                color_map_2.insert({{l, 1}});
                            } else {
                                it->second++;
                            }

                            check_2.insert({{e, true}});
                        }
                    }
                }
            }
        }



        // Iteration 0.
        for (Node v = 0; v < num_nodes; ++v) {
            Label new_color = coloring[v];

            if ((algorithm == "localp" or algorithm == "local1p") and num_iterations == 0) {
                new_color = AuxiliaryMethods::pairing(coloring[v], color_map_1.find(coloring[v])->second);
                new_color = AuxiliaryMethods::pairing(new_color, color_map_2.find(coloring[v])->second);
            }


            ColorCounter::iterator it(color_map.find(new_color));
            if (it == color_map.end()) {
                color_map.insert({{new_color, 1}});
                m_label_to_index.insert({{new_color, m_num_labels}});
                m_num_labels++;
            } else {
                it->second++;
            }
        }

        //size_t tmp = color_map.size();;
        uint h = 1;
        while (h <= num_iterations) {
            // Iterate over all nodes.
            for (Node v = 0; v < num_nodes; ++v) {
                Labels colors_local;
                Labels colors_global;
                Nodes neighbors(tuple_graph.get_neighbours(v));
                colors_local.reserve(neighbors.size() + 1);
                colors_global.reserve(neighbors.size() + 1);

                // New color of node v.
                Label new_color;

                vector<vector<Label>> set_m_local;
                vector<vector<Label>> set_m_global;

                set_m_local.push_back(vector<Label>());
                set_m_local.push_back(vector<Label>());

                set_m_global.push_back(vector<Label>());
                set_m_global.push_back(vector<Label>());

                // Get colors of neighbors.
                for (const Node &n: neighbors) {
                    const auto type = local.find(make_tuple(v, n));
                    const auto label = edge_labels.find(make_tuple(v, n))->second;

                    // Local neighbor.
                    if (type->second == 1) {
                        if (label == 1) {
                            if ((algorithm == "localp" or algorithm == "local1p") and num_iterations == h) {
                                set_m_local[0].push_back(
                                        AuxiliaryMethods::pairing(coloring[n], color_map_1.find(coloring[n])->second));
                            } else {
                                set_m_local[0].push_back(coloring[n]);
                            }
                        }
                        if (label == 2) {
                            if ((algorithm == "localp" or algorithm == "local1p") and num_iterations == h) {
                                set_m_local[1].push_back(
                                        AuxiliaryMethods::pairing(coloring[n], color_map_2.find(coloring[n])->second));
                            } else {
                                set_m_local[1].push_back(coloring[n]);
                            }
                        }
                    } else {  // Global neighbor.
                        if (label == 1) {
                            set_m_global[0].push_back(coloring[n]);
                        }
                        if (label == 2) {
                            set_m_global[1].push_back(coloring[n]);
                        }
                    }
                }

                for (auto &m: set_m_local) {
                    if (m.size() != 0) {
                        sort(m.begin(), m.end());
                        new_color = m.back();
                        m.pop_back();
                        for (const Label &c: m) {
                            new_color = AuxiliaryMethods::pairing(new_color, c);
                        }
                        colors_local.push_back(new_color);
                    }
                }
                sort(colors_local.begin(), colors_local.end());

                for (auto &m: set_m_global) {
                    if (m.size() != 0) {
                        sort(m.begin(), m.end());
                        new_color = m.back();
                        m.pop_back();
                        for (const Label &c: m) {
                            new_color = AuxiliaryMethods::pairing(new_color, c);
                        }
                        colors_global.push_back(new_color);
                    }
                }
                sort(colors_global.begin(), colors_global.end());

                for (auto &c: colors_global) {
                    colors_local.push_back(c);
                }


                colors_local.push_back(coloring[v]);

                // Compute new label using composition of pairing function of Matthew Szudzik to map two integers to on integer.
                new_color = colors_local.back();
                colors_local.pop_back();

                for (const Label &c: colors_local) {
                    new_color = AuxiliaryMethods::pairing(new_color, c);
                }
                coloring_temp[v] = new_color;

                // Keep track how often "new_label" occurs.
                auto it(color_map.find(new_color));
                if (it == color_map.end()) {
                    color_map.insert({{new_color, 1}});
                    m_label_to_index.insert({{new_color, m_num_labels}});
                    m_num_labels++;
                } else {
                    it->second++;
                }

            }
//            cout << color_map.size()-tmp << endl;
//            tmp  = color_map.size();

            // Assign new colors.
            coloring = coloring_temp;
            h++;

            unordered_map<Label, bool> check_1;
            unordered_map<Label, bool> check_2;

            if ((algorithm == "localp" or algorithm == "local1p") and num_iterations == h) {

                for (Node v = 0; v < num_nodes; ++v) {
                    Nodes neighbors(tuple_graph.get_neighbours(v));

                    for (const Node &n: neighbors) {
                        const auto t = edge_labels.find(make_tuple(v, n));

                        TwoTuple p = node_to_two_tuple.find(n)->second;
                        Node a = std::get<0>(p);
                        Node b = std::get<1>(p);

                        if (t->second == 1) {
                            Label l = b;
                            l = AuxiliaryMethods::pairing(l, 1);
                            l = AuxiliaryMethods::pairing(l, coloring[n]);

                            Label e = a;
                            e = AuxiliaryMethods::pairing(e, b);
                            e = AuxiliaryMethods::pairing(e, 1);
                            const auto is = check_1.find(e);

                            if (is == check_1.end()) {
                                const auto it = color_map_1.find(l);

                                if (it == color_map_1.end()) {
                                    color_map_1.insert({{l, 1}});
                                } else {
                                    it->second++;
                                }

                                check_1.insert({{e, true}});
                            }
                        }

                        if (t->second == 2) {
                            Label l = a;

                            l = AuxiliaryMethods::pairing(l, 2);
                            l = AuxiliaryMethods::pairing(l, coloring[n]);

                            Label e = a;
                            e = AuxiliaryMethods::pairing(e, b);
                            e = AuxiliaryMethods::pairing(e, 2);
                            const auto is = check_2.find(e);

                            if (is == check_2.end()) {
                                const auto it = color_map_2.find(l);

                                if (it == color_map_2.end()) {
                                    color_map_2.insert({{l, 1}});
                                } else {
                                    it->second++;
                                }

                                check_2.insert({{e, true}});
                            }
                        }
                    }
                }
            }
        }

        return color_map;
    }

    Graph GenerateTwo::generate_local_graph(const Graph &g, const bool use_labels, const bool use_edge_labels) {
        size_t num_nodes = g.get_num_nodes();
        // New graph to be generated.
        Graph two_tuple_graph(false);

        // Maps node in two set graph to corresponding two tuple.
        unordered_map<Node, TwoTuple> node_to_two_tuple;
        // Inverse of the above map.
        unordered_map<TwoTuple, Node> two_tuple_to_node;
        // Manages edges labels.
        unordered_map<Edge, uint> edge_type;
        // Manages vertex ids
        unordered_map<Edge, uint> vertex_id;
        unordered_map<Edge, uint> local;

        // Create a node for each two set.
        Labels labels;
        Labels tuple_labels;
        if (use_labels) {
            labels = g.get_labels();
        }

        EdgeLabels edge_labels;
        if (use_edge_labels) {
            edge_labels = g.get_edge_labels();
        }

        Node num_two_tuples = 0;
        for (Node i = 0; i < num_nodes; ++i) {
            for (Node j = 0; j < num_nodes; ++j) {
                two_tuple_graph.add_node();

                // Map each pair to node in two set graph and also inverse.
                node_to_two_tuple.insert({{num_two_tuples, make_tuple(i, j)}});
                two_tuple_to_node.insert({{make_tuple(i, j), num_two_tuples}});
                num_two_tuples++;

                Label c_i = 1;
                Label c_j = 2;
                if (use_labels) {
                    c_i = AuxiliaryMethods::pairing(labels[i] + 1, c_i);
                    c_j = AuxiliaryMethods::pairing(labels[j] + 1, c_j);
                }

                Label c;
                if (g.has_edge(i, j)) {
                    if (use_edge_labels) {
                        auto s = edge_labels.find(make_tuple(i, j));
                        c = AuxiliaryMethods::pairing(3, s->second);
                    } else {
                        c = 3;
                    }
                } else if (i == j) {
                    c = 1;
                } else {
                    c = 2;
                }

                Label new_color = AuxiliaryMethods::pairing(AuxiliaryMethods::pairing(c_i, c_j), c);
                tuple_labels.push_back(new_color);
            }
        }


        for (Node i = 0; i < num_two_tuples; ++i) {
            // Get nodes of original graph corresponding to two tuple i.
            TwoTuple p = node_to_two_tuple.find(i)->second;
            Node v = std::get<0>(p);
            Node w = std::get<1>(p);

            // Exchange first node.
            Nodes v_neighbors = g.get_neighbours(v);
            for (Node v_n: v_neighbors) {
                unordered_map<TwoTuple, Node>::const_iterator t = two_tuple_to_node.find(make_tuple(v_n, w));
                two_tuple_graph.add_edge(i, t->second);
                edge_type.insert({{make_tuple(i, t->second), 1}});
                vertex_id.insert({{make_tuple(i, t->second), v_n}});
                local.insert({{make_tuple(i, t->second), 1}});
            }

            // Exchange second node.
            Nodes w_neighbors = g.get_neighbours(w);
            for (Node w_n: w_neighbors) {
                unordered_map<TwoTuple, Node>::const_iterator t = two_tuple_to_node.find(make_tuple(v, w_n));
                two_tuple_graph.add_edge(i, t->second);
                edge_type.insert({{make_tuple(i, t->second), 2}});
                vertex_id.insert({{make_tuple(i, t->second), w_n}});
                local.insert({{make_tuple(i, t->second), 1}});
            }
        }

        two_tuple_graph.set_edge_labels(edge_type);
        two_tuple_graph.set_labels(tuple_labels);
        two_tuple_graph.set_vertex_id(vertex_id);
        two_tuple_graph.set_local(local);
        two_tuple_graph.set_node_to_two_tuple(node_to_two_tuple);

        return two_tuple_graph;
    }

    Graph GenerateTwo::generate_local_graph_1(const Graph &g, const bool use_labels, const bool use_edge_labels) {
        size_t num_nodes = g.get_num_nodes();
        // New graph to be generated.
        Graph two_tuple_graph(false);

        // Maps node in two set graph to corresponding two tuple.
        unordered_map<Node, TwoTuple> node_to_two_tuple;
        // Inverse of the above map.
        unordered_map<TwoTuple, Node> two_tuple_to_node;
        // Manages edges labels.
        unordered_map<Edge, uint> edge_type;
        // Manages vertex ids
        unordered_map<Edge, uint> vertex_id;
        unordered_map<Edge, uint> local;

        // Create a node for each two set.
        Labels labels;
        Labels tuple_labels;
        if (use_labels) {
            labels = g.get_labels();
        }

        EdgeLabels edge_labels;
        if (use_edge_labels) {
            edge_labels = g.get_edge_labels();
        }

        Node num_two_tuples = 0;
        for (Node i = 0; i < num_nodes; ++i) {
            Nodes neighbors = g.get_neighbours(i);

            for (Node j: neighbors) {
                two_tuple_graph.add_node();

                // Map each pair to node in two set graph and also inverse.
                node_to_two_tuple.insert({{num_two_tuples, make_tuple(i, j)}});
                two_tuple_to_node.insert({{make_tuple(i, j), num_two_tuples}});
                num_two_tuples++;

                Label c_i = 1;
                Label c_j = 2;
                if (use_labels) {
                    c_i = AuxiliaryMethods::pairing(labels[i] + 1, c_i);
                    c_j = AuxiliaryMethods::pairing(labels[j] + 1, c_j);
                }

                Label c;
                if (g.has_edge(i, j)) {
                    if (use_edge_labels) {
                        auto s = edge_labels.find(make_tuple(i, j));
                        c = AuxiliaryMethods::pairing(3, s->second);
                    } else {
                        c = 3;
                    }
                } else if (i == j) {
                    c = 1;
                } else {
                    c = 2;
                }

                Label new_color = AuxiliaryMethods::pairing(AuxiliaryMethods::pairing(c_i, c_j), c);
                tuple_labels.push_back(new_color);
            }

            // Add self loops.
            two_tuple_graph.add_node();

            // Map each pair to node in two set graph and also inverse.
            node_to_two_tuple.insert({{num_two_tuples, make_tuple(i, i)}});
            two_tuple_to_node.insert({{make_tuple(i, i), num_two_tuples}});
            num_two_tuples++;

            Label c_i = 1;
            Label c_j = 2;
            if (use_labels) {
                c_i = AuxiliaryMethods::pairing(labels[i] + 1, c_i);
                c_j = AuxiliaryMethods::pairing(labels[i] + 1, c_j);
            }

            Label new_color = AuxiliaryMethods::pairing(AuxiliaryMethods::pairing(c_i, c_j), 1);
            tuple_labels.push_back(new_color);
        }

        for (Node i = 0; i < num_two_tuples; ++i) {
            // Get nodes of original graph corresponding to two tuple i.
            TwoTuple p = node_to_two_tuple.find(i)->second;
            Node v = std::get<0>(p);
            Node w = std::get<1>(p);

            // Exchange first node.
            Nodes v_neighbors = g.get_neighbours(v);
            for (Node v_n: v_neighbors) {
                unordered_map<TwoTuple, Node>::const_iterator t = two_tuple_to_node.find(make_tuple(v_n, w));

                if (t != two_tuple_to_node.end()) {
                    two_tuple_graph.add_edge(i, t->second);
                    edge_type.insert({{make_tuple(i, t->second), 1}});
                    vertex_id.insert({{make_tuple(i, t->second), v_n}});
                    local.insert({{make_tuple(i, t->second), 1}});
                }
            }

            // Exchange second node.
            Nodes w_neighbors = g.get_neighbours(w);
            for (Node w_n: w_neighbors) {
                unordered_map<TwoTuple, Node>::const_iterator t = two_tuple_to_node.find(make_tuple(v, w_n));

                if (t != two_tuple_to_node.end()) {
                    two_tuple_graph.add_edge(i, t->second);
                    edge_type.insert({{make_tuple(i, t->second), 2}});
                    vertex_id.insert({{make_tuple(i, t->second), w_n}});
                    local.insert({{make_tuple(i, t->second), 1}});
                }
            }
        }

        two_tuple_graph.set_edge_labels(edge_type);
        two_tuple_graph.set_labels(tuple_labels);
        two_tuple_graph.set_vertex_id(vertex_id);
        two_tuple_graph.set_local(local);
        two_tuple_graph.set_node_to_two_tuple(node_to_two_tuple);

        return two_tuple_graph;
    }


    Graph GenerateTwo::generate_global_graph(const Graph &g, const bool use_labels, const bool use_edge_labels) {
        size_t num_nodes = g.get_num_nodes();
        // New graph to be generated.
        Graph two_tuple_graph(false);

        // Maps node in two set graph to corresponding two tuple.
        unordered_map<Node, TwoTuple> node_to_two_tuple;
        // Inverse of the above map.
        unordered_map<TwoTuple, Node> two_tuple_to_node;
        // Manages edge labels.
        unordered_map<Edge, uint> edge_type;
        // Manages vertex ids
        unordered_map<Edge, uint> vertex_id;
        unordered_map<Edge, uint> local;

        // Create a node for each two set.
        Labels labels;
        Labels tuple_labels;
        if (use_labels) {
            labels = g.get_labels();
        }

        EdgeLabels edge_labels;
        if (use_edge_labels) {
            edge_labels = g.get_edge_labels();
        }

        size_t num_two_tuples = 0;
        for (Node i = 0; i < num_nodes; ++i) {
            for (Node j = 0; j < num_nodes; ++j) {
                two_tuple_graph.add_node();

                // Map each pair to node in two set graph and also inverse.
                node_to_two_tuple.insert({{num_two_tuples, make_tuple(i, j)}});
                two_tuple_to_node.insert({{make_tuple(i, j), num_two_tuples}});
                num_two_tuples++;

                Label c_i = 1;
                Label c_j = 2;
                if (use_labels) {
                    c_i = AuxiliaryMethods::pairing(labels[i] + 1, c_i);
                    c_j = AuxiliaryMethods::pairing(labels[j] + 1, c_j);
                }

                Label c;
                if (g.has_edge(i, j)) {
                    if (use_edge_labels) {
                        auto s = edge_labels.find(make_tuple(i, j));
                        c = AuxiliaryMethods::pairing(3, s->second);
                    } else {
                        c = 3;
                    }
                } else if (i == j) {
                    c = 1;
                } else {
                    c = 2;
                }

                Label new_color = AuxiliaryMethods::pairing(AuxiliaryMethods::pairing(c_i, c_j), c);
                tuple_labels.push_back(new_color);
            }
        }

        for (Node i = 0; i < num_two_tuples; ++i) {
            // Get nodes of original graph corresponding to two set i.
            TwoTuple p = node_to_two_tuple.find(i)->second;
            Node v = std::get<0>(p);
            Node w = std::get<1>(p);

            // Exchange first node.
            // Iterate over nodes.
            for (Node v_i = 0; v_i < num_nodes; ++v_i) {
                unordered_map<TwoTuple, Node>::const_iterator t;
                t = two_tuple_to_node.find(make_tuple(v_i, w));
                two_tuple_graph.add_edge(i, t->second);
                edge_type.insert({{make_tuple(i, t->second), 1}});
                vertex_id.insert({{make_tuple(i, t->second), v_i}});
                local.insert({{make_tuple(i, t->second), 1}});
            }

            // Exchange second node.
            // Iterate over nodes.
            for (Node v_i = 0; v_i < num_nodes; ++v_i) {
                unordered_map<TwoTuple, Node>::const_iterator t;
                t = two_tuple_to_node.find(make_tuple(v, v_i));
                two_tuple_graph.add_edge(i, t->second);
                edge_type.insert({{make_tuple(i, t->second), 2}});
                vertex_id.insert({{make_tuple(i, t->second), v_i}});
                local.insert({{make_tuple(i, t->second), 1}});
            }
        }

        two_tuple_graph.set_edge_labels(edge_type);
        two_tuple_graph.set_labels(tuple_labels);
        two_tuple_graph.set_vertex_id(vertex_id);
        two_tuple_graph.set_local(local);

        return two_tuple_graph;
    }

    Graph GenerateTwo::generate_global_graph_malkin(const Graph &g, const bool use_labels, const bool use_edge_labels) {
        size_t num_nodes = g.get_num_nodes();
        // New graph to be generated.
        Graph two_tuple_graph(false);

        // Maps node in two set graph to correponding two set.
        unordered_map<Node, TwoTuple> node_to_two_tuple;
        // Inverse of the above map.
        unordered_map<TwoTuple, Node> two_tuple_to_node;
        // Manages edge labels.
        unordered_map<Edge, uint> edge_type;
        // Manages vertex ids
        unordered_map<Edge, uint> vertex_id;
        unordered_map<Edge, uint> local;


        // Create a node for each two set.
        Labels labels;
        Labels tuple_labels;
        if (use_labels) {
            labels = g.get_labels();
        }

        EdgeLabels edge_labels;
        if (use_edge_labels) {
            edge_labels = g.get_edge_labels();
        }

        size_t num_two_tuples = 0;
        for (Node i = 0; i < num_nodes; ++i) {
            for (Node j = 0; j < num_nodes; ++j) {
                two_tuple_graph.add_node();

                // Map each pair to node in two set graph and also inverse.
                node_to_two_tuple.insert({{num_two_tuples, make_tuple(i, j)}});
                two_tuple_to_node.insert({{make_tuple(i, j), num_two_tuples}});
                num_two_tuples++;

                Label c_i = 1;
                Label c_j = 2;
                if (use_labels) {
                    c_i = AuxiliaryMethods::pairing(labels[i] + 1, c_i);
                    c_j = AuxiliaryMethods::pairing(labels[j] + 1, c_j);
                }

                Label c;
                if (g.has_edge(i, j)) {
                    if (use_edge_labels) {
                        auto s = edge_labels.find(make_tuple(i, j));
                        c = AuxiliaryMethods::pairing(3, s->second);
                    } else {
                        c = 3;
                    }
                } else if (i == j) {
                    c = 1;
                } else {
                    c = 2;
                }

                Label new_color = AuxiliaryMethods::pairing(AuxiliaryMethods::pairing(c_i, c_j), c);
                tuple_labels.push_back(new_color);
            }
        }

        for (Node i = 0; i < num_two_tuples; ++i) {
            // Get nodes of orginal graph corresponding to two set i.
            TwoTuple p = node_to_two_tuple.find(i)->second;
            Node v = std::get<0>(p);
            Node w = std::get<1>(p);

            // Exchange first node.
            // Iterate over nodes.
            for (Node v_i = 0; v_i < num_nodes; ++v_i) {
                unordered_map<TwoTuple, Node>::const_iterator t;
                t = two_tuple_to_node.find(make_tuple(v_i, w));

                // Local vs. global edge.
                if (g.has_edge(v, v_i)) {
                    edge_type.insert({{make_tuple(i, t->second), 1}});
                    vertex_id.insert({{make_tuple(i, t->second), v_i}});
                    local.insert({{make_tuple(i, t->second), 1}});

                } else {
                    edge_type.insert({{make_tuple(i, t->second), 1}});
                    vertex_id.insert({{make_tuple(i, t->second), v_i}});
                    local.insert({{make_tuple(i, t->second), 2}});
                }

                two_tuple_graph.add_edge(i, t->second);
            }
            // Exchange second node.
            // Iterate over nodes.
            for (Node v_i = 0; v_i < num_nodes; ++v_i) {
                unordered_map<TwoTuple, Node>::const_iterator t;
                t = two_tuple_to_node.find(make_tuple(v, v_i));

                // Local vs. global edge.
                if (g.has_edge(w, v_i)) {
                    edge_type.insert({{make_tuple(i, t->second), 2}});
                    vertex_id.insert({{make_tuple(i, t->second), v_i}});
                    local.insert({{make_tuple(i, t->second), 1}});

                } else {
                    edge_type.insert({{make_tuple(i, t->second), 2}});
                    vertex_id.insert({{make_tuple(i, t->second), v_i}});
                    local.insert({{make_tuple(i, t->second), 2}});
                }

                two_tuple_graph.add_edge(i, t->second);
            }
        }

        two_tuple_graph.set_edge_labels(edge_type);
        two_tuple_graph.set_labels(tuple_labels);
        two_tuple_graph.set_vertex_id(vertex_id);
        two_tuple_graph.set_local(local);


        return two_tuple_graph;
    }

    GenerateTwo::~GenerateTwo() {}
}
