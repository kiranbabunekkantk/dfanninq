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
#include <thrust/host_vector.h>

#include "cupoch_pybind/cupoch_pybind.h"

namespace pybind11 {

template <typename Vector,
          typename holder_type = std::unique_ptr<Vector>,
          typename... Args>
py::class_<Vector, holder_type> bind_vector_without_repr(
        py::module &m, std::string const &name, Args &&... args) {
    // hack function to disable __repr__ for the convenient function
    // bind_vector()
    using Class_ = py::class_<Vector, holder_type>;
    Class_ cl(m, name.c_str(), std::forward<Args>(args)...);
    cl.def(py::init<>());
    cl.def(
            "__bool__", [](const Vector &v) -> bool { return !v.empty(); },
            "Check whether the list is nonempty");
    cl.def("__len__", &Vector::size);
    return cl;
}

template <typename EigenVector>
cupoch::wrapper::device_vector_wrapper<EigenVector> py_array_to_vectors_float(
        py::array_t<float, py::array::c_style | py::array::forcecast> array) {
    size_t eigen_vector_size = EigenVector::SizeAtCompileTime;
    if (array.ndim() != 2 || array.shape(1) != eigen_vector_size) {
        throw py::cast_error();
    }
    thrust::host_vector<EigenVector> eigen_vectors(array.shape(0));
    auto array_unchecked = array.mutable_unchecked<2>();
    for (auto i = 0; i < array_unchecked.shape(0); ++i) {
        // The EigenVector here must be a float-typed eigen vector, since only
        // cupoch::Vector3dVector binds to py_array_to_vectors_float.
        // Therefore, we can use the memory map directly.
        eigen_vectors[i] = Eigen::Map<EigenVector>(&array_unchecked(i, 0));
    }
    return cupoch::wrapper::device_vector_wrapper<EigenVector>(eigen_vectors);
}

template <typename EigenVector>
cupoch::wrapper::device_vector_wrapper<EigenVector> py_array_to_vectors_int(
        py::array_t<int, py::array::c_style | py::array::forcecast> array) {
    size_t eigen_vector_size = EigenVector::SizeAtCompileTime;
    if (array.ndim() != 2 || array.shape(1) != eigen_vector_size) {
        throw py::cast_error();
    }
    thrust::host_vector<EigenVector> eigen_vectors(array.shape(0));
    auto array_unchecked = array.mutable_unchecked<2>();
    for (auto i = 0; i < array_unchecked.shape(0); ++i) {
        eigen_vectors[i] = Eigen::Map<EigenVector>(&array_unchecked(i, 0));
    }
    return cupoch::wrapper::device_vector_wrapper<EigenVector>(eigen_vectors);
}

template <typename EigenVector,
          typename EigenAllocator = Eigen::aligned_allocator<EigenVector>>
cupoch::wrapper::device_vector_wrapper<EigenVector>
py_array_to_vectors_int_eigen_allocator(
        py::array_t<int, py::array::c_style | py::array::forcecast> array) {
    size_t eigen_vector_size = EigenVector::SizeAtCompileTime;
    if (array.ndim() != 2 || array.shape(1) != eigen_vector_size) {
        throw py::cast_error();
    }
    thrust::host_vector<EigenVector, EigenAllocator> eigen_vectors(
            array.shape(0));
    auto array_unchecked = array.mutable_unchecked<2>();
    for (auto i = 0; i < array_unchecked.shape(0); ++i) {
        eigen_vectors[i] = Eigen::Map<EigenVector>(&array_unchecked(i, 0));
    }
    return cupoch::wrapper::device_vector_wrapper<EigenVector>(eigen_vectors);
}

}  // namespace pybind11

