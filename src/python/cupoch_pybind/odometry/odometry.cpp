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
#include "cupoch/odometry/odometry.h"

#include "cupoch/geometry/image.h"
#include "cupoch/geometry/rgbdimage.h"
#include "cupoch/odometry/odometry_option.h"
#include "cupoch/odometry/rgbdodometry_jacobian.h"
#include "cupoch_pybind/docstring.h"
#include "cupoch_pybind/odometry/odometry.h"

using namespace cupoch;

template <class RGBDOdometryJacobianBase = odometry::RGBDOdometryJacobian>
class PyRGBDOdometryJacobian : public RGBDOdometryJacobianBase {
public:
    using RGBDOdometryJacobianBase::RGBDOdometryJacobianBase;
};

void pybind_odometry_classes(py::module &m) {
    // cupoch.odometry.OdometryOption
    py::class_<odometry::OdometryOption> odometry_option(
            m, "OdometryOption", "Class that defines Odometry options.");
    odometry_option
            .def(py::init(
                         [](std::vector<int> iteration_number_per_pyramid_level,
                            float max_depth_diff, float min_depth,
                            float max_depth) {
                             return new odometry::OdometryOption(
                                     iteration_number_per_pyramid_level,
                                     max_depth_diff, min_depth, max_depth);
                         }),
                 "iteration_number_per_pyramid_level"_a =
                         std::vector<int>{20, 10, 5},
                 "max_depth_diff"_a = 0.03, "min_depth"_a = 0.0,
                 "max_depth"_a = 4.0)
            .def_readwrite("iteration_number_per_pyramid_level",
                           &odometry::OdometryOption::
                                   iteration_number_per_pyramid_level_,
                           "List(int): Iteration number per image pyramid "
                           "level, typically larger image in the pyramid have "
                           "lower interation number to reduce computation "
                           "time.")
            .def_readwrite("max_depth_diff",
                           &odometry::OdometryOption::max_depth_diff_,
                           "Maximum depth difference to be considered as "
                           "correspondence. In depth image domain, if two "
                           "aligned pixels have a depth difference less than "
                           "specified value, they are considered as a "
                           "correspondence. Larger value induce more "
                           "aggressive search, but it is prone to unstable "
                           "result.")
            .def_readwrite("min_depth", &odometry::OdometryOption::min_depth_,
                           "Pixels that has smaller than specified depth "
                           "values are ignored.")
            .def_readwrite("max_depth", &odometry::OdometryOption::max_depth_,
                           "Pixels that has larger than specified depth values "
                           "are ignored.")
            .def_readwrite(
                    "nu", &odometry::OdometryOption::nu_,
                    "Degree of freedom for computing weighted RGBD odometry.")
            .def_readwrite(
                    "sigma2_init", &odometry::OdometryOption::sigma2_init_,
                    "Initial variance for computing weighted RGBD odometry.")
            .def_readwrite(
                    "inv_sigma_mat_diag",
                    &odometry::OdometryOption::inv_sigma_mat_diag_,
                    "Covariance matrix for the influence of the motion prior.")
            .def("__repr__", [](const odometry::OdometryOption &c) {
                int num_pyramid_level =
                        (int)c.iteration_number_per_pyramid_level_.size();
                std::string str_iteration_number_per_pyramid_level_ = "[ ";
                for (int i = 0; i < num_pyramid_level; i++)
                    str_iteration_number_per_pyramid_level_ +=
                            std::to_string(
                                    c.iteration_number_per_pyramid_level_[i]) +
                            ", ";
                str_iteration_number_per_pyramid_level_ += "] ";
                std::string str_inv_sigma_mat_diag_ = "[ ";
                for (int i = 0; i < 6; i++)
                    str_inv_sigma_mat_diag_ +=
                            std::to_string(c.inv_sigma_mat_diag_[i]) + ", ";
                str_inv_sigma_mat_diag_ += "] ";
                return std::string("odometry::OdometryOption class.") +
                       /*std::string("\nodo_init = ") +
                          std::to_string(c.odo_init_) +*/
                       std::string("\niteration_number_per_pyramid_level = ") +
                       str_iteration_number_per_pyramid_level_ +
                       std::string("\nmax_depth_diff = ") +
                       std::to_string(c.max_depth_diff_) +
                       std::string("\nmin_depth = ") +
                       std::to_string(c.min_depth_) +
                       std::string("\nmax_depth = ") +
                       std::to_string(c.max_depth_) + std::string("\nnu = ") +
                       std::to_string(c.nu_) + std::string("\nsigma2_init = ") +
                       std::to_string(c.sigma2_init_) +
                       std::string("\ninv_sigma_mat_diag") +
                       str_inv_sigma_mat_diag_;
            });

    // cupoch.odometry.RGBDOdometryJacobian
    py::class_<odometry::RGBDOdometryJacobian,
               PyRGBDOdometryJacobian<odometry::RGBDOdometryJacobian>>
            jacobian(
                    m, "RGBDOdometryJacobian",
                    "Base class that computes Jacobian from two RGB-D images.");

    // cupoch.odometry.RGBDOdometryJacobianFromColorTerm: RGBDOdometryJacobian
    py::class_<
            odometry::RGBDOdometryJacobianFromColorTerm,
            PyRGBDOdometryJacobian<odometry::RGBDOdometryJacobianFromColorTerm>,
            odometry::RGBDOdometryJacobian>
            jacobian_color(m, "RGBDOdometryJacobianFromColorTerm",
                           R"(Class to Compute Jacobian using color term.
Energy: :math:`(I_p-I_q)^2.`
Reference:
F. Steinbrucker, J. Sturm, and D. Cremers.
Real-time visual odometry from dense RGB-D images.
In ICCV Workshops, 2011.)");
    py::detail::bind_default_constructor<
            odometry::RGBDOdometryJacobianFromColorTerm>(jacobian_color);
    py::detail::bind_copy_functions<
            odometry::RGBDOdometryJacobianFromColorTerm>(jacobian_color);
    jacobian_color.def(
            "__repr__",
            [](const odometry::RGBDOdometryJacobianFromColorTerm &te) {
                return std::string("RGBDOdometryJacobianFromColorTerm");
            });

    // cupoch.odometry.RGBDOdometryJacobianFromHybridTerm: RGBDOdometryJacobian
    py::class_<odometry::RGBDOdometryJacobianFromHybridTerm,
               PyRGBDOdometryJacobian<
                       odometry::RGBDOdometryJacobianFromHybridTerm>,
               odometry::RGBDOdometryJacobian>
            jacobian_hybrid(m, "RGBDOdometryJacobianFromHybridTerm",
                            R"(Class to compute Jacobian using hybrid term
Energy: :math:`(I_p-I_q)^2 + \lambda(D_p-D_q')^2`
Reference:
J. Park, Q.-Y. Zhou, and V. Koltun
Anonymous submission.)");
    py::detail::bind_default_constructor<
            odometry::RGBDOdometryJacobianFromHybridTerm>(jacobian_hybrid);
    py::detail::bind_copy_functions<
            odometry::RGBDOdometryJacobianFromHybridTerm>(jacobian_hybrid);
    jacobian_hybrid.def(
            "__repr__",
            [](const odometry::RGBDOdometryJacobianFromHybridTerm &te) {
                return std::string("RGBDOdometryJacobianFromHybridTerm");
            });
}

