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
#include "cupoch/geometry/lineset.h"

#include "cupoch/geometry/pointcloud.h"
#include "cupoch_pybind/docstring.h"
#include "cupoch_pybind/dl_converter.h"
#include "cupoch_pybind/geometry/geometry.h"
#include "cupoch_pybind/geometry/geometry_trampoline.h"

using namespace cupoch;

void pybind_lineset(py::module &m) {
    py::class_<geometry::LineSet<3>, PyGeometry3D<geometry::LineSet<3>>,
               std::shared_ptr<geometry::LineSet<3>>, geometry::GeometryBase<3>>
            lineset(m, "LineSet",
                    "LineSet define a sets of lines in 3D. A typical "
                    "application is to display the point cloud correspondence "
                    "pairs.");
    py::detail::bind_default_constructor<geometry::LineSet<3>>(lineset);
    py::detail::bind_copy_functions<geometry::LineSet<3>>(lineset);
    lineset.def(py::init<const thrust::host_vector<Eigen::Vector3f> &,
                         const thrust::host_vector<Eigen::Vector2i> &>(),
                "Create a LineSet from given points and line indices",
                "points"_a, "lines"_a)
            .def(py::init([](const wrapper::device_vector_vector3f &points,
                             const wrapper::device_vector_vector2i &lines) {
                     return std::unique_ptr<geometry::LineSet<3>>(
                             new geometry::LineSet<3>(points.data_,
                                                      lines.data_));
                 }),
                 "Create a LineSet from given points and line indices",
                 "points"_a, "lines"_a)
            .def("__repr__",
                 [](const geometry::LineSet<3> &lineset) {
                     return std::string("geometry::LineSet with ") +
                            std::to_string(lineset.lines_.size()) + " lines.";
                 })
            .def("has_points", &geometry::LineSet<3>::HasPoints,
                 "Returns ``True`` if the object contains points.")
            .def("has_lines", &geometry::LineSet<3>::HasLines,
                 "Returns ``True`` if the object contains lines.")
            .def("has_colors", &geometry::LineSet<3>::HasColors,
                 "Returns ``True`` if the object's lines contain contain "
                 "colors.")
            .def("get_line_coordinate",
                 &geometry::LineSet<3>::GetLineCoordinate, "line_index"_a)
            .def("paint_uniform_color",
                 &geometry::LineSet<3>::PaintUniformColor,
                 "Assigns each line in the line set the same color.")
            .def("paint_indexed_color",
                 [] (geometry::LineSet<3>& self, const wrapper::device_vector_size_t& indices, const Eigen::Vector3f& color) {
                     return self.PaintIndexedColor(indices.data_, color);
                 })
            .def_static(
                    "create_from_point_cloud_correspondences",
                    &geometry::LineSet<3>::CreateFromPointCloudCorrespondences,
                    "Factory function to create a LineSet from two "
                    "pointclouds and a correspondence set.",
                    "cloud0"_a, "cloud1"_a, "correspondences"_a)
            .def_static("create_from_oriented_bounding_box",
                        &geometry::LineSet<3>::CreateFromOrientedBoundingBox,
                        "Factory function to create a LineSet from an "
                        "OrientedBoundingBox.",
                        "box"_a)
            .def_static("create_from_axis_aligned_bounding_box",
                        &geometry::LineSet<3>::CreateFromAxisAlignedBoundingBox,
                        "Factory function to create a LineSet from an "
                        "AxisAlignedBoundingBox.",
                        "box"_a)
            .def_static("create_from_triangle_mesh",
                        &geometry::LineSet<3>::CreateFromTriangleMesh,
                        "Factory function to create a LineSet from edges of a "
                        "triangle mesh.",
                        "mesh"_a)
            .def_property(
                    "points",
                    [](geometry::LineSet<3> &line) {
                        return wrapper::device_vector_vector3f(line.points_);
                    },
                    [](geometry::LineSet<3> &line,
                       const wrapper::device_vector_vector3f &vec) {
                        wrapper::FromWrapper(line.points_, vec);
                    })
            .def_property(
                    "lines",
                    [](geometry::LineSet<3> &line) {
                        return wrapper::device_vector_vector2i(line.lines_);
                    },
                    [](geometry::LineSet<3> &line,
                       const wrapper::device_vector_vector2i &vec) {
                        wrapper::FromWrapper(line.lines_, vec);
                    })
            .def_property(
                    "colors",
                    [](geometry::LineSet<3> &line) {
                        return wrapper::device_vector_vector3f(line.colors_);
                    },
                    [](geometry::LineSet<3> &line,
                       const wrapper::device_vector_vector3f &vec) {
                        wrapper::FromWrapper(line.colors_, vec);
                    })
            .def("to_lines_dlpack",
                 [](geometry::LineSet<3> &line) {
                     return dlpack::ToDLpackCapsule<Eigen::Vector2i>(line.lines_);
                 })
            .def("from_lines_dlpack",
                 [](geometry::LineSet<3> &line, py::capsule dlpack) {
                     dlpack::FromDLpackCapsule<Eigen::Vector2i>(dlpack, line.lines_);
                 });
    docstring::ClassMethodDocInject(m, "LineSet", "has_colors");
    docstring::ClassMethodDocInject(m, "LineSet", "has_lines");
    docstring::ClassMethodDocInject(m, "LineSet", "has_points");
    docstring::ClassMethodDocInject(m, "LineSet", "get_line_coordinate",
                                    {{"line_index", "Index of the line."}});
    docstring::ClassMethodDocInject(m, "LineSet", "paint_uniform_color",
                                    {{"color", "Color for the LineSet."}});
    docstring::ClassMethodDocInject(
            m, "LineSet", "create_from_point_cloud_correspondences",
            {{"cloud0", "First point cloud."},
             {"cloud1", "Second point cloud."},
             {"correspondences", "Set of correspondences."}});
    docstring::ClassMethodDocInject(m, "LineSet",
                                    "create_from_oriented_bounding_box",
                                    {{"box", "The input bounding box."}});
    docstring::ClassMethodDocInject(m, "LineSet",
                                    "create_from_axis_aligned_bounding_box",
                                    {{"box", "The input bounding box."}});
    docstring::ClassMethodDocInject(m, "LineSet", "create_from_triangle_mesh",
                                    {{"mesh", "The input triangle mesh."}});
}

void pybind_lineset_methods(py::module &m) {}