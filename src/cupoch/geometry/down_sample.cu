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
#include <thrust/gather.h>
#include <thrust/iterator/discard_iterator.h>
#include <thrust/set_operations.h>
#include <thrust/sort.h>

#include "cupoch/geometry/kdtree_flann.h"
#include "cupoch/geometry/pointcloud.h"
#include "cupoch/utility/console.h"
#include "cupoch/utility/helper.h"
#include "cupoch/utility/platform.h"
#include "cupoch/utility/range.h"

using namespace cupoch;
using namespace cupoch::geometry;

namespace {

void SelectByIndexImpl(const geometry::PointCloud &src,
                       geometry::PointCloud &dst,
                       const utility::device_vector<size_t> &indices) {
    const bool has_normals = src.HasNormals();
    const bool has_colors = src.HasColors();
    if (has_normals) dst.normals_.resize(indices.size());
    if (has_colors) dst.colors_.resize(indices.size());
    dst.points_.resize(indices.size());
    thrust::gather(utility::exec_policy(utility::GetStream(0))
                           ->on(utility::GetStream(0)),
                   indices.begin(), indices.end(), src.points_.begin(),
                   dst.points_.begin());
    if (has_normals) {
        thrust::gather(utility::exec_policy(utility::GetStream(1))
                               ->on(utility::GetStream(1)),
                       indices.begin(), indices.end(), src.normals_.begin(),
                       dst.normals_.begin());
    }
    if (has_colors) {
        thrust::gather(utility::exec_policy(utility::GetStream(2))
                               ->on(utility::GetStream(2)),
                       indices.begin(), indices.end(), src.colors_.begin(),
                       dst.colors_.begin());
    }
    cudaSafeCall(cudaDeviceSynchronize());
}

struct compute_key_functor {
    compute_key_functor(const Eigen::Vector3f &voxel_min_bound,
                        float voxel_size)
        : voxel_min_bound_(voxel_min_bound), voxel_size_(voxel_size){};
    const Eigen::Vector3f voxel_min_bound_;
    const float voxel_size_;
    __device__ Eigen::Vector3i operator()(const Eigen::Vector3f &pt) {
        auto ref_coord = (pt - voxel_min_bound_) / voxel_size_;
        return Eigen::device_vectorize<float, 3, ::floor>(ref_coord)
                .cast<int>();
    }
};

template <int Index, class... Args>
struct normalize_and_devide_tuple_functor
    : public thrust::binary_function<const thrust::tuple<Args...>,
                                     const int,
                                     thrust::tuple<Args...>> {
    __host__ __device__ thrust::tuple<Args...> operator()(
            const thrust::tuple<Args...> &x, const int &y) const {
        thrust::tuple<Args...> ans = x;
        devide_tuple_impl(ans, y,
                          thrust::make_index_sequence<sizeof...(Args)>{});
        thrust::get<Index>(ans).normalize();
        return ans;
    }
};

struct check_distance_threshold_functor {
    check_distance_threshold_functor(float distance_threshold)
        : distance_threshold_(distance_threshold){};
    const float distance_threshold_;
    __device__ bool operator()(thrust::tuple<int, float> x) const {
        const float dist = thrust::get<1>(x);
        return (dist > 0 && dist < distance_threshold_);
    }
};

}  // namespace

std::shared_ptr<PointCloud> PointCloud::SelectByIndex(
        const utility::device_vector<size_t> &indices, bool invert) const {
    auto output = std::make_shared<PointCloud>();

    if (invert) {
        size_t n_out = points_.size() - indices.size();
        utility::device_vector<size_t> sorted_indices = indices;
        thrust::sort(utility::exec_policy(0)->on(0), sorted_indices.begin(),
                     sorted_indices.end());
        utility::device_vector<size_t> inv_indices(n_out);
        thrust::set_difference(thrust::make_counting_iterator<size_t>(0),
                               thrust::make_counting_iterator(points_.size()),
                               sorted_indices.begin(), sorted_indices.end(),
                               inv_indices.begin());
        SelectByIndexImpl(*this, *output, inv_indices);
    } else {
        SelectByIndexImpl(*this, *output, indices);
    }
    return output;
}

