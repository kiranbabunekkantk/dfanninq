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
#include "cupoch/geometry/boundingvolume.h"
#include "cupoch/geometry/graph.h"
#include "cupoch/geometry/trianglemesh.h"

namespace cupoch {
namespace geometry {

namespace {

const int NUM_OFFSET[2] = {8, 26};

__constant__ int voxel_offset[26][3] = {
        {1, 0, 0},  {-1, 0, 0},  {0, 1, 0},   {0, -1, 0},  {0, 0, 1},
        {0, 0, -1}, {1, 1, 0},   {1, -1, 0},  {-1, 1, 0},  {-1, -1, 0},
        {1, 0, 1},  {1, 0, -1},  {-1, 0, 1},  {-1, 0, -1}, {0, 1, 1},
        {0, 1, -1}, {0, -1, 1},  {0, -1, -1}, {1, 1, 1},   {-1, -1, -1},
        {-1, 1, 1}, {1, -1, -1}, {1, -1, 1},  {-1, 1, -1}, {-1, -1, 1},
        {1, 1, -1}};

__constant__ int voxel_offset_2d[8][2] = {
        {1, 0},  {-1, 0},  {0, 1},   {0, -1},
        {1, 1},  {1, -1},  {-1, 1},  {-1, -1}};

template <int Dim>
struct create_dense_grid_points_functor {
    create_dense_grid_points_functor(
            const Eigen::Matrix<int, Dim, 1>& resolutions,
            const Eigen::Matrix<float, Dim, 1>& min_bound,
            const Eigen::Matrix<float, Dim, 1>& steps)
        : resolutions_(resolutions), min_bound_(min_bound), steps_(steps){};
    const Eigen::Matrix<int, Dim, 1> resolutions_;
    const Eigen::Matrix<float, Dim, 1> min_bound_;
    const Eigen::Matrix<float, Dim, 1> steps_;
    __device__ Eigen::Matrix<float, Dim, 1> operator()(size_t idx) const;
};

template <>
__device__ Eigen::Vector3f create_dense_grid_points_functor<3>::operator()(
        size_t idx) const {
    int x = idx / (resolutions_[1] * resolutions_[2]);
    int yz = idx % (resolutions_[1] * resolutions_[2]);
    int y = yz / resolutions_[2];
    int z = yz % resolutions_[2];
    return min_bound_ +
           (Eigen::Vector3i(x, y, z).cast<float>().array() * steps_.array())
                   .matrix();
}

template <>
__device__ Eigen::Vector2f create_dense_grid_points_functor<2>::operator()(
        size_t idx) const {
    int x = idx / resolutions_[1];
    int y = idx % resolutions_[1];
    return min_bound_ +
           (Eigen::Vector2i(x, y).cast<float>().array() * steps_.array())
                   .matrix();
}

template <int Dim>
struct create_dense_grid_lines_functor {
    create_dense_grid_lines_functor(const Eigen::Matrix<int, Dim, 1>& resolutions)
        : resolutions_(resolutions){};
    const Eigen::Matrix<int, Dim, 1> resolutions_;
    __device__ Eigen::Vector2i operator()(size_t idx) const;
};

template <>
__device__
Eigen::Vector2i create_dense_grid_lines_functor<3>::operator()(size_t idx) const {
    int x = idx / (resolutions_[1] * resolutions_[2] * 26);
    int yzk = idx % (resolutions_[1] * resolutions_[2] * 26);
    int y = yzk / (resolutions_[2] * 26);
    int zk = yzk % (resolutions_[2] * 26);
    int z = zk / 26;
    int k = zk % 26;
    Eigen::Vector3i sidx = Eigen::Vector3i(x, y, z);
    Eigen::Vector3i gidx =
            Eigen::Vector3i(x + voxel_offset[k][0], y + voxel_offset[k][1],
                            z + voxel_offset[k][2]);
    if (gidx[0] < 0 || gidx[0] >= resolutions_[0] || gidx[1] < 0 ||
        gidx[1] >= resolutions_[1] || gidx[2] < 0 ||
        gidx[2] >= resolutions_[2])
        return Eigen::Vector2i(-1, -1);
    int j = gidx[0] * resolutions_[1] * resolutions_[2] +
            gidx[1] * resolutions_[2] + gidx[2];
    int i = sidx[0] * resolutions_[1] * resolutions_[2] +
            sidx[1] * resolutions_[2] + sidx[2];
    return Eigen::Vector2i(i, j);
}

template <>
__device__
Eigen::Vector2i create_dense_grid_lines_functor<2>::operator()(size_t idx) const {
    int x = idx / (resolutions_[1] * 8);
    int yk = idx % (resolutions_[1] * 8);
    int y = yk / 8;
    int k = yk % 8;
    Eigen::Vector2i sidx = Eigen::Vector2i(x, y);
    Eigen::Vector2i gidx =
            Eigen::Vector2i(x + voxel_offset_2d[k][0], y + voxel_offset_2d[k][1]);
    if (gidx[0] < 0 || gidx[0] >= resolutions_[0] || gidx[1] < 0 ||
        gidx[1] >= resolutions_[1])
        return Eigen::Vector2i(-1, -1);
    int j = gidx[0] * resolutions_[1] + gidx[1];
    int i = sidx[0] * resolutions_[1] + sidx[1];
    return Eigen::Vector2i(i, j);
}

}  // namespace

template <>
std::shared_ptr<Graph<3>> Graph<3>::CreateFromTriangleMesh(
        const TriangleMesh& input) {
    auto out = std::make_shared<Graph<3>>();
    out->points_ = input.vertices_;
    if (input.HasEdgeList()) {
        out->lines_ = input.edge_list_;
    } else {
        TriangleMesh tmp;
        tmp.triangles_ = input.triangles_;
        tmp.ComputeEdgeList();
        out->lines_ = tmp.edge_list_;
    }
    out->ConstructGraph();
    return out;
}

template <>
std::shared_ptr<Graph<2>> Graph<2>::CreateFromTriangleMesh(
        const TriangleMesh& input) {
    utility::LogError("Graph<2>::CreateFromTriangleMesh is not supported");
    return std::make_shared<Graph<2>>();
}

template <>
std::shared_ptr<Graph<3>> Graph<3>::CreateFromAxisAlignedBoundingBox(
        const geometry::AxisAlignedBoundingBox& bbox,
        const Eigen::Vector3i& resolutions) {
    return Graph<3>::CreateFromAxisAlignedBoundingBox(
            bbox.min_bound_, bbox.max_bound_, resolutions);
}

template <>
std::shared_ptr<Graph<2>> Graph<2>::CreateFromAxisAlignedBoundingBox(
        const geometry::AxisAlignedBoundingBox& bbox,
        const Eigen::Vector3i& resolutions) {
    utility::LogError(
            "Graph<2>::CreateFromAxisAlignedBoundingBox is not supported");
    return std::make_shared<Graph<2>>();
}

template <int Dim>
std::shared_ptr<Graph<Dim>> Graph<Dim>::CreateFromAxisAlignedBoundingBox(
        const Eigen::Matrix<float, Dim, 1>& min_bound,
        const Eigen::Matrix<float, Dim, 1>& max_bound,
        const Eigen::Matrix<int, Dim, 1>& resolutions) {
    auto out = std::make_shared<Graph<Dim>>();
    Eigen::Matrix<float, Dim, 1> steps =
            (max_bound - min_bound).array() /
            resolutions.template cast<float>().array();
    size_t n_points = resolutions.prod();
    out->points_.resize(n_points);
    create_dense_grid_points_functor<Dim> pfunc(resolutions, min_bound, steps);
    thrust::transform(thrust::make_counting_iterator<size_t>(0),
                      thrust::make_counting_iterator(n_points),
                      out->points_.begin(), pfunc);
    out->lines_.resize(n_points * NUM_OFFSET[Dim - 2]);
    create_dense_grid_lines_functor<Dim> lfunc(resolutions);
    thrust::transform(thrust::make_counting_iterator<size_t>(0),
                      thrust::make_counting_iterator(n_points * NUM_OFFSET[Dim - 2]),
                      out->lines_.begin(), lfunc);
    auto end = thrust::remove_if(
            out->lines_.begin(), out->lines_.end(),
            [] __device__(const Eigen::Vector2i& l) { return l[0] < 0; });
    out->lines_.resize(thrust::distance(out->lines_.begin(), end));
    out->ConstructGraph();
    return out;
}

template std::shared_ptr<Graph<3>> Graph<3>::CreateFromAxisAlignedBoundingBox(
    const Eigen::Vector3f& min_bound,
    const Eigen::Vector3f& max_bound,
    const Eigen::Vector3i& resolutions);

template std::shared_ptr<Graph<2>> Graph<2>::CreateFromAxisAlignedBoundingBox(
    const Eigen::Vector2f& min_bound,
    const Eigen::Vector2f& max_bound,
    const Eigen::Vector2i& resolutions);

}  // namespace geometry
}  // namespace cupoch