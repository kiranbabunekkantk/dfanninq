/**
 * Copyright (c) 2020 Neka-Nat
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
**/
#include "cupoch/geometry/trianglemesh.h"

#include "cupoch/geometry/image.h"
#include "cupoch/geometry/pointcloud.h"
#include "cupoch/utility/console.h"
#include "cupoch_pybind/dl_converter.h"
#include "cupoch_pybind/docstring.h"
#include "cupoch_pybind/geometry/geometry.h"
#include "cupoch_pybind/geometry/geometry_trampoline.h"

using namespace cupoch;

void pybind_trianglemesh(py::module &m) {
    py::class_<geometry::TriangleMesh, PyGeometry3D<geometry::TriangleMesh>,
               std::shared_ptr<geometry::TriangleMesh>, geometry::MeshBase>
            trianglemesh(m, "TriangleMesh",
                         "TriangleMesh class. Triangle mesh contains vertices "
                         "and triangles represented by the indices to the "
                         "vertices. Optionally, the mesh may also contain "
                         "triangle normals, vertex normals and vertex colors.");
    py::detail::bind_default_constructor<geometry::TriangleMesh>(trianglemesh);
    py::detail::bind_copy_functions<geometry::TriangleMesh>(trianglemesh);
    trianglemesh
            .def(py::init([](const wrapper::device_vector_vector3f &vertices,
                             const wrapper::device_vector_vector3i &triangles) {
                     return std::unique_ptr<geometry::TriangleMesh>(
                             new geometry::TriangleMesh(vertices.data_,
                                                        triangles.data_));
                 }),
                 "Create a triangle mesh from vertices and triangle indices",
                 "vertices"_a, "triangles"_a)
            .def("__repr__",
                 [](const geometry::TriangleMesh &mesh) {
                     std::string info = fmt::format(
                             "geometry::TriangleMesh with {} points and {} "
                             "triangles",
                             mesh.vertices_.size(), mesh.triangles_.size());

                     if (mesh.HasTexture()) {
                         info += fmt::format(", and ({}, {}) texture.",
                                             mesh.texture_.width_,
                                             mesh.texture_.height_);
                     } else {
                         info += ".";
                     }
                     return info;
                 })
            .def(py::self + py::self)
            .def(py::self += py::self)
            .def("compute_triangle_normals",
                 &geometry::TriangleMesh::ComputeTriangleNormals,
                 "Function to compute triangle normals, usually called before "
                 "rendering",
                 "normalized"_a = true)
            .def("compute_vertex_normals",
                 &geometry::TriangleMesh::ComputeVertexNormals,
                 "Function to compute vertex normals, usually called before "
                 "rendering",
                 "normalized"_a = true)
            .def("compute_edge_list", &geometry::TriangleMesh::ComputeEdgeList,
                 "Function to compute edge list, call before edge "
                 "list is needed")
            .def("remove_duplicated_vertices",
                 &geometry::TriangleMesh::RemoveDuplicatedVertices,
                 "Function that removes duplicated verties, i.e., vertices "
                 "that have identical coordinates.")
            .def("remove_duplicated_triangles",
                 &geometry::TriangleMesh::RemoveDuplicatedTriangles,
                 "Function that removes duplicated triangles, i.e., removes "
                 "triangles that reference the same three vertices, "
                 "independent of their order.")
            .def("remove_unreferenced_vertices",
                 &geometry::TriangleMesh::RemoveUnreferencedVertices,
                 "This function removes vertices from the triangle mesh that "
                 "are not referenced in any triangle of the mesh.")
            .def("remove_degenerate_triangles",
                 &geometry::TriangleMesh::RemoveDegenerateTriangles,
                 "Function that removes degenerate triangles, i.e., triangles "
                 "that references a single vertex multiple times in a single "
                 "triangle. They are usually the product of removing "
                 "duplicated vertices.")
            .def("filter_sharpen", &geometry::TriangleMesh::FilterSharpen,
                 "Function to sharpen triangle mesh. The output value "
                 "(:math:`v_o`) is the input value (:math:`v_i`) plus strength "
                 "times the input value minus he sum of he adjacent values. "
                 ":math:`v_o = v_i x strength (v_i * |N| - \\sum_{n \\in N} "
                 "v_n)`",
                 "number_of_iterations"_a = 1, "strength"_a = 1,
                 "filter_scope"_a = geometry::MeshBase::FilterScope::All)
            .def("filter_smooth_simple",
                 &geometry::TriangleMesh::FilterSmoothSimple,
                 "Function to smooth triangle mesh with simple neighbour "
                 "average. :math:`v_o = \\frac{v_i + \\sum_{n \\in N} "
                 "v_n)}{|N| + 1}`, with :math:`v_i` being the input value, "
                 ":math:`v_o` the output value, and :math:`N` is the set of "
                 "adjacent neighbours.",
                 "number_of_iterations"_a = 1,
                 "filter_scope"_a = geometry::MeshBase::FilterScope::All)
            .def("filter_smooth_laplacian",
                 &geometry::TriangleMesh::FilterSmoothLaplacian,
                 "Function to smooth triangle mesh using Laplacian. :math:`v_o "
                 "= v_i \\cdot \\lambda (sum_{n \\in N} w_n v_n - v_i)`, with "
                 ":math:`v_i` being the input value, :math:`v_o` the output "
                 "value, :math:`N` is the  set of adjacent neighbours, "
                 ":math:`w_n` is the weighting of the neighbour based on the "
                 "inverse distance (closer neighbours have higher weight), and "
                 "lambda is the smoothing parameter.",
                 "number_of_iterations"_a = 1, "lambda"_a = 0.5,
                 "filter_scope"_a = geometry::MeshBase::FilterScope::All)
            .def("filter_smooth_taubin",
                 &geometry::TriangleMesh::FilterSmoothTaubin,
                 "Function to smooth triangle mesh using method of Taubin, "
                 "\"Curve and Surface Smoothing Without Shrinkage\", 1995. "
                 "Applies in each iteration two times filter_smooth_laplacian, "
                 "first with filter parameter lambda and second with filter "
                 "parameter mu as smoothing parameter. This method avoids "
                 "shrinkage of the triangle mesh.",
                 "number_of_iterations"_a = 1, "lambda"_a = 0.5, "mu"_a = -0.53,
                 "filter_scope"_a = geometry::MeshBase::FilterScope::All)
            .def("has_vertices", &geometry::TriangleMesh::HasVertices,
                 "Returns ``True`` if the mesh contains vertices.")
            .def("has_triangles", &geometry::TriangleMesh::HasTriangles,
                 "Returns ``True`` if the mesh contains triangles.")
            .def("has_vertex_normals",
                 &geometry::TriangleMesh::HasVertexNormals,
                 "Returns ``True`` if the mesh contains vertex normals.")
            .def("has_vertex_colors", &geometry::TriangleMesh::HasVertexColors,
                 "Returns ``True`` if the mesh contains vertex colors.")
            .def("has_triangle_normals",
                 &geometry::TriangleMesh::HasTriangleNormals,
                 "Returns ``True`` if the mesh contains triangle normals.")
            .def("has_edge_list", &geometry::TriangleMesh::HasEdgeList,
                 "Returns ``True`` if the mesh contains edge list.")
            .def("has_triangle_uvs", &geometry::TriangleMesh::HasTriangleUvs,
                 "Returns ``True`` if the mesh contains uv coordinates.")
            .def("has_texture", &geometry::TriangleMesh::HasTexture,
                 "Returns ``True`` if the mesh contains a texture image.")
            .def("normalize_normals", &geometry::TriangleMesh::NormalizeNormals,
                 "Normalize both triangle normals and vertex normals to legnth "
                 "1.")
            .def("paint_uniform_color",
                 &geometry::TriangleMesh::PaintUniformColor,
                 "Assigns each vertex in the TriangleMesh the same color.")
            .def("get_surface_area",
                 (float (geometry::TriangleMesh::*)() const) &
                         geometry::TriangleMesh::GetSurfaceArea,
                 "Function that computes the surface area of the mesh, i.e. "
                 "the sum of the individual triangle surfaces.")
            .def("sample_points_uniformly",
                 &geometry::TriangleMesh::SamplePointsUniformly,
                 "Function to uniformly sample points from the mesh.",
                 "number_of_points"_a = 100, "use_triangle_normal"_a = false)
            .def_static("create_box", &geometry::TriangleMesh::CreateBox,
                        "Factory function to create a box. The left bottom "
                        "corner on the "
                        "front will be placed at (0, 0, 0).",
                        "width"_a = 1.0, "height"_a = 1.0, "depth"_a = 1.0)
            .def_static("create_tetrahedron",
                        &geometry::TriangleMesh::CreateTetrahedron,
                        "Factory function to create a tetrahedron. The "
                        "centroid of the mesh "
                        "will be placed at (0, 0, 0) and the vertices have a "
                        "distance of "
                        "radius to the center.",
                        "radius"_a = 1.0)
            .def_static("create_octahedron",
                        &geometry::TriangleMesh::CreateOctahedron,
                        "Factory function to create a octahedron. The centroid "
                        "of the mesh "
                        "will be placed at (0, 0, 0) and the vertices have a "
                        "distance of "
                        "radius to the center.",
                        "radius"_a = 1.0)
            .def_static("create_icosahedron",
                        &geometry::TriangleMesh::CreateIcosahedron,
                        "Factory function to create a icosahedron. The "
                        "centroid of the mesh "
                        "will be placed at (0, 0, 0) and the vertices have a "
                        "distance of "
                        "radius to the center.",
                        "radius"_a = 1.0)
            .def_static("create_sphere", &geometry::TriangleMesh::CreateSphere,
                        "Factory function to create a sphere mesh centered at "
                        "(0, 0, 0).",
                        "radius"_a = 1.0, "resolution"_a = 20)
            .def_static("create_cylinder",
                        &geometry::TriangleMesh::CreateCylinder,
                        "Factory function to create a cylinder mesh.",
                        "radius"_a = 1.0, "height"_a = 2.0, "resolution"_a = 20,
                        "split"_a = 4)
            .def_static("create_capsule",
                        &geometry::TriangleMesh::CreateCapsule,
                        "Factory function to create a capsule mesh.",
                        "radius"_a = 1.0, "height"_a = 2.0, "resolution"_a = 20,
                        "split"_a = 4)
            .def_static("create_cone", &geometry::TriangleMesh::CreateCone,
                        "Factory function to create a cone mesh.",
                        "radius"_a = 1.0, "height"_a = 2.0, "resolution"_a = 20,
                        "split"_a = 1)
            .def_static("create_torus", &geometry::TriangleMesh::CreateTorus,
                        "Factory function to create a torus mesh.",
                        "torus_radius"_a = 1.0, "tube_radius"_a = 0.5,
                        "radial_resolution"_a = 30, "tubular_resolution"_a = 20)
            .def_static("create_arrow", &geometry::TriangleMesh::CreateArrow,
                        "Factory function to create an arrow mesh",
                        "cylinder_radius"_a = 1.0, "cone_radius"_a = 1.5,
                        "cylinder_height"_a = 5.0, "cone_height"_a = 4.0,
                        "resolution"_a = 20, "cylinder_split"_a = 4,
                        "cone_split"_a = 1)
            .def_static("create_moebius",
                        &geometry::TriangleMesh::CreateMoebius,
                        "Factory function to create a Moebius strip.",
                        "length_split"_a = 70, "width_split"_a = 15,
                        "twists"_a = 1, "raidus"_a = 1, "flatness"_a = 1,
                        "width"_a = 1, "scale"_a = 1)
            .def_static("create_coordinate_frame",
                        &geometry::TriangleMesh::CreateCoordinateFrame,
                        "Factory function to create a coordinate frame mesh. "
                        "The coordinate "
                        "frame will be centered at ``origin``. The x, y, z "
                        "axis will be "
                        "rendered as red, green, and blue arrows respectively.",
                        "size"_a = 1.0,
                        "origin"_a = Eigen::Vector3d(0.0, 0.0, 0.0))
            .def_property(
                    "triangles",
                    [](geometry::TriangleMesh &mesh) {
                        return wrapper::device_vector_vector3i(mesh.triangles_);
                    },
                    [](geometry::TriangleMesh &mesh,
                       const wrapper::device_vector_vector3i &vec) {
                        wrapper::FromWrapper(mesh.triangles_, vec);
                    })
            .def_property(
                    "triangle_normals",
                    [](geometry::TriangleMesh &mesh) {
                        return wrapper::device_vector_vector3f(
                                mesh.triangle_normals_);
                    },
                    [](geometry::TriangleMesh &mesh,
                       const wrapper::device_vector_vector3f &vec) {
                        wrapper::FromWrapper(mesh.triangle_normals_, vec);
                    })
            .def_property(
                    "edge_list",
                    [](geometry::TriangleMesh &mesh) {
                        return wrapper::device_vector_vector2i(mesh.edge_list_);
                    },
                    [](geometry::TriangleMesh &mesh,
                       const wrapper::device_vector_vector2i &vec) {
                        wrapper::FromWrapper(mesh.edge_list_, vec);
                    })
            .def_property(
                    "triangle_uvs",
                    [](geometry::TriangleMesh &mesh) {
                        return wrapper::device_vector_vector2f(
                                mesh.triangle_uvs_);
                    },
                    [](geometry::TriangleMesh &mesh,
                       const wrapper::device_vector_vector2f &vec) {
                        wrapper::FromWrapper(mesh.triangle_uvs_, vec);
                    })
            .def_readwrite("texture", &geometry::TriangleMesh::texture_,
                           "cupoch.geometry.Image: The texture image.")
            .def("to_vertices_dlpack",
                 [](geometry::TriangleMesh &mesh) {
                     return dlpack::ToDLpackCapsule<Eigen::Vector3f>(mesh.vertices_);
                 })
            .def("to_vertex_normals_dlpack",
                 [](geometry::TriangleMesh &mesh) {
                     return dlpack::ToDLpackCapsule<Eigen::Vector3f>(mesh.vertex_normals_);
                 })
            .def("to_vertex_colors_dlpack",
                 [](geometry::TriangleMesh &mesh) {
                     return dlpack::ToDLpackCapsule<Eigen::Vector3f>(mesh.vertex_colors_);
                 })
            .def("from_vertices_dlpack",
                 [](geometry::TriangleMesh &mesh, py::capsule dlpack) {
                     dlpack::FromDLpackCapsule<Eigen::Vector3f>(dlpack, mesh.vertices_);
                 })
            .def("from_vertex_normals_dlpack",
                 [](geometry::TriangleMesh &mesh, py::capsule dlpack) {
                     dlpack::FromDLpackCapsule<Eigen::Vector3f>(dlpack, mesh.vertex_normals_);
                 })
            .def("from_vertex_colors_dlpack",
                 [](geometry::TriangleMesh &mesh, py::capsule dlpack) {
                     dlpack::FromDLpackCapsule<Eigen::Vector3f>(dlpack, mesh.vertex_colors_);
                 });
    docstring::ClassMethodDocInject(m, "TriangleMesh", "compute_edge_list");
    docstring::ClassMethodDocInject(m, "TriangleMesh",
                                    "compute_triangle_normals");
    docstring::ClassMethodDocInject(m, "TriangleMesh",
                                    "compute_vertex_normals");
    docstring::ClassMethodDocInject(m, "TriangleMesh", "has_edge_list");
    docstring::ClassMethodDocInject(
            m, "TriangleMesh", "has_triangle_normals",
            {{"normalized",
              "Set to ``True`` to normalize the normal to length 1."}});
    docstring::ClassMethodDocInject(m, "TriangleMesh", "has_triangles");
    docstring::ClassMethodDocInject(m, "TriangleMesh", "has_triangle_uvs");
    docstring::ClassMethodDocInject(m, "TriangleMesh", "has_texture");
    docstring::ClassMethodDocInject(m, "TriangleMesh", "has_vertex_colors");
    docstring::ClassMethodDocInject(
            m, "TriangleMesh", "has_vertex_normals",
            {{"normalized",
              "Set to ``True`` to normalize the normal to length 1."}});
    docstring::ClassMethodDocInject(m, "TriangleMesh", "has_vertices");
    docstring::ClassMethodDocInject(m, "TriangleMesh", "normalize_normals");
    docstring::ClassMethodDocInject(m, "TriangleMesh",
                                    "remove_duplicated_vertices");
    docstring::ClassMethodDocInject(m, "TriangleMesh",
                                    "remove_duplicated_triangles");
    docstring::ClassMethodDocInject(m, "TriangleMesh",
                                    "remove_unreferenced_vertices");
    docstring::ClassMethodDocInject(m, "TriangleMesh",
                                    "remove_degenerate_triangles");
    docstring::ClassMethodDocInject(
            m, "TriangleMesh", "filter_sharpen",
            {{"number_of_iterations",
              " Number of repetitions of this operation"},
             {"strengh", "Filter parameter."},
             {"scope", "Mesh property that should be filtered."}});
    docstring::ClassMethodDocInject(
            m, "TriangleMesh", "filter_smooth_simple",
            {{"number_of_iterations",
              " Number of repetitions of this operation"},
             {"scope", "Mesh property that should be filtered."}});
    docstring::ClassMethodDocInject(
            m, "TriangleMesh", "filter_smooth_laplacian",
            {{"number_of_iterations",
              " Number of repetitions of this operation"},
             {"lambda", "Filter parameter."},
             {"scope", "Mesh property that should be filtered."}});
    docstring::ClassMethodDocInject(
            m, "TriangleMesh", "filter_smooth_taubin",
            {{"number_of_iterations",
              " Number of repetitions of this operation"},
             {"lambda", "Filter parameter."},
             {"mu", "Filter parameter."},
             {"scope", "Mesh property that should be filtered."}});
    docstring::ClassMethodDocInject(
            m, "TriangleMesh", "paint_uniform_color",
            {{"color", "RGB color for the PointCloud."}});
    docstring::ClassMethodDocInject(
            m, "TriangleMesh", "create_sphere",
            {{"radius", "The radius of the sphere."},
             {"resolution",
              "The resolution of the sphere. The longitues will be split into "
              "``resolution`` segments (i.e. there are ``resolution + 1`` "
              "latitude lines including the north and south pole). The "
              "latitudes will be split into ```2 * resolution`` segments (i.e. "
              "there are ``2 * resolution`` longitude lines.)"}});
    docstring::ClassMethodDocInject(
            m, "TriangleMesh", "create_cylinder",
            {{"radius", "The radius of the cylinder."},
             {"height",
              "The height of the cylinder. The axis of the cylinder will be "
              "from (0, 0, -height/2) to (0, 0, height/2)."},
             {"resolution",
              " The circle will be split into ``resolution`` segments"},
             {"split",
              "The ``height`` will be split into ``split`` segments."}});
    docstring::ClassMethodDocInject(
            m, "TriangleMesh", "create_cone",
            {{"radius", "The radius of the cone."},
             {"height",
              "The height of the cone. The axis of the cone will be from (0, "
              "0, 0) to (0, 0, height)."},
             {"resolution",
              "The circle will be split into ``resolution`` segments"},
             {"split",
              "The ``height`` will be split into ``split`` segments."}});
    docstring::ClassMethodDocInject(
            m, "TriangleMesh", "create_arrow",
            {{"cylinder_radius", "The radius of the cylinder."},
             {"cone_radius", "The radius of the cone."},
             {"cylinder_height",
              "The height of the cylinder. The cylinder is from (0, 0, 0) to "
              "(0, 0, cylinder_height)"},
             {"cone_height",
              "The height of the cone. The axis of the cone will be from (0, "
              "0, cylinder_height) to (0, 0, cylinder_height + cone_height)"},
             {"resolution",
              "The cone will be split into ``resolution`` segments."},
             {"cylinder_split",
              "The ``cylinder_height`` will be split into ``cylinder_split`` "
              "segments."},
             {"cone_split",
              "The ``cone_height`` will be split into ``cone_split`` "
              "segments."}});
    docstring::ClassMethodDocInject(
            m, "TriangleMesh", "create_coordinate_frame",
            {{"size", "The size of the coordinate frame."},
             {"origin", "The origin of the cooridnate frame."}});
}

void pybind_trianglemesh_methods(py::module &m) {}