std::shared_ptr<PointCloud> PointCloud::VoxelDownSample(
        float voxel_size) const {
    auto output = std::make_shared<PointCloud>();
    if (voxel_size <= 0.0) {
        utility::LogWarning("[VoxelDownSample] voxel_size <= 0.\n");
        return output;
    }

    const Eigen::Vector3f voxel_size3 =
            Eigen::Vector3f(voxel_size, voxel_size, voxel_size);
    const Eigen::Vector3f voxel_min_bound = GetMinBound() - voxel_size3 * 0.5;
    const Eigen::Vector3f voxel_max_bound = GetMaxBound() + voxel_size3 * 0.5;

    if (voxel_size * std::numeric_limits<int>::max() <
        (voxel_max_bound - voxel_min_bound).maxCoeff()) {
        utility::LogWarning("[VoxelDownSample] voxel_size is too small.\n");
        return output;
    }

    const int n = points_.size();
    const bool has_normals = HasNormals();
    const bool has_colors = HasColors();
    compute_key_functor ck_func(voxel_min_bound, voxel_size);
    utility::device_vector<Eigen::Vector3i> keys(n);
    thrust::transform(points_.begin(), points_.end(), keys.begin(), ck_func);

    utility::device_vector<Eigen::Vector3f> sorted_points = points_;
    output->points_.resize(n);
    utility::device_vector<int> counts(n);
    thrust::equal_to<Eigen::Vector3i> binary_pred;
    if (!has_normals && !has_colors) {
        thrust::sort_by_key(utility::exec_policy(0)->on(0), keys.begin(),
                            keys.end(), make_tuple_begin(sorted_points));
        add_tuple_functor<Eigen::Vector3f, int> add_func;
        auto begin = make_tuple_begin(output->points_, counts);
        auto end = thrust::reduce_by_key(
                utility::exec_policy(0)->on(0), keys.begin(), keys.end(),
                make_tuple_iterator(sorted_points.begin(),
                                    thrust::make_constant_iterator(1)),
                thrust::make_discard_iterator(), begin, binary_pred, add_func);
        int n_out = thrust::distance(begin, end.second);
        devide_tuple_functor<Eigen::Vector3f> dv_func;
        auto output_begins = make_tuple_begin(output->points_);
        thrust::transform(output_begins, output_begins + n_out, counts.begin(),
                          output_begins, dv_func);
        output->points_.resize(n_out);
    } else if (has_normals && !has_colors) {
        utility::device_vector<Eigen::Vector3f> sorted_normals = normals_;
        output->normals_.resize(n);
        thrust::sort_by_key(utility::exec_policy(0)->on(0), keys.begin(),
                            keys.end(),
                            make_tuple_begin(sorted_points, sorted_normals));
        add_tuple_functor<Eigen::Vector3f, Eigen::Vector3f, int> add_func;
        auto begin =
                make_tuple_begin(output->points_, output->normals_, counts);
        auto end = thrust::reduce_by_key(
                utility::exec_policy(0)->on(0), keys.begin(), keys.end(),
                make_tuple_iterator(sorted_points.begin(),
                                    sorted_normals.begin(),
                                    thrust::make_constant_iterator(1)),
                thrust::make_discard_iterator(), begin, binary_pred, add_func);
        int n_out = thrust::distance(begin, end.second);
        normalize_and_devide_tuple_functor<1, Eigen::Vector3f, Eigen::Vector3f>
                dv_func;
        auto output_begins =
                make_tuple_begin(output->points_, output->normals_);
        thrust::transform(output_begins, output_begins + n_out, counts.begin(),
                          output_begins, dv_func);
        resize_all(n_out, output->points_, output->normals_);
    } else if (!has_normals && has_colors) {
        utility::device_vector<Eigen::Vector3f> sorted_colors = colors_;
        resize_all(n, output->colors_);
        thrust::sort_by_key(utility::exec_policy(0)->on(0), keys.begin(),
                            keys.end(),
                            make_tuple_begin(sorted_points, sorted_colors));
        add_tuple_functor<Eigen::Vector3f, Eigen::Vector3f, int> add_func;
        auto begin = make_tuple_begin(output->points_, output->colors_, counts);
        auto end = thrust::reduce_by_key(
                utility::exec_policy(0)->on(0), keys.begin(), keys.end(),
                make_tuple_iterator(sorted_points.begin(),
                                    sorted_colors.begin(),
                                    thrust::make_constant_iterator(1)),
                thrust::make_discard_iterator(), begin, binary_pred, add_func);
        int n_out = thrust::distance(begin, end.second);
        devide_tuple_functor<Eigen::Vector3f, Eigen::Vector3f> dv_func;
        auto output_begins = make_tuple_begin(output->points_, output->colors_);
        thrust::transform(output_begins, output_begins + n_out, counts.begin(),
                          output_begins, dv_func);
        resize_all(n_out, output->points_, output->colors_);
    } else {
        utility::device_vector<Eigen::Vector3f> sorted_normals = normals_;
        utility::device_vector<Eigen::Vector3f> sorted_colors = colors_;
        resize_all(n, output->normals_, output->colors_);
        thrust::sort_by_key(
                utility::exec_policy(0)->on(0), keys.begin(), keys.end(),
                make_tuple_begin(sorted_points, sorted_normals, sorted_colors));
        add_tuple_functor<Eigen::Vector3f, Eigen::Vector3f, Eigen::Vector3f,
                          int>
                add_func;
        auto begin = make_tuple_begin(output->points_, output->normals_,
                                      output->colors_, counts);
        auto end = thrust::reduce_by_key(
                utility::exec_policy(0)->on(0), keys.begin(), keys.end(),
                make_tuple_iterator(sorted_points.begin(),
                                    sorted_normals.begin(),
                                    sorted_colors.begin(),
                                    thrust::make_constant_iterator(1)),
                thrust::make_discard_iterator(), begin, binary_pred, add_func);
        int n_out = thrust::distance(begin, end.second);
        normalize_and_devide_tuple_functor<1, Eigen::Vector3f, Eigen::Vector3f,
                                           Eigen::Vector3f>
                dv_func;
        auto output_begins = make_tuple_begin(output->points_, output->normals_,
                                              output->colors_);
        thrust::transform(output_begins, output_begins + n_out, counts.begin(),
                          output_begins, dv_func);
        resize_all(n_out, output->points_, output->normals_, output->colors_);
    }

    utility::LogDebug(
            "Pointcloud down sampled from {:d} points to {:d} points.\n",
            (int)points_.size(), (int)output->points_.size());
    return output;
}