namespace {

template <typename Scalar,
          typename Vector = cupoch::wrapper::device_vector_wrapper<Scalar>,
          typename holder_type = std::unique_ptr<Vector>>
py::class_<Vector, holder_type> pybind_eigen_vector_of_scalar(
        py::module &m, const std::string &bind_name) {
    auto vec = py::bind_vector_without_repr<
            cupoch::wrapper::device_vector_wrapper<Scalar>>(m, bind_name,
                                                            py::module_local());
    vec.def(py::init<thrust::host_vector<Scalar>>());
    vec.def("cpu", &cupoch::wrapper::device_vector_wrapper<Scalar>::cpu);
    vec.def(
            "__iadd__",
            [](cupoch::wrapper::device_vector_wrapper<Scalar> &self,
               const Eigen::Matrix<Scalar, Eigen::Dynamic, 1> &other) {
                thrust::host_vector<Scalar> hso(other.data(),
                                                other.data() + other.rows());
                self += hso;
                return self;
            },
            py::is_operator());
    vec.def("__copy__", [](cupoch::wrapper::device_vector_wrapper<Scalar> &v) {
        return cupoch::wrapper::device_vector_wrapper<Scalar>(v);
    });
    vec.def("__deepcopy__",
            [](cupoch::wrapper::device_vector_wrapper<Scalar> &v,
               py::dict &memo) {
                return cupoch::wrapper::device_vector_wrapper<Scalar>(v);
            });
    return vec;
}

template <typename EigenVector,
          typename Vector = cupoch::wrapper::device_vector_wrapper<EigenVector>,
          typename holder_type = std::unique_ptr<Vector>,
          typename InitFunc>
py::class_<Vector, holder_type> pybind_eigen_vector_of_vector(
        py::module &m,
        const std::string &bind_name,
        const std::string &repr_name,
        InitFunc init_func) {
    typedef typename EigenVector::Scalar Scalar;
    auto vec = py::bind_vector_without_repr<
            cupoch::wrapper::device_vector_wrapper<EigenVector>>(
            m, bind_name, py::module_local());
    vec.def(py::init(init_func));
    vec.def("__repr__",
            [repr_name](
                    const cupoch::wrapper::device_vector_wrapper<EigenVector>
                            &v) {
                return repr_name + std::string(" with ") +
                       std::to_string(v.size()) + std::string(" elements.\n") +
                       std::string("Use cpu() method to copy data to host.");
            });
    vec.def("cpu", &cupoch::wrapper::device_vector_wrapper<EigenVector>::cpu);
    vec.def(
            "__iadd__",
            [](cupoch::wrapper::device_vector_wrapper<EigenVector> &self,
               const Eigen::Matrix<Scalar, Eigen::Dynamic,
                                   EigenVector::RowsAtCompileTime> &other) {
                thrust::host_vector<EigenVector> hso(other.rows());
                for (int i = 0; i < other.rows(); ++i) {
                    hso[i] = other.row(i);
                }
                self += hso;
                return self;
            },
            py::is_operator());
    vec.def("__copy__",
            [](cupoch::wrapper::device_vector_wrapper<EigenVector> &v) {
                return cupoch::wrapper::device_vector_wrapper<EigenVector>(v);
            });
    vec.def("__deepcopy__",
            [](cupoch::wrapper::device_vector_wrapper<EigenVector> &v,
               py::dict &memo) {
                return cupoch::wrapper::device_vector_wrapper<EigenVector>(v);
            });

    return vec;
}

}  // unnamed namespace

