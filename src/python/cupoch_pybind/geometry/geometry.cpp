#include "cupoch_pybind/geometry/geometry.h"
#include "cupoch_pybind/docstring.h"
#include "cupoch_pybind/geometry/geometry_trampoline.h"

using namespace cupoch;

void pybind_geometry_classes(py::module &m) {
    // cupoch.geometry functions
    m.def("get_rotation_matrix_from_xyz",
          &geometry::Geometry3D::GetRotationMatrixFromXYZ, "rotation"_a);
    m.def("get_rotation_matrix_from_yzx",
          &geometry::Geometry3D::GetRotationMatrixFromYZX, "rotation"_a);
    m.def("get_rotation_matrix_from_zxy",
          &geometry::Geometry3D::GetRotationMatrixFromZXY, "rotation"_a);
    m.def("get_rotation_matrix_from_xzy",
          &geometry::Geometry3D::GetRotationMatrixFromXZY, "rotation"_a);
    m.def("get_rotation_matrix_from_zyx",
          &geometry::Geometry3D::GetRotationMatrixFromZYX, "rotation"_a);
    m.def("get_rotation_matrix_from_yxz",
          &geometry::Geometry3D::GetRotationMatrixFromYXZ, "rotation"_a);
    m.def("get_rotation_matrix_from_axis_angle",
          &geometry::Geometry3D::GetRotationMatrixFromAxisAngle, "rotation"_a);
    m.def("get_rotation_matrix_from_quaternion",
          &geometry::Geometry3D::GetRotationMatrixFromQuaternion, "rotation"_a);

    // cupoch.geometry.Geometry
    py::class_<geometry::Geometry, PyGeometry<geometry::Geometry>,
               std::shared_ptr<geometry::Geometry>>
            geometry(m, "Geometry", "The base geometry class.");
    geometry.def("clear", &geometry::Geometry::Clear,
                 "Clear all elements in the geometry.")
            .def("is_empty", &geometry::Geometry::IsEmpty,
                 "Returns ``True`` iff the geometry is empty.")
            .def("get_geometry_type", &geometry::Geometry::GetGeometryType,
                 "Returns one of registered geometry types.")
            .def("dimension", &geometry::Geometry::Dimension,
                 "Returns whether the geometry is 2D or 3D.");
    docstring::ClassMethodDocInject(m, "Geometry", "clear");
    docstring::ClassMethodDocInject(m, "Geometry", "is_empty");
    docstring::ClassMethodDocInject(m, "Geometry", "get_geometry_type");
    docstring::ClassMethodDocInject(m, "Geometry", "dimension");

    // cupoch.geometry.Geometry.Type
    py::enum_<geometry::Geometry::GeometryType> geometry_type(geometry, "Type",
                                                              py::arithmetic());
    // Trick to write docs without listing the members in the enum class again.
    geometry_type.attr("__doc__") = docstring::static_property(
            py::cpp_function([](py::handle arg) -> std::string {
                return "Enum class for Geometry types.";
            }),
            py::none(), py::none(), "");

    geometry_type
            .value("Unspecified", geometry::Geometry::GeometryType::Unspecified)
            .value("PointCloud", geometry::Geometry::GeometryType::PointCloud)
            .value("VoxelGrid", geometry::Geometry::GeometryType::VoxelGrid)
            .value("OccupancyGrid", geometry::Geometry::GeometryType::OccupancyGrid)
            .value("LineSet", geometry::Geometry::GeometryType::LineSet)
            .value("TriangleMesh",
                   geometry::Geometry::GeometryType::TriangleMesh)
            .value("Image", geometry::Geometry::GeometryType::Image)
            .value("RGBDImage", geometry::Geometry::GeometryType::RGBDImage)
            .export_values();

    py::class_<geometry::Geometry3D, PyGeometry3D<geometry::Geometry3D>,
               std::shared_ptr<geometry::Geometry3D>, geometry::Geometry>
            geometry3d(m, "Geometry3D",
                       "The base geometry class for 3D geometries.");
    geometry3d
            .def("get_min_bound", &geometry::Geometry3D::GetMinBound,
                 "Returns min bounds for geometry coordinates.")
            .def("get_max_bound", &geometry::Geometry3D::GetMaxBound,
                 "Returns max bounds for geometry coordinates.")
            .def("get_center", &geometry::Geometry3D::GetCenter,
                 "Returns the center of the geometry coordinates.")
            .def("get_axis_aligned_bounding_box",
                 &geometry::Geometry3D::GetAxisAlignedBoundingBox,
                 "Returns an axis-aligned bounding box of the geometry.")
            .def("transform", &geometry::Geometry3D::Transform,
                 "Apply transformation (4x4 matrix) to the geometry "
                 "coordinates.")
            .def("translate", &geometry::Geometry3D::Translate,
                 "Apply translation to the geometry coordinates.",
                 "translation"_a, "relative"_a = true)
            .def("scale", &geometry::Geometry3D::Scale,
                 "Apply scaling to the geometry coordinates.", "scale"_a,
                 "center"_a = true)
            .def("rotate", &geometry::Geometry3D::Rotate,
                 "Apply rotation to the geometry coordinates and normals.",
                 "R"_a, "center"_a = true)
            .def_static("get_rotation_matrix_from_xyz",
                        &geometry::Geometry3D::GetRotationMatrixFromXYZ,
                        "rotation"_a)
            .def_static("get_rotation_matrix_from_yzx",
                        &geometry::Geometry3D::GetRotationMatrixFromYZX,
                        "rotation"_a)
            .def_static("get_rotation_matrix_from_zxy",
                        &geometry::Geometry3D::GetRotationMatrixFromZXY,
                        "rotation"_a)
            .def_static("get_rotation_matrix_from_xzy",
                        &geometry::Geometry3D::GetRotationMatrixFromXZY,
                        "rotation"_a)
            .def_static("get_rotation_matrix_from_zyx",
                        &geometry::Geometry3D::GetRotationMatrixFromZYX,
                        "rotation"_a)
            .def_static("get_rotation_matrix_from_yxz",
                        &geometry::Geometry3D::GetRotationMatrixFromYXZ,
                        "rotation"_a)
            .def_static("get_rotation_matrix_from_axis_angle",
                        &geometry::Geometry3D::GetRotationMatrixFromAxisAngle,
                        "rotation"_a)
            .def_static("get_rotation_matrix_from_quaternion",
                        &geometry::Geometry3D::GetRotationMatrixFromQuaternion,
                        "rotation"_a);
    docstring::ClassMethodDocInject(m, "Geometry3D", "get_min_bound");
    docstring::ClassMethodDocInject(m, "Geometry3D", "get_max_bound");
    docstring::ClassMethodDocInject(m, "Geometry3D", "get_center");
    docstring::ClassMethodDocInject(m, "Geometry3D",
                                    "get_axis_aligned_bounding_box");
    docstring::ClassMethodDocInject(m, "Geometry3D", "transform");
    docstring::ClassMethodDocInject(
            m, "Geometry3D", "translate",
            {{"translation", "A 3D vector to transform the geometry"},
             {"relative",
              "If true, the translation vector is directly added to the "
              "geometry "
              "coordinates. Otherwise, the center is moved to the translation "
              "vector."}});
    docstring::ClassMethodDocInject(
            m, "Geometry3D", "scale",
            {{"scale",
              "The scale parameter that is multiplied to the points/vertices "
              "of the geometry"},
             {"center",
              "If true, then the scale is applied to the centered geometry"}});
    docstring::ClassMethodDocInject(m, "Geometry3D", "rotate",
                                    {{"R", "The rotation matrix"},
                                     {"center",
                                      "If true, then the rotation is applied "
                                      "to the centered geometry"}});

    // cupoch.geometry.Geometry2D
    py::class_<geometry::Geometry2D, PyGeometry2D<geometry::Geometry2D>,
               std::shared_ptr<geometry::Geometry2D>, geometry::Geometry>
            geometry2d(m, "Geometry2D",
                       "The base geometry class for 2D geometries.");
    geometry2d
            .def("get_min_bound", &geometry::Geometry2D::GetMinBound,
                 "Returns min bounds for geometry coordinates.")
            .def("get_max_bound", &geometry::Geometry2D::GetMaxBound,
                 "Returns max bounds for geometry coordinates.");
    docstring::ClassMethodDocInject(m, "Geometry2D", "get_min_bound");
    docstring::ClassMethodDocInject(m, "Geometry2D", "get_max_bound");
}

void pybind_geometry(py::module &m) {
    py::module m_submodule = m.def_submodule("geometry");
    pybind_geometry_classes(m_submodule);
    pybind_kdtreeflann(m_submodule);
    pybind_pointcloud(m_submodule);
    pybind_voxelgrid(m_submodule);
    pybind_occupanygrid(m_submodule);
    pybind_lineset(m_submodule);
    pybind_graph(m_submodule);
    pybind_meshbase(m_submodule);
    pybind_trianglemesh(m_submodule);
    pybind_image(m_submodule);
    pybind_boundingvolume(m_submodule);
}