std::shared_ptr<PointCloud> PointCloud::UniformDownSample(
        size_t every_k_points) const {
    const bool has_normals = HasNormals();
    const bool has_colors = HasColors();
    auto output = std::make_shared<PointCloud>();
    if (every_k_points == 0) {
        utility::LogError("[UniformDownSample] Illegal sample rate.");
        return output;
    }
    const int n_out = points_.size() / every_k_points;
    output->points_.resize(n_out);
    if (has_normals) output->normals_.resize(n_out);
    if (has_colors) output->colors_.resize(n_out);
    thrust::strided_range<
            utility::device_vector<Eigen::Vector3f>::const_iterator>
            range_points(points_.begin(), points_.end(), every_k_points);
    thrust::copy(utility::exec_policy(utility::GetStream(0))
                         ->on(utility::GetStream(0)),
                 range_points.begin(), range_points.end(),
                 output->points_.begin());
    if (has_normals) {
        thrust::strided_range<
                utility::device_vector<Eigen::Vector3f>::const_iterator>
                range_normals(normals_.begin(), normals_.end(), every_k_points);
        thrust::copy(utility::exec_policy(utility::GetStream(1))
                             ->on(utility::GetStream(1)),
                     range_normals.begin(), range_normals.end(),
                     output->normals_.begin());
    }
    if (has_colors) {
        thrust::strided_range<
                utility::device_vector<Eigen::Vector3f>::const_iterator>
                range_colors(colors_.begin(), colors_.end(), every_k_points);
        thrust::copy(utility::exec_policy(utility::GetStream(2))
                             ->on(utility::GetStream(2)),
                     range_colors.begin(), range_colors.end(),
                     output->colors_.begin());
    }
    cudaSafeCall(cudaDeviceSynchronize());
    return output;
}

std::tuple<std::shared_ptr<PointCloud>, utility::device_vector<size_t>>
PointCloud::RemoveRadiusOutliers(size_t nb_points, float search_radius) const {
    if (nb_points < 1 || search_radius <= 0) {
        utility::LogError(
                "[RemoveRadiusOutliers] Illegal input parameters,"
                "number of points and radius must be positive");
    }
    KDTreeFlann kdtree;
    kdtree.SetGeometry(*this);
    utility::device_vector<int> tmp_indices;
    utility::device_vector<float> dist;
    kdtree.SearchRadius(points_, search_radius, nb_points + 1, tmp_indices,
                        dist);
    const size_t n_pt = points_.size();
    utility::device_vector<size_t> counts(n_pt);
    utility::device_vector<size_t> indices(n_pt);
    thrust::repeated_range<thrust::counting_iterator<size_t>> range(
            thrust::make_counting_iterator<size_t>(0),
            thrust::make_counting_iterator(n_pt), nb_points + 1);
    thrust::reduce_by_key(
            utility::exec_policy(0)->on(0), range.begin(), range.end(),
            thrust::make_transform_iterator(
                    tmp_indices.begin(),
                    [] __device__(int idx) { return (int)(idx >= 0); }),
            thrust::make_discard_iterator(), counts.begin(),
            thrust::equal_to<size_t>(), thrust::plus<size_t>());
    auto begin = make_tuple_iterator(indices.begin(),
                                     thrust::make_discard_iterator());
    auto end = thrust::copy_if(
            enumerate_begin(counts), enumerate_end(counts), begin,
            [nb_points] __device__(const thrust::tuple<size_t, size_t> &x) {
                return thrust::get<1>(x) > nb_points;
            });
    indices.resize(thrust::distance(begin, end));
    return std::make_tuple(SelectByIndex(indices), indices);
}