void pybind_odometry_methods(py::module &m) {
    m.def("compute_rgbd_odometry", &odometry::ComputeRGBDOdometry,
          "Function to estimate 6D rigid motion from two RGBD image pairs. "
          "Output: (is_success, 4x4 motion matrix, 6x6 information matrix).",
          "rgbd_source"_a, "rgbd_target"_a,
          "pinhole_camera_intrinsic"_a = camera::PinholeCameraIntrinsic(),
          "odo_init"_a = Eigen::Matrix4f::Identity(),
          "jacobian"_a = odometry::RGBDOdometryJacobianFromHybridTerm(),
          "option"_a = odometry::OdometryOption());
    m.def("compute_weighted_rgbd_odometry",
          &odometry::ComputeWeightedRGBDOdometry,
          "Function to estimate 6D rigid motion from two RGBD image pairs. "
          "Output: (is_success, 4x4 motion matrix, 6x6 information matrix).",
          "rgbd_source"_a, "rgbd_target"_a,
          "pinhole_camera_intrinsic"_a = camera::PinholeCameraIntrinsic(),
          "odo_init"_a = Eigen::Matrix4f::Identity(),
          "prev_twist"_a = Eigen::Vector6f::Zero(),
          "jacobian"_a = odometry::RGBDOdometryJacobianFromHybridTerm(),
          "option"_a = odometry::OdometryOption());
    docstring::FunctionDocInject(
            m, "compute_rgbd_odometry",
            {
                    {"rgbd_source", "Source RGBD image."},
                    {"rgbd_target", "Target RGBD image."},
                    {"pinhole_camera_intrinsic", "Camera intrinsic parameters"},
                    {"odo_init", "Initial 4x4 motion matrix estimation."},
                    {"jacobian",
                     "The odometry Jacobian method to use. Can be "
                     "``odometry::"
                     "RGBDOdometryJacobianFrComputeRGBDOdometryes."},
            });
}

void pybind_odometry(py::module &m) {
    py::module m_submodule = m.def_submodule("odometry");
    pybind_odometry_classes(m_submodule);
    pybind_odometry_methods(m_submodule);
}