void pybind_eigen(py::module &m) {
    py::handle static_property = py::handle(
            (PyObject *)py::detail::get_internals().static_property_type);

    auto intvector = pybind_eigen_vector_of_scalar<int>(m, "IntVector");
    intvector.attr("__doc__") = static_property(
            py::cpp_function([](py::handle arg) -> std::string {
                return R"(Convert int32 numpy array of shape ``(n,)`` to Cupoch format.)";
            }),
            py::none(), py::none(), "");

    auto ulongvector =
            pybind_eigen_vector_of_scalar<unsigned long>(m, "ULongVector");
    ulongvector.attr("__doc__") = static_property(
            py::cpp_function([](py::handle arg) -> std::string {
                return R"(Convert ulong numpy array of shape ``(n,)`` to Cupoch format.)";
            }),
            py::none(), py::none(), "");

    auto floatvector = pybind_eigen_vector_of_scalar<float>(m, "FloatVector");
    floatvector.attr("__doc__") = static_property(
            py::cpp_function([](py::handle arg) -> std::string {
                return R"(Convert float32 numpy array of shape ``(n,)`` to Cupoch format.)";
            }),
            py::none(), py::none(), "");

    auto vector3fvector = pybind_eigen_vector_of_vector<Eigen::Vector3f>(
            m, "Vector3fVector", "utility::device_vector<Eigen::Vector3f>",
            py::py_array_to_vectors_float<Eigen::Vector3f>);
    vector3fvector.attr("__doc__") = static_property(
            py::cpp_function([](py::handle arg) -> std::string {
                return R"(Convert float32 numpy array of shape ``(n, 3)`` to Cupoch format..
Example usage
.. code-block:: python
    import cupoch
    import numpy as np
    pcd = cupoch.geometry.PointCloud()
    np_points = np.random.rand(100, 3)
    # From numpy to Cupoch
    pcd.points = cupoch.utility.Vector3fVector(np_points)
    # From Cupoch to numpy
    np_points = np.asarray(pcd.points.cpu())
)";
            }),
            py::none(), py::none(), "");

    auto vector2fvector = pybind_eigen_vector_of_vector<Eigen::Vector2f>(
            m, "Vector2fVector", "utility::device_vector<Eigen::Vector2f>",
            py::py_array_to_vectors_float<Eigen::Vector2f>);
    vector2fvector.attr("__doc__") = static_property(
            py::cpp_function([](py::handle arg) -> std::string {
                return R"(Convert float32 numpy array of shape ``(n, 2)`` to Cupoch format..
Example usage
.. code-block:: python
    import cupoch
    import numpy as np
    pcd = cupoch.geometry.PointCloud()
    np_points = np.random.rand(100, 2)
    # From numpy to Cupoch
    pcd.points = cupoch.utility.Vector2fVector(np_points)
    # From Cupoch to numpy
    np_points = np.asarray(pcd.points.cpu())
)";
            }),
            py::none(), py::none(), "");

    auto vector3ivector = pybind_eigen_vector_of_vector<Eigen::Vector3i>(
            m, "Vector3iVector", "utility::device_vector<Eigen::Vector3i>",
            py::py_array_to_vectors_int<Eigen::Vector3i>);
    vector3ivector.attr("__doc__") = static_property(
            py::cpp_function([](py::handle arg) -> std::string {
                return R"(Convert int32 numpy array of shape ``(n, 3)`` to Cupoch format..
Example usage
.. code-block:: python
    import cupoch
    import numpy as np
    # Example mesh
    # x, y coordinates:
    # [0: (-1, 2)]__________[1: (1, 2)]
    #             \        /\
    #              \  (0) /  \
    #               \    / (1)\
    #                \  /      \
    #      [2: (0, 0)]\/________\[3: (2, 0)]
    #
    # z coordinate: 0
    mesh = cupoch.geometry.TriangleMesh()
    np_vertices = np.array([[-1, 2, 0],
                            [1, 2, 0],
                            [0, 0, 0],
                            [2, 0, 0]])
    np_triangles = np.array([[0, 2, 1],
                             [1, 2, 3]]).astype(np.int32)
    mesh.vertices = cupoch.Vector3fVector(np_vertices)
    # From numpy to Cupoch
    mesh.triangles = cupoch.Vector3iVector(np_triangles)
    # From Cupoch to numpy
    np_triangles = np.asarray(mesh.triangles.cpu())
)";
            }),
            py::none(), py::none(), "");

    auto vector2ivector = pybind_eigen_vector_of_vector<Eigen::Vector2i>(
            m, "Vector2iVector", "utility::device_vector<Eigen::Vector2i>",
            py::py_array_to_vectors_int<Eigen::Vector2i>);
    vector2ivector.attr("__doc__") = static_property(
            py::cpp_function([](py::handle arg) -> std::string {
                return "Convert int32 numpy array of shape ``(n, 2)`` to "
                       "Cupoch format.";
            }),
            py::none(), py::none(), "");
}