std::tuple<std::shared_ptr<PointCloud>, utility::device_vector<size_t>>
PointCloud::RemoveStatisticalOutliers(size_t nb_neighbors,
                                      float std_ratio) const {
    if (nb_neighbors < 1 || std_ratio <= 0) {
        utility::LogError(
                "[RemoveStatisticalOutliers] Illegal input parameters, number "
                "of neighbors and standard deviation ratio must be positive");
    }
    if (points_.empty()) {
        return std::make_tuple(std::make_shared<PointCloud>(),
                               utility::device_vector<size_t>());
    }
    KDTreeFlann kdtree;
    kdtree.SetGeometry(*this);
    const size_t n_pt = points_.size();
    utility::device_vector<float> avg_distances(n_pt);
    utility::device_vector<size_t> indices(n_pt);
    utility::device_vector<size_t> counts(n_pt);
    utility::device_vector<int> tmp_indices;
    utility::device_vector<float> dist;
    kdtree.SearchKNN(points_, int(nb_neighbors), tmp_indices, dist);
    thrust::repeated_range<thrust::counting_iterator<size_t>> range(
            thrust::make_counting_iterator<size_t>(0),
            thrust::make_counting_iterator(n_pt), nb_neighbors);
    thrust::reduce_by_key(
            utility::exec_policy(0)->on(0), range.begin(), range.end(),
            make_tuple_iterator(thrust::make_constant_iterator<size_t>(1),
                                dist.begin()),
            thrust::make_discard_iterator(),
            make_tuple_iterator(counts.begin(), avg_distances.begin()),
            thrust::equal_to<size_t>(),
            [] __device__(const thrust::tuple<size_t, float> &rhs,
                          const thrust::tuple<size_t, float> &lhs) {
                float rd = thrust::get<1>(rhs);
                size_t rc = thrust::get<0>(rhs);
                if (isinf(rd) || rd < 0.0) {
                    rd = 0.0;
                    rc = 0;
                }
                float ld = thrust::get<1>(lhs);
                size_t lc = thrust::get<0>(lhs);
                if (isinf(ld) || ld < 0.0) {
                    ld = 0.0;
                    lc = 0;
                }
                return thrust::make_tuple(rc + lc, rd + ld);
            });
    thrust::transform(avg_distances.begin(), avg_distances.end(),
                      counts.begin(), avg_distances.begin(),
                      [] __device__(float avg, size_t cnt) {
                          return (cnt > 0) ? avg / (float)cnt : -1.0;
                      });
    auto mean_and_count = thrust::transform_reduce(
            utility::exec_policy(0)->on(0), avg_distances.begin(),
            avg_distances.end(),
            [] __device__(float const &x) {
                return thrust::make_tuple(max(x, 0.0f), (size_t)(x >= 0.0));
            },
            thrust::make_tuple(0.0f, size_t(0)),
            add_tuple_functor<float, size_t>());
    const size_t valid_distances = thrust::get<1>(mean_and_count);
    if (valid_distances == 0) {
        return std::make_tuple(std::make_shared<PointCloud>(),
                               utility::device_vector<size_t>());
    }
    float cloud_mean = thrust::get<0>(mean_and_count);
    cloud_mean /= valid_distances;
    const float sq_sum = thrust::transform_reduce(
            utility::exec_policy(0)->on(0), avg_distances.begin(),
            avg_distances.end(),
            [cloud_mean] __device__(const float x) {
                return (x > 0) ? (x - cloud_mean) * (x - cloud_mean) : 0;
            },
            0.0, thrust::plus<float>());
    // Bessel's correction
    const float std_dev = std::sqrt(sq_sum / (valid_distances - 1));
    const float distance_threshold = cloud_mean + std_ratio * std_dev;
    check_distance_threshold_functor th_func(distance_threshold);
    auto begin = make_tuple_iterator(indices.begin(),
                                     thrust::make_discard_iterator());
    auto end = thrust::copy_if(enumerate_begin(avg_distances),
                               enumerate_end(avg_distances), begin, th_func);
    indices.resize(thrust::distance(begin, end));
    return std::make_tuple(SelectByIndex(indices), indices);
}
