import open3d as o3d
import cupoch as cph
import numpy as np
import time

if __name__ == "__main__":
    source_cpu = o3d.io.read_point_cloud("../../testdata/icp/cloud_bin_0.pcd")
    target_cpu = o3d.io.read_point_cloud("../../testdata/icp/cloud_bin_1.pcd")
    source_gpu = cph.io.read_point_cloud("../../testdata/icp/cloud_bin_0.pcd")
    target_gpu = cph.io.read_point_cloud("../../testdata/icp/cloud_bin_1.pcd")
    threshold = 0.02
    target_cpu.estimate_normals()
    target_gpu.estimate_normals()
    trans_init = np.asarray([[0.862, 0.011, -0.507, 0.5],
                             [-0.139, 0.967, -0.215, 0.7],
                             [0.487, 0.255, 0.835, -1.4],
                             [0.0, 0.0, 0.0, 1.0]])
    start = time.time()
    reg_p2p = o3d.pipelines.registration.registration_icp(
        source_cpu, target_cpu, threshold, trans_init,
        o3d.pipelines.registration.TransformationEstimationPointToPlane())
    elapsed_time = time.time() - start
    print(reg_p2p.transformation)
    print("ICP (CPU) [sec]:", elapsed_time)
    source_cpu.transform(reg_p2p.transformation)
    o3d.visualization.draw_geometries([source_cpu, target_cpu])

    start = time.time()
    reg_p2p = cph.registration.registration_icp(
        source_gpu, target_gpu, threshold, trans_init.astype(np.float32),
        cph.registration.TransformationEstimationPointToPlane())
    elapsed_time = time.time() - start
    print(reg_p2p.transformation)
    print("ICP (GPU) [sec]:", elapsed_time)
    source_gpu.transform(reg_p2p.transformation)
    cph.visualization.draw_geometries([source_gpu, target_gpu])

